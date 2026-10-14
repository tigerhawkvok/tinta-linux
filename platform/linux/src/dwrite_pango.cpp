// DirectWrite on Pango.
//
// Layout model: Pango shapes, wraps and ellipsizes; this file positions the
// lines itself, so DirectWrite's paragraph alignment (vertical placement in
// the max height), uniform line spacing (fixed pitch and baseline) and
// alignment of unwrapped text behave as on Windows, and hit testing, metrics
// and drawing all read the same line boxes. Text positions are wchar_t
// indices (code points); Pango works in UTF-8 bytes.
#include "compat_internal.h"

#include <pango/pangocairo.h>
#include <pango/pangofc-fontmap.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>

namespace tinta {

// ---- Text ----------------------------------------------------------------------
std::string toUtf8(const wchar_t* text, size_t length) {
    std::string out;
    out.reserve(length);
    for (size_t i = 0; i < length; i++) {
        auto c = static_cast<char32_t>(text[i]);
        if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF) || c == 0) c = 0xFFFD;
        char buffer[4];
        int n = g_unichar_to_utf8(static_cast<gunichar>(c), buffer);
        out.append(buffer, n);
    }
    return out;
}

std::wstring toWide(const char* text, size_t length) {
    std::wstring out;
    out.reserve(length);
    const char* p = text;
    const char* end = text + length;
    while (p < end) {
        gunichar c = g_utf8_get_char_validated(p, end - p);
        if (c == static_cast<gunichar>(-1) || c == static_cast<gunichar>(-2)) {
            out.push_back(static_cast<wchar_t>(0xFFFD));
            p++;
            continue;
        }
        out.push_back(static_cast<wchar_t>(c));
        p = g_utf8_next_char(p);
    }
    return out;
}

std::string nativePath(const wchar_t* path) {
    if (!path) return {};
    std::string out = toUtf8(path, wcslen(path));
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

// ---- Fonts -------------------------------------------------------------------------
PangoContext* pangoContext() {
    static PangoContext* context = [] {
        PangoFontMap* map = pango_cairo_font_map_new();
        PangoContext* ctx = pango_font_map_create_context(map);
        g_object_unref(map);
        // 1 Pango point = 1 DIP: sizes are set absolute, so resolution only
        // matters for relative sizes; keep it at 96
        pango_cairo_context_set_resolution(ctx, 96.0);
        cairo_font_options_t* options = cairo_font_options_create();
        cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_OFF);
        cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_SLIGHT);
        cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
        pango_cairo_context_set_font_options(ctx, options);
        cairo_font_options_destroy(options);
        pango_context_set_round_glyph_positions(ctx, FALSE);
        return ctx;
    }();
    return context;
}

namespace {

std::string lower(std::string text) {
    for (char& c : text) c = static_cast<char>(g_ascii_tolower(c));
    return text;
}

struct Alias {
    const char* windowsName;
    const char* stack;  // appended after the original name
};

// Windows families Tinta names in themes and code, and what stands in for
// them on Linux. The original name stays first, so an installed copy wins.
const Alias kAliases[] = {
    {"segoe ui", "Inter,Noto Sans,Cantarell,Ubuntu,DejaVu Sans,sans-serif"},
    {"segoe ui light", "Inter Light,Noto Sans Light,Noto Sans,DejaVu Sans,sans-serif"},
    {"segoe ui semibold", "Inter,Noto Sans,DejaVu Sans,sans-serif"},
    {"segoe ui variable", "Inter,Noto Sans,Cantarell,DejaVu Sans,sans-serif"},
    {"segoe ui emoji", "Noto Color Emoji,Twemoji,emoji"},
    {"segoe ui symbol", "Noto Sans Symbols,Noto Sans Symbols2,DejaVu Sans,sans-serif"},
    {"consolas", "Cascadia Code,JetBrains Mono,Fira Code,DejaVu Sans Mono,Noto Sans Mono,monospace"},
    {"cascadia code", "Cascadia Mono,JetBrains Mono,Fira Code,DejaVu Sans Mono,Noto Sans Mono,monospace"},
    {"cascadia mono", "Cascadia Code,JetBrains Mono,DejaVu Sans Mono,Noto Sans Mono,monospace"},
    {"courier new", "Liberation Mono,DejaVu Sans Mono,monospace"},
    {"lucida console", "DejaVu Sans Mono,monospace"},
    {"cambria math", "STIX Two Math,Latin Modern Math,Noto Sans Math,DejaVu Math TeX Gyre,serif"},
    {"cambria", "Caladea,Noto Serif,DejaVu Serif,serif"},
    {"georgia", "Gelasio,Noto Serif,DejaVu Serif,serif"},
    {"times new roman", "Liberation Serif,Noto Serif,DejaVu Serif,serif"},
    {"arial", "Liberation Sans,Noto Sans,DejaVu Sans,sans-serif"},
    {"calibri", "Carlito,Noto Sans,DejaVu Sans,sans-serif"},
    {"yu gothic ui", "Noto Sans CJK JP,Noto Sans JP,sans-serif"},
    {"yu gothic", "Noto Sans CJK JP,Noto Sans JP,sans-serif"},
    {"meiryo", "Noto Sans CJK JP,sans-serif"},
    {"microsoft yahei ui", "Noto Sans CJK SC,Noto Sans SC,sans-serif"},
    {"microsoft yahei", "Noto Sans CJK SC,Noto Sans SC,sans-serif"},
    {"microsoft jhenghei ui", "Noto Sans CJK TC,Noto Sans TC,sans-serif"},
    {"malgun gothic", "Noto Sans CJK KR,Noto Sans KR,sans-serif"},
};

const Alias* findAlias(const std::string& lowerName) {
    for (const auto& alias : kAliases) {
        if (lowerName == alias.windowsName) return &alias;
    }
    return nullptr;
}

}  // namespace

std::string resolveFamily(const std::wstring& family) {
    std::string name = toUtf8(family);
    const Alias* alias = findAlias(lower(name));
    if (!alias) return name.empty() ? std::string("sans-serif") : name;
    return name + "," + alias->stack;
}

namespace {

PangoWeight pangoWeight(DWRITE_FONT_WEIGHT weight) { return static_cast<PangoWeight>(static_cast<int>(weight)); }

PangoStyle pangoStyle(DWRITE_FONT_STYLE style) {
    switch (style) {
        case DWRITE_FONT_STYLE_ITALIC: return PANGO_STYLE_ITALIC;
        case DWRITE_FONT_STYLE_OBLIQUE: return PANGO_STYLE_OBLIQUE;
        default: return PANGO_STYLE_NORMAL;
    }
}

PangoStretch pangoStretch(DWRITE_FONT_STRETCH stretch) {
    int s = static_cast<int>(stretch);
    if (s <= 0) s = 5;
    return static_cast<PangoStretch>(std::clamp(s - 1, 0, 8));
}

std::string featureString(const std::vector<DWRITE_FONT_FEATURE>& features) {
    std::string out;
    for (const auto& feature : features) {
        UINT32 tag = feature.nameTag;
        char name[5] = {static_cast<char>(tag & 0xff), static_cast<char>((tag >> 8) & 0xff),
                        static_cast<char>((tag >> 16) & 0xff), static_cast<char>((tag >> 24) & 0xff), 0};
        if (!out.empty()) out += ',';
        out += name;
        out += '=';
        out += std::to_string(feature.parameter);
    }
    return out;
}

// ---- Typography, inline objects, rendering params, fallback ----------------------
class Typography final : public TintaComObject<IDWriteTypography> {
public:
    using Primary = IDWriteTypography;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteTypography>(this, riid, object);
    }
    HRESULT AddFontFeature(DWRITE_FONT_FEATURE feature) override {
        features.push_back(feature);
        return S_OK;
    }
    UINT32 GetFontFeatureCount() override { return static_cast<UINT32>(features.size()); }
    HRESULT GetFontFeature(UINT32 index, DWRITE_FONT_FEATURE* feature) override {
        if (index >= features.size()) return E_INVALIDARG;
        *feature = features[index];
        return S_OK;
    }
    std::vector<DWRITE_FONT_FEATURE> features;
};

class EllipsisSign final : public TintaComObject<IDWriteInlineObject> {
public:
    using Primary = IDWriteInlineObject;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteInlineObject>(this, riid, object);
    }
};

class RenderingParams final : public TintaComObject<IDWriteRenderingParams> {
public:
    using Primary = IDWriteRenderingParams;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteRenderingParams>(this, riid, object);
    }
};

// fontconfig already falls back per character; mappings are accepted and the
// result is a marker object
class FontFallback final : public TintaComObject<IDWriteFontFallback> {
public:
    using Primary = IDWriteFontFallback;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteFontFallback>(this, riid, object);
    }
};

class FontFallbackBuilder final : public TintaComObject<IDWriteFontFallbackBuilder> {
public:
    using Primary = IDWriteFontFallbackBuilder;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteFontFallbackBuilder>(this, riid, object);
    }
    HRESULT AddMapping(const DWRITE_UNICODE_RANGE*, UINT32, const WCHAR**, UINT32, IDWriteFontCollection*,
                       const WCHAR*, const WCHAR*, FLOAT) override {
        return S_OK;
    }
    HRESULT AddMappings(IDWriteFontFallback*) override { return S_OK; }
    HRESULT CreateFontFallback(IDWriteFontFallback** fallback) override {
        *fallback = new FontFallback();
        return S_OK;
    }
};

// ---- Font collection -------------------------------------------------------------
class LocalizedStrings final : public TintaComObject<IDWriteLocalizedStrings> {
public:
    using Primary = IDWriteLocalizedStrings;
    explicit LocalizedStrings(std::wstring name) : name_(std::move(name)) {}
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteLocalizedStrings>(this, riid, object);
    }
    UINT32 GetCount() override { return 1; }
    HRESULT FindLocaleName(const WCHAR*, UINT32* index, BOOL* exists) override {
        *index = 0;
        *exists = TRUE;
        return S_OK;
    }
    HRESULT GetStringLength(UINT32 index, UINT32* length) override {
        if (index != 0) return E_INVALIDARG;
        *length = static_cast<UINT32>(name_.size());
        return S_OK;
    }
    HRESULT GetString(UINT32 index, WCHAR* buffer, UINT32 size) override {
        if (index != 0 || size <= name_.size()) return E_INVALIDARG;
        wmemcpy(buffer, name_.c_str(), name_.size() + 1);
        return S_OK;
    }

private:
    std::wstring name_;
};

class Font final : public TintaComObject<IDWriteFont> {
public:
    using Primary = IDWriteFont;
    Font(std::string family, DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STYLE style)
        : family_(std::move(family)), weight_(weight), style_(style) {}
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteFont>(this, riid, object);
    }
    DWRITE_FONT_WEIGHT GetWeight() override { return weight_; }
    DWRITE_FONT_STYLE GetStyle() override { return style_; }
    HRESULT HasCharacter(UINT32 codepoint, BOOL* exists) override {
        *exists = FALSE;
        PangoFontDescription* desc = pango_font_description_from_string(family_.c_str());
        pango_font_description_set_absolute_size(desc, 16 * PANGO_SCALE);
        PangoFont* font = pango_font_map_load_font(pango_context_get_font_map(pangoContext()), pangoContext(), desc);
        pango_font_description_free(desc);
        if (!font) return S_OK;
        // The exact family must cover it: a fallback face does not count
        PangoFontDescription* loaded = pango_font_describe(font);
        std::string loadedFamily = lower(pango_font_description_get_family(loaded) ? pango_font_description_get_family(loaded) : "");
        pango_font_description_free(loaded);
        if (loadedFamily == lower(family_)) *exists = pango_font_has_char(font, codepoint) ? TRUE : FALSE;
        g_object_unref(font);
        return S_OK;
    }

private:
    std::string family_;
    DWRITE_FONT_WEIGHT weight_;
    DWRITE_FONT_STYLE style_;
};

class FontFamily final : public TintaComObject<IDWriteFontFamily> {
public:
    using Primary = IDWriteFontFamily;
    explicit FontFamily(std::string name) : name_(std::move(name)) {}
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteFontFamily, IDWriteFontList>(this, riid, object);
    }
    HRESULT GetFamilyNames(IDWriteLocalizedStrings** names) override {
        *names = new LocalizedStrings(toWide(name_));
        return S_OK;
    }
    HRESULT GetFirstMatchingFont(DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STRETCH, DWRITE_FONT_STYLE style,
                                 IDWriteFont** font) override {
        *font = new Font(name_, weight, style);
        return S_OK;
    }

private:
    std::string name_;
};

// Installed Pango families, plus the Windows names kAliases stands in for
class FontCollection final : public TintaComObject<IDWriteFontCollection> {
public:
    using Primary = IDWriteFontCollection;
    FontCollection() {
        PangoFontFamily** list = nullptr;
        int count = 0;
        pango_font_map_list_families(pango_context_get_font_map(pangoContext()), &list, &count);
        for (int i = 0; i < count; i++) names_.push_back(pango_font_family_get_name(list[i]));
        g_free(list);
        std::sort(names_.begin(), names_.end());
    }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteFontCollection>(this, riid, object);
    }
    UINT32 GetFontFamilyCount() override { return static_cast<UINT32>(names_.size()); }
    HRESULT GetFontFamily(UINT32 index, IDWriteFontFamily** family) override {
        if (index >= names_.size()) return E_INVALIDARG;
        *family = new FontFamily(names_[index]);
        return S_OK;
    }
    HRESULT FindFamilyName(const WCHAR* name, UINT32* index, BOOL* exists) override {
        std::string wanted = lower(toUtf8(name, wcslen(name)));
        *index = 0;
        *exists = FALSE;
        for (size_t i = 0; i < names_.size(); i++) {
            if (lower(names_[i]) == wanted) {
                *index = static_cast<UINT32>(i);
                *exists = TRUE;
                return S_OK;
            }
        }
        // A stand-in exists for this Windows name: report the first
        // installed member of its stack
        if (const Alias* alias = findAlias(wanted)) {
            std::string stack = alias->stack;
            size_t start = 0;
            while (start < stack.size()) {
                size_t comma = stack.find(',', start);
                std::string member = lower(stack.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
                for (size_t i = 0; i < names_.size(); i++) {
                    if (lower(names_[i]) == member) {
                        *index = static_cast<UINT32>(i);
                        *exists = TRUE;
                        return S_OK;
                    }
                }
                if (comma == std::string::npos) break;
                start = comma + 1;
            }
        }
        return S_OK;
    }

private:
    std::vector<std::string> names_;
};

// ---- Text format state ---------------------------------------------------------------
struct FormatState {
    std::wstring family = L"Segoe UI";
    std::wstring locale = L"en-us";
    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
    DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
    DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
    FLOAT size = 16.0f;
    DWRITE_TEXT_ALIGNMENT textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
    DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
    DWRITE_WORD_WRAPPING wordWrapping = DWRITE_WORD_WRAPPING_WRAP;
    DWRITE_READING_DIRECTION readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0};
    bool trimmingSign = false;
    DWRITE_LINE_SPACING_METHOD lineSpacingMethod = DWRITE_LINE_SPACING_METHOD_DEFAULT;
    FLOAT lineSpacing = 0.0f;
    FLOAT baseline = 0.0f;
    FLOAT tabStop = 0.0f;  // 0: Pango's default

    PangoFontDescription* describe() const {
        PangoFontDescription* desc = pango_font_description_new();
        pango_font_description_set_family(desc, resolveFamily(family).c_str());
        pango_font_description_set_weight(desc, pangoWeight(weight));
        pango_font_description_set_style(desc, pangoStyle(style));
        pango_font_description_set_stretch(desc, pangoStretch(stretch));
        pango_font_description_set_absolute_size(desc, std::max(1.0, static_cast<double>(size)) * PANGO_SCALE);
        return desc;
    }
};

// IDWriteTextFormat methods over a FormatState, shared by formats and layouts
template <class Base>
class FormatMethods : public TintaComObject<Base> {
public:
    HRESULT SetTextAlignment(DWRITE_TEXT_ALIGNMENT value) override { format_.textAlignment = value; changed(); return S_OK; }
    HRESULT SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT value) override { format_.paragraphAlignment = value; changed(); return S_OK; }
    HRESULT SetWordWrapping(DWRITE_WORD_WRAPPING value) override { format_.wordWrapping = value; changed(); return S_OK; }
    HRESULT SetReadingDirection(DWRITE_READING_DIRECTION value) override { format_.readingDirection = value; changed(); return S_OK; }
    HRESULT SetIncrementalTabStop(FLOAT value) override { format_.tabStop = value; changed(); return S_OK; }
    HRESULT SetTrimming(const DWRITE_TRIMMING* options, IDWriteInlineObject* sign) override {
        if (options) format_.trimming = *options;
        format_.trimmingSign = sign != nullptr;
        changed();
        return S_OK;
    }
    HRESULT SetLineSpacing(DWRITE_LINE_SPACING_METHOD method, FLOAT spacing, FLOAT baseline) override {
        format_.lineSpacingMethod = method;
        format_.lineSpacing = spacing;
        format_.baseline = baseline;
        changed();
        return S_OK;
    }
    DWRITE_TEXT_ALIGNMENT GetTextAlignment() override { return format_.textAlignment; }
    DWRITE_PARAGRAPH_ALIGNMENT GetParagraphAlignment() override { return format_.paragraphAlignment; }
    DWRITE_WORD_WRAPPING GetWordWrapping() override { return format_.wordWrapping; }
    DWRITE_READING_DIRECTION GetReadingDirection() override { return format_.readingDirection; }
    FLOAT GetIncrementalTabStop() override { return format_.tabStop > 0 ? format_.tabStop : 4 * format_.size; }
    HRESULT GetTrimming(DWRITE_TRIMMING* options, IDWriteInlineObject** sign) override {
        *options = format_.trimming;
        if (sign) *sign = nullptr;
        return S_OK;
    }
    HRESULT GetLineSpacing(DWRITE_LINE_SPACING_METHOD* method, FLOAT* spacing, FLOAT* baseline) override {
        *method = format_.lineSpacingMethod;
        *spacing = format_.lineSpacing;
        *baseline = format_.baseline;
        return S_OK;
    }
    HRESULT GetFontCollection(IDWriteFontCollection** collection) override {
        *collection = nullptr;
        return S_OK;
    }
    UINT32 GetFontFamilyNameLength() override { return static_cast<UINT32>(format_.family.size()); }
    HRESULT GetFontFamilyName(WCHAR* name, UINT32 size) override {
        if (size <= format_.family.size()) return E_INVALIDARG;
        wmemcpy(name, format_.family.c_str(), format_.family.size() + 1);
        return S_OK;
    }
    DWRITE_FONT_WEIGHT GetFontWeight() override { return format_.weight; }
    DWRITE_FONT_STYLE GetFontStyle() override { return format_.style; }
    DWRITE_FONT_STRETCH GetFontStretch() override { return format_.stretch; }
    FLOAT GetFontSize() override { return format_.size; }
    UINT32 GetLocaleNameLength() override { return static_cast<UINT32>(format_.locale.size()); }
    HRESULT GetLocaleName(WCHAR* name, UINT32 size) override {
        if (size <= format_.locale.size()) return E_INVALIDARG;
        wmemcpy(name, format_.locale.c_str(), format_.locale.size() + 1);
        return S_OK;
    }

    const FormatState& state() const { return format_; }

protected:
    virtual void changed() {}
    FormatState format_;
};

class TextFormat final : public FormatMethods<IDWriteTextFormat1> {
public:
    using Primary = IDWriteTextFormat1;
    explicit TextFormat(const FormatState& state) { format_ = state; }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteTextFormat1, IDWriteTextFormat>(this, riid, object);
    }
    HRESULT SetFontFallback(IDWriteFontFallback*) override { return S_OK; }
    HRESULT GetFontFallback(IDWriteFontFallback** fallback) override {
        *fallback = nullptr;
        return S_OK;
    }
};

FormatState stateOf(IDWriteTextFormat* format);

// ---- Text layout -----------------------------------------------------------------------
class TextLayout final : public FormatMethods<IDWriteTextLayout2>, public TextLayoutDrawing {
public:
    using Primary = IDWriteTextLayout2;

    TextLayout(const WCHAR* text, UINT32 length, const FormatState& state, FLOAT maxWidth, FLOAT maxHeight)
        : text_(text ? text : L"", text ? length : 0), maxWidth_(maxWidth), maxHeight_(maxHeight) {
        format_ = state;
        utf8_ = toUtf8(text_);
        // byteOf_[i]: UTF-8 offset of character i; byteOf_[n] = total
        byteOf_.reserve(text_.size() + 1);
        size_t offset = 0;
        for (size_t i = 0; i < text_.size(); i++) {
            byteOf_.push_back(static_cast<int>(offset));
            gunichar c = g_utf8_get_char(utf8_.c_str() + offset);
            offset += g_unichar_to_utf8(c, nullptr);
        }
        byteOf_.push_back(static_cast<int>(utf8_.size()));
    }
    ~TextLayout() override {
        if (layout_) g_object_unref(layout_);
        for (PangoAttribute* attr : attrs_) pango_attribute_destroy(attr);
    }

    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteTextLayout2, IDWriteTextLayout1, IDWriteTextLayout, IDWriteTextFormat>(
            this, riid, object);
    }

    // -- IDWriteTextLayout setters
    HRESULT SetMaxWidth(FLOAT width) override { maxWidth_ = width; changed(); return S_OK; }
    HRESULT SetMaxHeight(FLOAT height) override { maxHeight_ = height; changed(); return S_OK; }
    HRESULT SetFontFamilyName(const WCHAR* name, DWRITE_TEXT_RANGE range) override {
        return addAttr(range, pango_attr_family_new(resolveFamily(name ? name : L"").c_str()));
    }
    HRESULT SetFontWeight(DWRITE_FONT_WEIGHT weight, DWRITE_TEXT_RANGE range) override {
        return addAttr(range, pango_attr_weight_new(pangoWeight(weight)));
    }
    HRESULT SetFontStyle(DWRITE_FONT_STYLE style, DWRITE_TEXT_RANGE range) override {
        return addAttr(range, pango_attr_style_new(pangoStyle(style)));
    }
    HRESULT SetFontStretch(DWRITE_FONT_STRETCH stretch, DWRITE_TEXT_RANGE range) override {
        return addAttr(range, pango_attr_stretch_new(pangoStretch(stretch)));
    }
    HRESULT SetFontSize(FLOAT size, DWRITE_TEXT_RANGE range) override {
        return addAttr(range, pango_attr_size_new_absolute(static_cast<int>(std::max(1.0f, size) * PANGO_SCALE)));
    }
    HRESULT SetUnderline(BOOL on, DWRITE_TEXT_RANGE range) override {
        return addAttr(range, pango_attr_underline_new(on ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE));
    }
    HRESULT SetStrikethrough(BOOL on, DWRITE_TEXT_RANGE range) override {
        return addAttr(range, pango_attr_strikethrough_new(on ? TRUE : FALSE));
    }
    HRESULT SetTypography(IDWriteTypography* typography, DWRITE_TEXT_RANGE range) override {
        auto* own = static_cast<Typography*>(typography);
        if (!own || own->features.empty()) return S_OK;
        return addAttr(range, pango_attr_font_features_new(featureString(own->features).c_str()));
    }
    HRESULT SetLocaleName(const WCHAR* locale, DWRITE_TEXT_RANGE range) override {
        std::string tag = toUtf8(locale ? locale : L"", locale ? wcslen(locale) : 0);
        return addAttr(range, pango_attr_language_new(pango_language_from_string(tag.c_str())));
    }
    HRESULT SetPairKerning(BOOL on, DWRITE_TEXT_RANGE range) override {
        return addAttr(range, pango_attr_font_features_new(on ? "kern=1" : "kern=0"));
    }
    HRESULT SetCharacterSpacing(FLOAT leading, FLOAT trailing, FLOAT, DWRITE_TEXT_RANGE range) override {
        return addAttr(range, pango_attr_letter_spacing_new(static_cast<int>((leading + trailing) * PANGO_SCALE)));
    }
    HRESULT SetFontFallback(IDWriteFontFallback*) override { return S_OK; }
    HRESULT GetFontFallback(IDWriteFontFallback** fallback) override {
        *fallback = nullptr;
        return S_OK;
    }
    FLOAT GetMaxWidth() override { return maxWidth_; }
    FLOAT GetMaxHeight() override { return maxHeight_; }

    // -- Metrics
    HRESULT GetMetrics(DWRITE_TEXT_METRICS* metrics) override {
        ensure();
        float left = 0.0f, width = 0.0f, withWhitespace = 0.0f;
        bool first = true;
        for (const auto& line : lines_) {
            if (first || line.x < left) left = line.x;
            first = false;
            width = std::max(width, line.width);
            withWhitespace = std::max(withWhitespace, line.widthWithWhitespace);
        }
        metrics->left = left;
        metrics->top = yOffset_;
        metrics->width = width;
        metrics->widthIncludingTrailingWhitespace = withWhitespace;
        metrics->height = totalHeight_;
        metrics->layoutWidth = maxWidth_;
        metrics->layoutHeight = maxHeight_;
        metrics->maxBidiReorderingDepth = 1;
        metrics->lineCount = static_cast<UINT32>(lines_.size());
        return S_OK;
    }

    HRESULT GetLineMetrics(DWRITE_LINE_METRICS* metrics, UINT32 maxCount, UINT32* actual) override {
        ensure();
        *actual = static_cast<UINT32>(lines_.size());
        if (!metrics || maxCount < lines_.size()) return E_NOT_SUFFICIENT_BUFFER_COMPAT;
        for (size_t i = 0; i < lines_.size(); i++) {
            const Line& line = lines_[i];
            metrics[i].length = line.length;
            metrics[i].trailingWhitespaceLength = line.trailingWhitespace;
            metrics[i].newlineLength = line.newline;
            metrics[i].height = line.height;
            metrics[i].baseline = line.baseline;
            metrics[i].isTrimmed = line.trimmed;
        }
        return S_OK;
    }

    HRESULT GetOverhangMetrics(DWRITE_OVERHANG_METRICS* overhangs) override {
        std::memset(overhangs, 0, sizeof *overhangs);
        return S_OK;
    }

    HRESULT GetClusterMetrics(DWRITE_CLUSTER_METRICS* metrics, UINT32 maxCount, UINT32* actual) override {
        ensure();
        *actual = static_cast<UINT32>(clusters_.size());
        if (!metrics || maxCount < clusters_.size()) return E_NOT_SUFFICIENT_BUFFER_COMPAT;
        std::copy(clusters_.begin(), clusters_.end(), metrics);
        return S_OK;
    }

    HRESULT DetermineMinWidth(FLOAT* minWidth) override {
        ensure();
        // The widest unbreakable run: the cluster widths between break points
        float widest = 0.0f, run = 0.0f;
        for (const auto& cluster : clusters_) {
            if (!cluster.isWhitespace) run += cluster.width;
            if (cluster.canWrapLineAfter) {
                widest = std::max(widest, run);
                run = 0.0f;
            }
        }
        *minWidth = std::max(widest, run);
        return S_OK;
    }

    // -- Hit testing
    HRESULT HitTestPoint(FLOAT x, FLOAT y, BOOL* isTrailingHit, BOOL* isInside,
                         DWRITE_HIT_TEST_METRICS* metrics) override {
        ensure();
        size_t lineIndex = 0;
        bool insideY = false;
        for (size_t i = 0; i < lines_.size(); i++) {
            const Line& line = lines_[i];
            float top = yOffset_ + line.top;
            if (y < top) break;
            lineIndex = i;
            if (y < top + line.height) {
                insideY = true;
                break;
            }
        }
        const Line& line = lines_[lineIndex];
        PangoLayoutLine* pl = pango_layout_get_line_readonly(layout_, line.pangoIndex);
        int index = 0, trailing = 0;
        bool insideX = pango_layout_line_x_to_index(pl, static_cast<int>((x - line.x) * PANGO_SCALE), &index, &trailing);
        insideX = insideX && x >= line.x && x <= line.x + line.widthWithWhitespace;
        UINT32 position = charOf(index);
        // Pango puts a point past the end of the line on its last cluster,
        // trailing; DirectWrite does the same
        *isTrailingHit = trailing > 0 ? TRUE : FALSE;
        *isInside = insideX && insideY ? TRUE : FALSE;
        fillHitMetrics(position, line, metrics);
        return S_OK;
    }

    HRESULT HitTestTextPosition(UINT32 position, BOOL trailing, FLOAT* x, FLOAT* y,
                                DWRITE_HIT_TEST_METRICS* metrics) override {
        ensure();
        position = std::min<UINT32>(position, static_cast<UINT32>(text_.size()));
        const Line& line = lineOf(position);
        PangoLayoutLine* pl = pango_layout_get_line_readonly(layout_, line.pangoIndex);
        int byte = byteOf_[position];
        int lineEnd = byteOf_[line.start + line.length];
        int xPos = 0;
        if (byte >= lineEnd - static_cast<int>(lineEndGap(line))) {
            // Past the line's visible text (its newline or the text end): the caret
            // sits after the last glyph
            pango_layout_line_index_to_x(pl, std::max(0, lineEnd - static_cast<int>(lineEndGap(line))), FALSE, &xPos);
        } else {
            pango_layout_line_index_to_x(pl, byte, trailing ? TRUE : FALSE, &xPos);
        }
        *x = line.x + static_cast<float>(xPos) / PANGO_SCALE;
        *y = yOffset_ + line.top;
        fillHitMetrics(position, line, metrics);
        return S_OK;
    }

    HRESULT HitTestTextRange(UINT32 position, UINT32 length, FLOAT originX, FLOAT originY,
                             DWRITE_HIT_TEST_METRICS* metrics, UINT32 maxCount, UINT32* actual) override {
        ensure();
        std::vector<DWRITE_HIT_TEST_METRICS> out;
        UINT32 end = std::min<UINT32>(position + length, static_cast<UINT32>(text_.size()));
        position = std::min<UINT32>(position, static_cast<UINT32>(text_.size()));
        for (const Line& line : lines_) {
            UINT32 lineStart = line.start, lineEnd = line.start + line.length;
            if (length == 0) {
                if (position < lineStart || position > lineEnd || (position == lineEnd && &line != &lines_.back())) continue;
                FLOAT cx = 0, cy = 0;
                DWRITE_HIT_TEST_METRICS caret{};
                HitTestTextPosition(position, FALSE, &cx, &cy, &caret);
                caret.left = originX + cx;
                caret.top = originY + cy;
                caret.width = 0;
                caret.length = 0;
                out.push_back(caret);
                break;
            }
            UINT32 a = std::max(position, lineStart), b = std::min(end, lineEnd);
            if (a >= b) continue;
            PangoLayoutLine* pl = pango_layout_get_line_readonly(layout_, line.pangoIndex);
            int* ranges = nullptr;
            int rangeCount = 0;
            pango_layout_line_get_x_ranges(pl, byteOf_[a], byteOf_[b], &ranges, &rangeCount);
            if (rangeCount == 0) {
                // Only the newline is selected: a zero-width box at the line end
                DWRITE_HIT_TEST_METRICS m{};
                m.textPosition = a;
                m.length = b - a;
                m.left = originX + line.x + line.widthWithWhitespace;
                m.top = originY + yOffset_ + line.top;
                m.width = 0;
                m.height = line.height;
                m.isText = TRUE;
                out.push_back(m);
            }
            for (int r = 0; r < rangeCount; r++) {
                DWRITE_HIT_TEST_METRICS m{};
                m.textPosition = a;
                m.length = b - a;
                float left = static_cast<float>(ranges[2 * r]) / PANGO_SCALE;
                float right = static_cast<float>(ranges[2 * r + 1]) / PANGO_SCALE;
                m.left = originX + line.x + left;
                m.top = originY + yOffset_ + line.top;
                m.width = right - left;
                m.height = line.height;
                m.isText = TRUE;
                out.push_back(m);
            }
            g_free(ranges);
        }
        *actual = static_cast<UINT32>(out.size());
        if (!metrics || maxCount < out.size()) return E_NOT_SUFFICIENT_BUFFER_COMPAT;
        std::copy(out.begin(), out.end(), metrics);
        return S_OK;
    }

    // -- TextLayoutDrawing
    void drawTo(cairo_t* cr, float originX, float originY) override {
        ensure();
        for (const Line& line : lines_) {
            PangoLayoutLine* pl = pango_layout_get_line_readonly(layout_, line.pangoIndex);
            cairo_move_to(cr, originX + line.x, originY + yOffset_ + line.top + line.baseline);
            pango_cairo_show_layout_line(cr, pl);
        }
    }
    float boxWidth() const override { return maxWidth_; }
    float boxHeight() const override { return maxHeight_; }

private:
    static constexpr HRESULT E_NOT_SUFFICIENT_BUFFER_COMPAT = static_cast<HRESULT>(0x8007007A);

    struct Line {
        int pangoIndex;
        UINT32 start;               // character range of the line, including its newline
        UINT32 length;
        UINT32 trailingWhitespace;  // includes the newline, as in DirectWrite
        UINT32 newline;
        float x;                    // alignment offset
        float top;                  // relative to the layout box, before yOffset_
        float height;
        float baseline;             // from the line top
        float width;                // without trailing whitespace
        float widthWithWhitespace;
        BOOL trimmed;
    };

    void changed() override { dirty_ = true; }

    HRESULT addAttr(DWRITE_TEXT_RANGE range, PangoAttribute* attr) {
        UINT32 start = std::min<UINT32>(range.startPosition, static_cast<UINT32>(text_.size()));
        UINT64 endWide = static_cast<UINT64>(range.startPosition) + range.length;
        UINT32 end = static_cast<UINT32>(std::min<UINT64>(endWide, text_.size()));
        attr->start_index = static_cast<guint>(byteOf_[start]);
        attr->end_index = static_cast<guint>(byteOf_[end]);
        attrs_.push_back(attr);
        dirty_ = true;
        return S_OK;
    }

    UINT32 charOf(int byte) const {
        auto it = std::lower_bound(byteOf_.begin(), byteOf_.end(), byte);
        return static_cast<UINT32>(it - byteOf_.begin());
    }

    // Characters at the line end that have no glyphs: the newline
    UINT32 lineEndGap(const Line& line) const { return line.newline; }

    const Line& lineOf(UINT32 position) const {
        for (const Line& line : lines_) {
            if (position < line.start + line.length) return line;
        }
        return lines_.back();
    }

    void fillHitMetrics(UINT32 position, const Line& line, DWRITE_HIT_TEST_METRICS* metrics) {
        if (!metrics) return;
        std::memset(metrics, 0, sizeof *metrics);
        // The cluster containing `position`
        UINT32 clusterStart = 0, clusterLength = 0;
        float clusterX = 0.0f, clusterWidth = 0.0f;
        UINT32 cursor = 0;
        for (const auto& cluster : clusters_) {
            if (position < cursor + cluster.length || &cluster == &clusters_.back()) {
                clusterStart = cursor;
                clusterLength = cluster.length;
                clusterWidth = cluster.width;
                break;
            }
            cursor += cluster.length;
        }
        if (clusterStart < text_.size()) {
            PangoLayoutLine* pl = pango_layout_get_line_readonly(layout_, line.pangoIndex);
            int x = 0;
            pango_layout_line_index_to_x(pl, byteOf_[clusterStart], FALSE, &x);
            clusterX = static_cast<float>(x) / PANGO_SCALE;
        } else {
            clusterX = line.widthWithWhitespace;
        }
        metrics->textPosition = clusterStart;
        metrics->length = clusterLength;
        metrics->left = line.x + clusterX;
        metrics->top = yOffset_ + line.top;
        metrics->width = clusterWidth;
        metrics->height = line.height;
        metrics->isText = TRUE;
    }

    bool wraps() const { return format_.wordWrapping != DWRITE_WORD_WRAPPING_NO_WRAP; }

    void ensure() {
        if (!dirty_ && layout_) return;
        dirty_ = false;
        if (!layout_) layout_ = pango_layout_new(pangoContext());
        else pango_layout_context_changed(layout_);

        PangoFontDescription* desc = format_.describe();
        pango_layout_set_font_description(layout_, desc);
        pango_font_description_free(desc);
        pango_layout_set_text(layout_, utf8_.data(), static_cast<int>(utf8_.size()));

        PangoAttrList* list = pango_attr_list_new();
        std::string locale = toUtf8(format_.locale);
        pango_attr_list_insert(list, pango_attr_language_new(pango_language_from_string(locale.c_str())));
        for (PangoAttribute* attr : attrs_) pango_attr_list_insert(list, pango_attribute_copy(attr));
        pango_layout_set_attributes(layout_, list);
        pango_attr_list_unref(list);

        if (format_.tabStop > 0) {
            PangoTabArray* tabs = pango_tab_array_new(1, TRUE);
            pango_tab_array_set_tab(tabs, 0, PANGO_TAB_LEFT, static_cast<int>(format_.tabStop));
            pango_layout_set_tabs(layout_, tabs);
            pango_tab_array_free(tabs);
        }

        bool trimming = format_.trimming.granularity != DWRITE_TRIMMING_GRANULARITY_NONE && format_.trimmingSign;
        bool useWidth = (wraps() || trimming) && maxWidth_ > 0 && maxWidth_ < 1.0e6f;
        pango_layout_set_width(layout_, useWidth ? static_cast<int>(maxWidth_ * PANGO_SCALE) : -1);
        switch (format_.wordWrapping) {
            case DWRITE_WORD_WRAPPING_CHARACTER: pango_layout_set_wrap(layout_, PANGO_WRAP_CHAR); break;
            case DWRITE_WORD_WRAPPING_WHOLE_WORD: pango_layout_set_wrap(layout_, PANGO_WRAP_WORD); break;
            default: pango_layout_set_wrap(layout_, PANGO_WRAP_WORD_CHAR); break;
        }
        pango_layout_set_ellipsize(layout_, trimming ? PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE);
        if (trimming && wraps() && maxHeight_ > 0 && maxHeight_ < 1.0e6f) {
            pango_layout_set_height(layout_, static_cast<int>(maxHeight_ * PANGO_SCALE));
        } else {
            pango_layout_set_height(layout_, -1);
        }
        // Alignment is applied per line below, against maxWidth
        pango_layout_set_alignment(layout_, PANGO_ALIGN_LEFT);
        pango_layout_set_justify(layout_, format_.textAlignment == DWRITE_TEXT_ALIGNMENT_JUSTIFIED && useWidth);
        pango_layout_set_auto_dir(layout_, TRUE);

        buildLines();
        buildClusters();
    }

    void buildLines() {
        lines_.clear();
        int count = pango_layout_get_line_count(layout_);
        bool uniform = format_.lineSpacingMethod == DWRITE_LINE_SPACING_METHOD_UNIFORM;
        bool proportional = format_.lineSpacingMethod == DWRITE_LINE_SPACING_METHOD_PROPORTIONAL;
        PangoLayoutIter* iter = pango_layout_get_iter(layout_);
        float top = 0.0f;
        for (int i = 0; i < count; i++) {
            PangoLayoutLine* pl = pango_layout_iter_get_line_readonly(iter);
            PangoRectangle logical;
            pango_layout_iter_get_line_extents(iter, nullptr, &logical);
            int baselineY = pango_layout_iter_get_baseline(iter);

            Line line{};
            line.pangoIndex = i;
            line.start = charOf(pl->start_index);
            UINT32 next = i + 1 < count ? charOf(pango_layout_get_line_readonly(layout_, i + 1)->start_index)
                                        : static_cast<UINT32>(text_.size());
            line.length = next - line.start;
            // Newline sequence at the end of the line
            UINT32 nl = 0;
            UINT32 end = line.start + line.length;
            if (nl < line.length && end >= 1 && text_[end - 1] == L'\n') nl++;
            if (nl < line.length && end >= nl + 1 && text_[end - nl - 1] == L'\r') nl++;
            if (nl == 0 && end >= 1 && (text_[end - 1] == L'\r' || text_[end - 1] == 0x2028 || text_[end - 1] == 0x2029)) nl = 1;
            line.newline = nl;
            UINT32 ws = nl;
            while (ws < line.length && std::iswspace(text_[end - ws - 1])) ws++;
            line.trailingWhitespace = ws;

            float naturalHeight = static_cast<float>(logical.height) / PANGO_SCALE;
            float naturalBaseline = static_cast<float>(baselineY - logical.y) / PANGO_SCALE;
            if (uniform) {
                line.height = format_.lineSpacing;
                line.baseline = format_.baseline;
                line.top = top;
            } else if (proportional) {
                line.height = naturalHeight * format_.lineSpacing;
                line.baseline = naturalBaseline * format_.baseline;
                line.top = top;
            } else {
                line.height = naturalHeight;
                line.baseline = naturalBaseline;
                line.top = static_cast<float>(logical.y) / PANGO_SCALE;
            }
            top = line.top + line.height;

            PangoRectangle lineLogical;
            pango_layout_line_get_extents(pl, nullptr, &lineLogical);
            line.widthWithWhitespace = static_cast<float>(lineLogical.width) / PANGO_SCALE;
            UINT32 visibleEnd = end - ws;
            if (visibleEnd <= line.start) {
                line.width = 0.0f;
            } else {
                int xEnd = 0;
                pango_layout_line_index_to_x(pl, byteOf_[visibleEnd - 1], TRUE, &xEnd);
                line.width = std::min(line.widthWithWhitespace, static_cast<float>(xEnd) / PANGO_SCALE);
            }
            line.trimmed = pango_layout_is_ellipsized(layout_) ? TRUE : FALSE;

            // Alignment within maxWidth, ignoring trailing whitespace as DirectWrite does
            float free = maxWidth_ < 1.0e6f ? maxWidth_ - line.width : 0.0f;
            switch (format_.textAlignment) {
                case DWRITE_TEXT_ALIGNMENT_CENTER: line.x = free / 2.0f; break;
                case DWRITE_TEXT_ALIGNMENT_TRAILING: line.x = free; break;
                default: line.x = 0.0f; break;
            }
            lines_.push_back(line);
            pango_layout_iter_next_line(iter);
        }
        pango_layout_iter_free(iter);

        totalHeight_ = lines_.empty() ? 0.0f : lines_.back().top + lines_.back().height;
        float freeHeight = maxHeight_ < 1.0e6f ? maxHeight_ - totalHeight_ : 0.0f;
        switch (format_.paragraphAlignment) {
            case DWRITE_PARAGRAPH_ALIGNMENT_CENTER: yOffset_ = freeHeight / 2.0f; break;
            case DWRITE_PARAGRAPH_ALIGNMENT_FAR: yOffset_ = freeHeight; break;
            default: yOffset_ = 0.0f; break;
        }
    }

    void buildClusters() {
        clusters_.clear();
        int attrCount = 0;
        const PangoLogAttr* logAttrs = pango_layout_get_log_attrs_readonly(layout_, &attrCount);
        // Glyph advances summed per character offset (cluster start)
        std::vector<float> advance(text_.size() + 1, 0.0f);
        PangoLayoutIter* iter = pango_layout_get_iter(layout_);
        do {
            PangoLayoutRun* run = pango_layout_iter_get_run_readonly(iter);
            if (!run) continue;
            PangoGlyphString* glyphs = run->glyphs;
            int offset = run->item->offset;
            for (int g = 0; g < glyphs->num_glyphs; g++) {
                int byte = offset + glyphs->log_clusters[g];
                UINT32 c = charOf(byte);
                if (c < advance.size()) advance[c] += static_cast<float>(glyphs->glyphs[g].geometry.width) / PANGO_SCALE;
            }
        } while (pango_layout_iter_next_run(iter));
        pango_layout_iter_free(iter);

        size_t n = text_.size();
        size_t start = 0;
        while (start < n) {
            size_t end = start + 1;
            // A cluster runs to the next cursor position (grapheme boundary)
            while (end < n && !(static_cast<int>(end) < attrCount && logAttrs[end].is_cursor_position)) end++;
            DWRITE_CLUSTER_METRICS cluster{};
            float width = 0.0f;
            for (size_t i = start; i < end; i++) width += advance[i];
            cluster.width = width;
            cluster.length = static_cast<UINT16>(end - start);
            cluster.canWrapLineAfter = (static_cast<int>(end) < attrCount && (logAttrs[end].is_line_break || logAttrs[end].is_mandatory_break)) || end == n;
            bool white = true;
            for (size_t i = start; i < end; i++) {
                if (!(static_cast<int>(i) < attrCount && logAttrs[i].is_white)) white = false;
            }
            cluster.isWhitespace = white;
            wchar_t c = text_[start];
            cluster.isNewline = (c == L'\n' || c == L'\r' || c == 0x2028 || c == 0x2029);
            cluster.isSoftHyphen = c == 0x00AD;
            clusters_.push_back(cluster);
            start = end;
        }
    }

    std::wstring text_;
    std::string utf8_;
    std::vector<int> byteOf_;
    FLOAT maxWidth_;
    FLOAT maxHeight_;
    std::vector<PangoAttribute*> attrs_;
    PangoLayout* layout_ = nullptr;
    bool dirty_ = true;
    std::vector<Line> lines_;
    std::vector<DWRITE_CLUSTER_METRICS> clusters_;
    float totalHeight_ = 0.0f;
    float yOffset_ = 0.0f;
};

FormatState stateOf(IDWriteTextFormat* format) {
    if (auto* own = dynamic_cast<TextFormat*>(format)) return own->state();
    if (auto* layout = dynamic_cast<TextLayout*>(format)) return layout->state();
    // A foreign implementation: read it through the interface
    FormatState state;
    if (!format) return state;
    UINT32 n = format->GetFontFamilyNameLength();
    std::wstring family(n + 1, L'\0');
    format->GetFontFamilyName(family.data(), n + 1);
    family.resize(n);
    state.family = family;
    state.weight = format->GetFontWeight();
    state.style = format->GetFontStyle();
    state.stretch = format->GetFontStretch();
    state.size = format->GetFontSize();
    state.textAlignment = format->GetTextAlignment();
    state.paragraphAlignment = format->GetParagraphAlignment();
    state.wordWrapping = format->GetWordWrapping();
    return state;
}

// ---- Text analyzer ----------------------------------------------------------------------
class TextAnalyzer final : public TintaComObject<IDWriteTextAnalyzer> {
public:
    using Primary = IDWriteTextAnalyzer;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteTextAnalyzer>(this, riid, object);
    }
    HRESULT AnalyzeLineBreakpoints(IDWriteTextAnalysisSource* source, UINT32 position, UINT32 length,
                                   IDWriteTextAnalysisSink* sink) override {
        if (!source || !sink) return E_INVALIDARG;
        std::wstring text;
        UINT32 cursor = position;
        while (cursor < position + length) {
            const WCHAR* chunk = nullptr;
            UINT32 chunkLength = 0;
            if (FAILED(source->GetTextAtPosition(cursor, &chunk, &chunkLength)) || !chunk || chunkLength == 0) break;
            chunkLength = std::min(chunkLength, position + length - cursor);
            text.append(chunk, chunkLength);
            cursor += chunkLength;
        }
        size_t n = text.size();
        std::string utf8 = toUtf8(text);
        std::vector<PangoLogAttr> attrs(n + 1);
        pango_get_log_attrs(utf8.c_str(), static_cast<int>(utf8.size()), -1, pango_language_get_default(),
                            attrs.data(), static_cast<int>(attrs.size()));
        auto condition = [&](size_t i) -> UINT8 {
            if (i == 0 || i >= n) return DWRITE_BREAK_CONDITION_NEUTRAL;
            if (attrs[i].is_mandatory_break) return DWRITE_BREAK_CONDITION_MUST_BREAK;
            return attrs[i].is_line_break ? DWRITE_BREAK_CONDITION_CAN_BREAK : DWRITE_BREAK_CONDITION_MAY_NOT_BREAK;
        };
        std::vector<DWRITE_LINE_BREAKPOINT> breakpoints(n);
        for (size_t i = 0; i < n; i++) {
            DWRITE_LINE_BREAKPOINT& bp = breakpoints[i];
            bp.breakConditionBefore = condition(i);
            bp.breakConditionAfter = condition(i + 1);
            bp.isWhitespace = attrs[i].is_white ? 1 : 0;
            bp.isSoftHyphen = text[i] == 0x00AD ? 1 : 0;
            bp.padding = 0;
        }
        if (n) sink->SetLineBreakpoints(position, static_cast<UINT32>(n), breakpoints.data());
        return S_OK;
    }
};

// ---- Factory ----------------------------------------------------------------------------
class Factory final : public TintaComObject<IDWriteFactory2> {
public:
    using Primary = IDWriteFactory2;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IDWriteFactory2, IDWriteFactory1, IDWriteFactory>(this, riid, object);
    }
    HRESULT GetSystemFontCollection(IDWriteFontCollection** collection, BOOL) override {
        *collection = new FontCollection();
        return S_OK;
    }
    HRESULT CreateRenderingParams(IDWriteRenderingParams** params) override {
        *params = new RenderingParams();
        return S_OK;
    }
    HRESULT CreateCustomRenderingParams(FLOAT, FLOAT, FLOAT, DWRITE_PIXEL_GEOMETRY, DWRITE_RENDERING_MODE,
                                        IDWriteRenderingParams** params) override {
        *params = new RenderingParams();
        return S_OK;
    }
    HRESULT CreateTextFormat(const WCHAR* family, IDWriteFontCollection*, DWRITE_FONT_WEIGHT weight,
                             DWRITE_FONT_STYLE style, DWRITE_FONT_STRETCH stretch, FLOAT size, const WCHAR* locale,
                             IDWriteTextFormat** format) override {
        if (!format) return E_POINTER;
        FormatState state;
        state.family = family ? family : L"";
        state.weight = weight;
        state.style = style;
        state.stretch = stretch;
        state.size = size;
        if (locale && locale[0]) state.locale = locale;
        *format = new TextFormat(state);
        return S_OK;
    }
    HRESULT CreateTypography(IDWriteTypography** typography) override {
        *typography = new Typography();
        return S_OK;
    }
    HRESULT CreateTextLayout(const WCHAR* string, UINT32 length, IDWriteTextFormat* format, FLOAT maxWidth,
                             FLOAT maxHeight, IDWriteTextLayout** layout) override {
        if (!layout || !format) return E_INVALIDARG;
        *layout = new TextLayout(string, length, stateOf(format), maxWidth, maxHeight);
        return S_OK;
    }
    HRESULT CreateEllipsisTrimmingSign(IDWriteTextFormat*, IDWriteInlineObject** sign) override {
        *sign = new EllipsisSign();
        return S_OK;
    }
    HRESULT CreateTextAnalyzer(IDWriteTextAnalyzer** analyzer) override {
        *analyzer = new TextAnalyzer();
        return S_OK;
    }
    HRESULT GetSystemFontFallback(IDWriteFontFallback** fallback) override {
        *fallback = new FontFallback();
        return S_OK;
    }
    HRESULT CreateFontFallbackBuilder(IDWriteFontFallbackBuilder** builder) override {
        *builder = new FontFallbackBuilder();
        return S_OK;
    }
};

}  // namespace

TextLayoutDrawing* drawingOf(IDWriteTextLayout* layout) { return dynamic_cast<TextLayout*>(layout); }

}  // namespace tinta

HRESULT DWriteCreateFactory(DWRITE_FACTORY_TYPE, REFIID iid, IUnknown** factory) {
    if (!factory) return E_POINTER;
    auto* created = new tinta::Factory();
    HRESULT hr = created->QueryInterface(iid, reinterpret_cast<void**>(factory));
    created->Release();
    return hr;
}
