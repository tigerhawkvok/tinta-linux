// DirectWrite-on-Pango behaviour the shared layout code depends on.
#include <dwrite_2.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    failures++;
}

bool near(float a, float b, float tolerance = 0.5f) { return std::fabs(a - b) <= tolerance; }

// Records each glyph run's family (as IDWriteFontCollection names it) and glyphs
class RunRecorder : public IDWriteTextRenderer {
public:
    explicit RunRecorder(IDWriteFontCollection* fonts) : fonts_(fonts) {}
    struct Run {
        std::wstring family;
        std::vector<UINT16> glyphs;
    };
    std::vector<Run> runs;
    HRESULT QueryInterface(REFIID, void** out) override {
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG AddRef() override { return 1; }
    ULONG Release() override { return 1; }
    HRESULT IsPixelSnappingDisabled(void*, BOOL* disabled) override {
        *disabled = FALSE;
        return S_OK;
    }
    HRESULT GetCurrentTransform(void*, DWRITE_MATRIX* m) override {
        *m = {1, 0, 0, 1, 0, 0};
        return S_OK;
    }
    HRESULT GetPixelsPerDip(void*, FLOAT* p) override {
        *p = 1;
        return S_OK;
    }
    HRESULT DrawGlyphRun(void*, FLOAT, FLOAT, DWRITE_MEASURING_MODE, const DWRITE_GLYPH_RUN* run,
                         const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown*) override {
        Run out;
        IDWriteFont* font = nullptr;
        IDWriteFontFamily* family = nullptr;
        IDWriteLocalizedStrings* names = nullptr;
        if (SUCCEEDED(fonts_->GetFontFromFontFace(run->fontFace, &font)) && SUCCEEDED(font->GetFontFamily(&family)) &&
            SUCCEEDED(family->GetFamilyNames(&names))) {
            UINT32 length = 0;
            names->GetStringLength(0, &length);
            out.family.resize(length + 1);
            names->GetString(0, out.family.data(), length + 1);
            out.family.resize(length);
        }
        if (names) names->Release();
        if (family) family->Release();
        if (font) font->Release();
        out.glyphs.assign(run->glyphIndices, run->glyphIndices + run->glyphCount);
        runs.push_back(out);
        return S_OK;
    }
    HRESULT DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) override { return S_OK; }
    HRESULT DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) override { return S_OK; }
    HRESULT DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override {
        return S_OK;
    }

private:
    IDWriteFontCollection* fonts_;
};

// A fallback putting ideographs in `first`, then `second`, then the system
IDWriteFontFallback* cjkFallback(IDWriteFactory2* factory, const wchar_t* first, const wchar_t* second) {
    IDWriteFontFallbackBuilder* builder = nullptr;
    factory->CreateFontFallbackBuilder(&builder);
    DWRITE_UNICODE_RANGE ideographs{0x4E00, 0x9FFF};
    const wchar_t* families[] = {first, second};
    builder->AddMapping(&ideographs, 1, families, 2);
    IDWriteFontFallback* system = nullptr;
    factory->GetSystemFontFallback(&system);
    builder->AddMappings(system);
    system->Release();
    IDWriteFontFallback* fallback = nullptr;
    builder->CreateFontFallback(&fallback);
    builder->Release();
    return fallback;
}

IDWriteTextLayout* layoutOf(IDWriteFactory* factory, IDWriteTextFormat* format, const std::wstring& text,
                            float maxWidth = 100000.0f, float maxHeight = 100000.0f) {
    IDWriteTextLayout* layout = nullptr;
    factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format, maxWidth, maxHeight, &layout);
    return layout;
}

}  // namespace

int main() {
    IDWriteFactory* factory = nullptr;
    check(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                        reinterpret_cast<IUnknown**>(&factory))) && factory,
          "factory");
    IDWriteFactory2* factory2 = nullptr;
    check(SUCCEEDED(factory->QueryInterface(__uuidof(IDWriteFactory2), reinterpret_cast<void**>(&factory2))),
          "factory answers IDWriteFactory2");

    IDWriteTextFormat* format = nullptr;
    factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                              DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &format);
    check(format && format->GetFontSize() == 16.0f, "format keeps its size");

    // Single line: positive width, a plausible line height, one line
    IDWriteTextLayout* hello = layoutOf(factory, format, L"Hello, world");
    DWRITE_TEXT_METRICS m{};
    hello->GetMetrics(&m);
    check(m.lineCount == 1, "one line");
    check(m.width > 50 && m.width < 150, "width plausible: " + std::to_string(m.width));
    check(m.height > 16 && m.height < 30, "height plausible: " + std::to_string(m.height));

    // Trailing whitespace counts only in widthIncludingTrailingWhitespace
    IDWriteTextLayout* spaced = layoutOf(factory, format, L"Hello, world   ");
    DWRITE_TEXT_METRICS ms{};
    spaced->GetMetrics(&ms);
    check(near(ms.width, m.width), "trailing spaces excluded from width");
    check(ms.widthIncludingTrailingWhitespace > m.width + 5, "trailing spaces in widthIncludingTrailingWhitespace");

    // Cluster widths add up to the width; lengths cover the text
    UINT32 clusterCount = 0;
    check(FAILED(hello->GetClusterMetrics(nullptr, 0, &clusterCount)) && clusterCount == 12,
          "cluster count query: " + std::to_string(clusterCount));
    std::vector<DWRITE_CLUSTER_METRICS> clusters(clusterCount);
    hello->GetClusterMetrics(clusters.data(), clusterCount, &clusterCount);
    float sum = 0;
    UINT32 length = 0;
    for (const auto& c : clusters) {
        sum += c.width;
        length += c.length;
    }
    check(near(sum, m.widthIncludingTrailingWhitespace, 0.05f), "cluster widths sum to the line width");
    check(length == 12, "cluster lengths cover the text");
    check(clusters[6].isWhitespace && clusters[6].canWrapLineAfter, "space after the comma is a break point");
    check(!clusters[0].canWrapLineAfter, "no break inside a word");

    // Hit testing round-trips: position -> x -> position
    for (UINT32 pos : {0u, 3u, 7u, 11u}) {
        FLOAT x = 0, y = 0;
        DWRITE_HIT_TEST_METRICS hm{};
        hello->HitTestTextPosition(pos, FALSE, &x, &y, &hm);
        BOOL trailing = FALSE, inside = FALSE;
        DWRITE_HIT_TEST_METRICS back{};
        hello->HitTestPoint(x + 0.5f, y + 2, &trailing, &inside, &back);
        check(back.textPosition == pos && !trailing && inside,
              "hit test round trip at " + std::to_string(pos) + " -> " + std::to_string(back.textPosition));
    }
    BOOL trailing = FALSE, inside = FALSE;
    DWRITE_HIT_TEST_METRICS past{};
    hello->HitTestPoint(m.width + 40, 5, &trailing, &inside, &past);
    check(past.textPosition == 11 && trailing && !inside, "a point past the end hits the last cluster, trailing");

    // Wrapping at a max width, with line metrics covering the text
    std::wstring prose = L"The quick brown fox jumps over the lazy dog and keeps running.";
    IDWriteTextLayout* wrapped = layoutOf(factory, format, prose, 120.0f);
    DWRITE_TEXT_METRICS mw{};
    wrapped->GetMetrics(&mw);
    check(mw.lineCount >= 3, "wraps to several lines: " + std::to_string(mw.lineCount));
    check(mw.width <= 120.5f, "wrapped width fits");
    std::vector<DWRITE_LINE_METRICS> lines(mw.lineCount);
    UINT32 lineCount = 0;
    wrapped->GetLineMetrics(lines.data(), mw.lineCount, &lineCount);
    UINT32 covered = 0;
    for (const auto& line : lines) covered += line.length;
    check(covered == prose.size(), "line lengths cover the text");

    // Newlines: the newline belongs to its line
    IDWriteTextLayout* twoLines = layoutOf(factory, format, L"ab\ncd");
    DWRITE_LINE_METRICS nl[2]{};
    twoLines->GetLineMetrics(nl, 2, &lineCount);
    check(lineCount == 2 && nl[0].length == 3 && nl[0].newlineLength == 1 && nl[0].trailingWhitespaceLength == 1 &&
              nl[1].length == 2,
          "newline counted in the first line");

    // Uniform line spacing: fixed pitch and baseline
    IDWriteTextFormat* uniform = nullptr;
    factory->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                              DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &uniform);
    uniform->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 22.0f);
    IDWriteTextLayout* pitched = layoutOf(factory, uniform, L"one\ntwo\nthree");
    DWRITE_TEXT_METRICS mp{};
    pitched->GetMetrics(&mp);
    check(mp.lineCount == 3 && near(mp.height, 90.0f, 0.01f), "uniform spacing: 3 x 30 = 90");
    FLOAT px = 0, py = 0;
    DWRITE_HIT_TEST_METRICS ph{};
    pitched->HitTestTextPosition(8, FALSE, &px, &py, &ph);
    check(near(py, 60.0f, 0.01f) && near(ph.height, 30.0f, 0.01f), "third line starts at 60");

    // Paragraph alignment centers within the max height
    IDWriteTextFormat* centered = nullptr;
    factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                              DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &centered);
    centered->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    centered->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    centered->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    IDWriteTextLayout* box = layoutOf(factory, centered, L"Hi", 200.0f, 100.0f);
    DWRITE_TEXT_METRICS mc{};
    box->GetMetrics(&mc);
    check(near(mc.top, (100.0f - mc.height) / 2) && near(mc.left, (200.0f - mc.width) / 2),
          "centered both ways");

    // Line breaking analysis: break after spaces, between CJK ideographs, not before closing punctuation
    IDWriteTextAnalyzer* analyzer = nullptr;
    factory->CreateTextAnalyzer(&analyzer);
    struct Source final : IDWriteTextAnalysisSource, IDWriteTextAnalysisSink {
        std::wstring text;
        std::vector<DWRITE_LINE_BREAKPOINT> points;
        ULONG AddRef() override { return 2; }
        ULONG Release() override { return 1; }
        HRESULT QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
        HRESULT GetTextAtPosition(UINT32 p, const WCHAR** t, UINT32* n) override {
            *t = p < text.size() ? text.c_str() + p : nullptr;
            *n = p < text.size() ? static_cast<UINT32>(text.size() - p) : 0;
            return S_OK;
        }
        HRESULT GetTextBeforePosition(UINT32, const WCHAR** t, UINT32* n) override { *t = nullptr; *n = 0; return S_OK; }
        DWRITE_READING_DIRECTION GetParagraphReadingDirection() override { return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT; }
        HRESULT GetLocaleName(UINT32, UINT32* n, const WCHAR** l) override { *l = L"en-us"; *n = 0; return S_OK; }
        HRESULT GetNumberSubstitution(UINT32, UINT32* n, IDWriteNumberSubstitution** s) override { *s = nullptr; *n = 0; return S_OK; }
        HRESULT SetScriptAnalysis(UINT32, UINT32, const DWRITE_SCRIPT_ANALYSIS*) override { return S_OK; }
        HRESULT SetLineBreakpoints(UINT32 p, UINT32 n, const DWRITE_LINE_BREAKPOINT* b) override {
            points.assign(b, b + n);
            (void)p;
            return S_OK;
        }
        HRESULT SetBidiLevel(UINT32, UINT32, UINT8, UINT8) override { return S_OK; }
        HRESULT SetNumberSubstitution(UINT32, UINT32, IDWriteNumberSubstitution*) override { return S_OK; }
    } source;
    source.text = L"ab cd 中文字。";
    analyzer->AnalyzeLineBreakpoints(&source, 0, static_cast<UINT32>(source.text.size()), &source);
    check(source.points.size() == source.text.size(), "one breakpoint per character");
    if (source.points.size() == source.text.size()) {
        check(source.points[2].breakConditionAfter == DWRITE_BREAK_CONDITION_CAN_BREAK, "break after the space");
        check(source.points[0].breakConditionAfter == DWRITE_BREAK_CONDITION_MAY_NOT_BREAK, "no break inside a word");
        check(source.points[6].breakConditionAfter == DWRITE_BREAK_CONDITION_CAN_BREAK, "break between ideographs");
        check(source.points[8].breakConditionAfter == DWRITE_BREAK_CONDITION_MAY_NOT_BREAK,
              "no break before the closing full stop");
    }

    // Windows family names resolve; installed families enumerate
    IDWriteFontCollection* fonts = nullptr;
    factory->GetSystemFontCollection(&fonts);
    UINT32 index = 0;
    BOOL exists = FALSE;
    fonts->FindFamilyName(L"Segoe UI", &index, &exists);
    check(exists, "Segoe UI has a stand-in");
    check(fonts->GetFontFamilyCount() > 5, "families enumerate");

    // Custom fallback (Tinta's CJK orders, #155 #254): the order decides
    // which region's face draws a unified ideograph. 直 differs between
    // Japanese and Simplified Chinese forms.
    BOOL jp = FALSE, sc = FALSE;
    fonts->FindFamilyName(L"Yu Gothic UI", &index, &jp);
    fonts->FindFamilyName(L"Microsoft YaHei UI", &index, &sc);
    if (!jp || !sc) {
        std::cout << "skipped CJK fallback: no Japanese and Chinese stand-ins installed\n";
    } else {
        IDWriteFontFallback* japanese = cjkFallback(factory2, L"Yu Gothic UI", L"Microsoft YaHei UI");
        IDWriteFontFallback* chinese = cjkFallback(factory2, L"Microsoft YaHei UI", L"Yu Gothic UI");
        auto runsWith = [&](IDWriteFontFallback* fallback, const std::wstring& text) {
            IDWriteTextFormat* f = nullptr;
            factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                      DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &f);
            IDWriteTextFormat1* f1 = nullptr;
            f->QueryInterface(__uuidof(IDWriteTextFormat1), reinterpret_cast<void**>(&f1));
            f1->SetFontFallback(fallback);
            IDWriteFontFallback* back = nullptr;
            f1->GetFontFallback(&back);
            check(back == fallback, "a format keeps its fallback");
            if (back) back->Release();
            IDWriteTextLayout* l = layoutOf(factory, f, text);
            RunRecorder recorder(fonts);
            l->Draw(nullptr, &recorder, 0, 0);
            l->Release();
            f1->Release();
            f->Release();
            return recorder.runs;
        };
        auto jpRuns = runsWith(japanese, L"A直");
        auto scRuns = runsWith(chinese, L"A直");
        check(jpRuns.size() == 2 && scRuns.size() == 2, "Latin and ideograph draw as two runs");
        if (jpRuns.size() == 2 && scRuns.size() == 2) {
            check(jpRuns[0].family == L"Segoe UI", "Latin stays in the base family");
            check(jpRuns[1].family == L"Yu Gothic UI", "Japanese order draws the ideograph in Yu Gothic UI");
            check(scRuns[1].family == L"Microsoft YaHei UI", "Chinese order draws it in Microsoft YaHei UI");
            check(jpRuns[1].glyphs != scRuns[1].glyphs, "the two orders draw different glyphs for 直");
        }
        // Outside the mapped ranges the chained system fallback still finds a face
        auto kana = runsWith(japanese, L"あ");
        check(kana.size() == 1 && !kana[0].family.empty() && kana[0].family != L"Segoe UI",
              "kana reach a face through the system fallback");
        japanese->Release();
        chinese->Release();
    }

    if (failures) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All DirectWrite tests passed\n";
    return 0;
}
