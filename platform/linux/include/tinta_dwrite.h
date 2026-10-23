// DirectWrite declarations for the Linux compat layer (implemented on Pango
// in platform/linux/src/dwrite_pango.cpp). Signatures follow the Windows SDK.
// Methods shared code only calls have default bodies, so implementations
// override what they support; interfaces shared code implements itself
// (analysis source and sink) stay pure.
//
// Text positions and lengths are in wchar_t units, as on Windows; wchar_t
// is UTF-32 here, so they count code points instead of UTF-16 units.
#ifndef TINTA_LINUX_DWRITE_H
#define TINTA_LINUX_DWRITE_H

#include "tinta_com.h"

typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int32_t INT32;
typedef float FLOAT;

enum DWRITE_FACTORY_TYPE { DWRITE_FACTORY_TYPE_SHARED, DWRITE_FACTORY_TYPE_ISOLATED };

enum DWRITE_FONT_WEIGHT {
    DWRITE_FONT_WEIGHT_THIN = 100,
    DWRITE_FONT_WEIGHT_EXTRA_LIGHT = 200,
    DWRITE_FONT_WEIGHT_ULTRA_LIGHT = 200,
    DWRITE_FONT_WEIGHT_LIGHT = 300,
    DWRITE_FONT_WEIGHT_SEMI_LIGHT = 350,
    DWRITE_FONT_WEIGHT_NORMAL = 400,
    DWRITE_FONT_WEIGHT_REGULAR = 400,
    DWRITE_FONT_WEIGHT_MEDIUM = 500,
    DWRITE_FONT_WEIGHT_DEMI_BOLD = 600,
    DWRITE_FONT_WEIGHT_SEMI_BOLD = 600,
    DWRITE_FONT_WEIGHT_BOLD = 700,
    DWRITE_FONT_WEIGHT_EXTRA_BOLD = 800,
    DWRITE_FONT_WEIGHT_ULTRA_BOLD = 800,
    DWRITE_FONT_WEIGHT_BLACK = 900,
    DWRITE_FONT_WEIGHT_HEAVY = 900,
    DWRITE_FONT_WEIGHT_EXTRA_BLACK = 950,
    DWRITE_FONT_WEIGHT_ULTRA_BLACK = 950,
};

enum DWRITE_FONT_STYLE { DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STYLE_OBLIQUE, DWRITE_FONT_STYLE_ITALIC };

enum DWRITE_FONT_STRETCH {
    DWRITE_FONT_STRETCH_UNDEFINED = 0,
    DWRITE_FONT_STRETCH_ULTRA_CONDENSED = 1,
    DWRITE_FONT_STRETCH_EXTRA_CONDENSED = 2,
    DWRITE_FONT_STRETCH_CONDENSED = 3,
    DWRITE_FONT_STRETCH_SEMI_CONDENSED = 4,
    DWRITE_FONT_STRETCH_NORMAL = 5,
    DWRITE_FONT_STRETCH_MEDIUM = 5,
    DWRITE_FONT_STRETCH_SEMI_EXPANDED = 6,
    DWRITE_FONT_STRETCH_EXPANDED = 7,
    DWRITE_FONT_STRETCH_EXTRA_EXPANDED = 8,
    DWRITE_FONT_STRETCH_ULTRA_EXPANDED = 9,
};

enum DWRITE_TEXT_ALIGNMENT {
    DWRITE_TEXT_ALIGNMENT_LEADING,
    DWRITE_TEXT_ALIGNMENT_TRAILING,
    DWRITE_TEXT_ALIGNMENT_CENTER,
    DWRITE_TEXT_ALIGNMENT_JUSTIFIED,
};

enum DWRITE_PARAGRAPH_ALIGNMENT {
    DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
    DWRITE_PARAGRAPH_ALIGNMENT_FAR,
    DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
};

enum DWRITE_WORD_WRAPPING {
    DWRITE_WORD_WRAPPING_WRAP = 0,
    DWRITE_WORD_WRAPPING_NO_WRAP = 1,
    DWRITE_WORD_WRAPPING_EMERGENCY_BREAK = 2,
    DWRITE_WORD_WRAPPING_WHOLE_WORD = 3,
    DWRITE_WORD_WRAPPING_CHARACTER = 4,
};

enum DWRITE_READING_DIRECTION {
    DWRITE_READING_DIRECTION_LEFT_TO_RIGHT = 0,
    DWRITE_READING_DIRECTION_RIGHT_TO_LEFT = 1,
    DWRITE_READING_DIRECTION_TOP_TO_BOTTOM = 2,
    DWRITE_READING_DIRECTION_BOTTOM_TO_TOP = 3,
};

enum DWRITE_FLOW_DIRECTION {
    DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM = 0,
    DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP = 1,
    DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT = 2,
    DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT = 3,
};

enum DWRITE_TRIMMING_GRANULARITY {
    DWRITE_TRIMMING_GRANULARITY_NONE,
    DWRITE_TRIMMING_GRANULARITY_CHARACTER,
    DWRITE_TRIMMING_GRANULARITY_WORD,
};

enum DWRITE_LINE_SPACING_METHOD {
    DWRITE_LINE_SPACING_METHOD_DEFAULT,
    DWRITE_LINE_SPACING_METHOD_UNIFORM,
    DWRITE_LINE_SPACING_METHOD_PROPORTIONAL,
};

enum DWRITE_BREAK_CONDITION {
    DWRITE_BREAK_CONDITION_NEUTRAL,
    DWRITE_BREAK_CONDITION_CAN_BREAK,
    DWRITE_BREAK_CONDITION_MAY_NOT_BREAK,
    DWRITE_BREAK_CONDITION_MUST_BREAK,
};

enum DWRITE_MEASURING_MODE {
    DWRITE_MEASURING_MODE_NATURAL,
    DWRITE_MEASURING_MODE_GDI_CLASSIC,
    DWRITE_MEASURING_MODE_GDI_NATURAL,
};

enum DWRITE_RENDERING_MODE {
    DWRITE_RENDERING_MODE_DEFAULT,
    DWRITE_RENDERING_MODE_ALIASED,
    DWRITE_RENDERING_MODE_GDI_CLASSIC,
    DWRITE_RENDERING_MODE_GDI_NATURAL,
    DWRITE_RENDERING_MODE_NATURAL,
    DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
    DWRITE_RENDERING_MODE_OUTLINE,
    DWRITE_RENDERING_MODE_CLEARTYPE_GDI_CLASSIC = DWRITE_RENDERING_MODE_GDI_CLASSIC,
    DWRITE_RENDERING_MODE_CLEARTYPE_GDI_NATURAL = DWRITE_RENDERING_MODE_GDI_NATURAL,
    DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL = DWRITE_RENDERING_MODE_NATURAL,
    DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC = DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
};

enum DWRITE_PIXEL_GEOMETRY { DWRITE_PIXEL_GEOMETRY_FLAT, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_PIXEL_GEOMETRY_BGR };

enum DWRITE_NUMBER_SUBSTITUTION_METHOD {
    DWRITE_NUMBER_SUBSTITUTION_METHOD_FROM_CULTURE,
    DWRITE_NUMBER_SUBSTITUTION_METHOD_CONTEXTUAL,
    DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE,
    DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL,
    DWRITE_NUMBER_SUBSTITUTION_METHOD_TRADITIONAL,
};

enum DWRITE_SCRIPT_SHAPES { DWRITE_SCRIPT_SHAPES_DEFAULT = 0, DWRITE_SCRIPT_SHAPES_NO_VISUAL = 1 };

#define DWRITE_MAKE_OPENTYPE_TAG(a, b, c, d) \
    ((static_cast<UINT32>(d) << 24) | (static_cast<UINT32>(c) << 16) | (static_cast<UINT32>(b) << 8) | static_cast<UINT32>(a))

enum DWRITE_FONT_FEATURE_TAG : UINT32 {
    DWRITE_FONT_FEATURE_TAG_ALTERNATIVE_FRACTIONS = DWRITE_MAKE_OPENTYPE_TAG('a', 'f', 'r', 'c'),
    DWRITE_FONT_FEATURE_TAG_CASE_SENSITIVE_FORMS = DWRITE_MAKE_OPENTYPE_TAG('c', 'a', 's', 'e'),
    DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES = DWRITE_MAKE_OPENTYPE_TAG('c', 'a', 'l', 't'),
    DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES = DWRITE_MAKE_OPENTYPE_TAG('c', 'l', 'i', 'g'),
    DWRITE_FONT_FEATURE_TAG_DISCRETIONARY_LIGATURES = DWRITE_MAKE_OPENTYPE_TAG('d', 'l', 'i', 'g'),
    DWRITE_FONT_FEATURE_TAG_FRACTIONS = DWRITE_MAKE_OPENTYPE_TAG('f', 'r', 'a', 'c'),
    DWRITE_FONT_FEATURE_TAG_KERNING = DWRITE_MAKE_OPENTYPE_TAG('k', 'e', 'r', 'n'),
    DWRITE_FONT_FEATURE_TAG_LINING_FIGURES = DWRITE_MAKE_OPENTYPE_TAG('l', 'n', 'u', 'm'),
    DWRITE_FONT_FEATURE_TAG_OLD_STYLE_FIGURES = DWRITE_MAKE_OPENTYPE_TAG('o', 'n', 'u', 'm'),
    DWRITE_FONT_FEATURE_TAG_PROPORTIONAL_FIGURES = DWRITE_MAKE_OPENTYPE_TAG('p', 'n', 'u', 'm'),
    DWRITE_FONT_FEATURE_TAG_SLASHED_ZERO = DWRITE_MAKE_OPENTYPE_TAG('z', 'e', 'r', 'o'),
    DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES = DWRITE_MAKE_OPENTYPE_TAG('l', 'i', 'g', 'a'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_ALTERNATES = DWRITE_MAKE_OPENTYPE_TAG('s', 'a', 'l', 't'),
    DWRITE_FONT_FEATURE_TAG_SUBSCRIPT = DWRITE_MAKE_OPENTYPE_TAG('s', 'u', 'b', 's'),
    DWRITE_FONT_FEATURE_TAG_SUPERSCRIPT = DWRITE_MAKE_OPENTYPE_TAG('s', 'u', 'p', 's'),
    DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES = DWRITE_MAKE_OPENTYPE_TAG('t', 'n', 'u', 'm'),
};

struct DWRITE_TEXT_RANGE {
    UINT32 startPosition;
    UINT32 length;
};

struct DWRITE_TRIMMING {
    DWRITE_TRIMMING_GRANULARITY granularity;
    UINT32 delimiter;
    UINT32 delimiterCount;
};

struct DWRITE_TEXT_METRICS {
    FLOAT left;
    FLOAT top;
    FLOAT width;
    FLOAT widthIncludingTrailingWhitespace;
    FLOAT height;
    FLOAT layoutWidth;
    FLOAT layoutHeight;
    UINT32 maxBidiReorderingDepth;
    UINT32 lineCount;
};

struct DWRITE_LINE_METRICS {
    UINT32 length;
    UINT32 trailingWhitespaceLength;
    UINT32 newlineLength;
    FLOAT height;
    FLOAT baseline;
    BOOL isTrimmed;
};

struct DWRITE_CLUSTER_METRICS {
    FLOAT width;
    UINT16 length;
    UINT16 canWrapLineAfter : 1;
    UINT16 isWhitespace : 1;
    UINT16 isNewline : 1;
    UINT16 isSoftHyphen : 1;
    UINT16 isRightToLeft : 1;
    UINT16 padding : 11;
};

struct DWRITE_HIT_TEST_METRICS {
    UINT32 textPosition;
    UINT32 length;
    FLOAT left;
    FLOAT top;
    FLOAT width;
    FLOAT height;
    UINT32 bidiLevel;
    BOOL isText;
    BOOL isTrimmed;
};

struct DWRITE_OVERHANG_METRICS {
    FLOAT left;
    FLOAT top;
    FLOAT right;
    FLOAT bottom;
};

struct DWRITE_INLINE_OBJECT_METRICS {
    FLOAT width;
    FLOAT height;
    FLOAT baseline;
    BOOL supportsSideways;
};

struct DWRITE_LINE_BREAKPOINT {
    UINT8 breakConditionBefore : 2;
    UINT8 breakConditionAfter : 2;
    UINT8 isWhitespace : 1;
    UINT8 isSoftHyphen : 1;
    UINT8 padding : 2;
};

struct DWRITE_SCRIPT_ANALYSIS {
    UINT16 script;
    DWRITE_SCRIPT_SHAPES shapes;
};

struct DWRITE_FONT_FEATURE {
    DWRITE_FONT_FEATURE_TAG nameTag;
    UINT32 parameter;
};

struct DWRITE_UNICODE_RANGE {
    UINT32 first;
    UINT32 last;
};

struct DWRITE_FONT_METRICS {
    UINT16 designUnitsPerEm;
    UINT16 ascent;
    UINT16 descent;
    int16_t lineGap;
    UINT16 capHeight;
    UINT16 xHeight;
    int16_t underlinePosition;
    UINT16 underlineThickness;
    int16_t strikethroughPosition;
    UINT16 strikethroughThickness;
};

// ---- Interfaces ---------------------------------------------------------------
TINTA_UUID(IDWriteFontFamily, 0x0d570001)
TINTA_UUID(IDWriteFont, 0x0d570002)
TINTA_UUID(IDWriteFontCollection, 0x0d570003)
TINTA_UUID(IDWriteLocalizedStrings, 0x0d570004)
TINTA_UUID(IDWriteFontList, 0x0d570005)
TINTA_UUID(IDWriteTextFormat, 0x0d570006)
TINTA_UUID(IDWriteTextFormat1, 0x0d570007)
TINTA_UUID(IDWriteTextLayout, 0x0d570008)
TINTA_UUID(IDWriteTextLayout1, 0x0d570009)
TINTA_UUID(IDWriteTextLayout2, 0x0d57000a)
TINTA_UUID(IDWriteTypography, 0x0d57000b)
TINTA_UUID(IDWriteInlineObject, 0x0d57000c)
TINTA_UUID(IDWriteRenderingParams, 0x0d57000d)
TINTA_UUID(IDWriteNumberSubstitution, 0x0d57000e)
TINTA_UUID(IDWriteTextAnalysisSource, 0x0d57000f)
TINTA_UUID(IDWriteTextAnalysisSink, 0x0d570010)
TINTA_UUID(IDWriteTextAnalyzer, 0x0d570011)
TINTA_UUID(IDWriteFontFallback, 0x0d570012)
TINTA_UUID(IDWriteFontFallbackBuilder, 0x0d570013)
TINTA_UUID(IDWriteFactory, 0x0d570014)
TINTA_UUID(IDWriteFactory1, 0x0d570015)
TINTA_UUID(IDWriteFactory2, 0x0d570016)
TINTA_UUID(IDWriteFontFace, 0x0d570017)
TINTA_UUID(IDWriteTextRenderer, 0x0d570018)
TINTA_UUID(IDWritePixelSnapping, 0x0d570019)

struct IDWriteLocalizedStrings : IUnknown {
    virtual UINT32 GetCount() { return 0; }
    virtual HRESULT FindLocaleName(const WCHAR* localeName, UINT32* index, BOOL* exists) {
        (void)localeName; *index = 0; *exists = FALSE; return E_NOTIMPL;
    }
    virtual HRESULT GetStringLength(UINT32 index, UINT32* length) { (void)index; *length = 0; return E_NOTIMPL; }
    virtual HRESULT GetString(UINT32 index, WCHAR* buffer, UINT32 size) {
        (void)index; if (size) buffer[0] = 0; return E_NOTIMPL;
    }
};

struct IDWriteFontList : IUnknown {
    virtual HRESULT GetFontCollection(IDWriteFontCollection** collection) { *collection = nullptr; return E_NOTIMPL; }
    virtual UINT32 GetFontCount() { return 0; }
    virtual HRESULT GetFont(UINT32 index, IDWriteFont** font) { (void)index; *font = nullptr; return E_NOTIMPL; }
};

struct IDWriteFontFamily : IDWriteFontList {
    virtual HRESULT GetFamilyNames(IDWriteLocalizedStrings** names) { *names = nullptr; return E_NOTIMPL; }
    virtual HRESULT GetFirstMatchingFont(DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STRETCH stretch,
                                         DWRITE_FONT_STYLE style, IDWriteFont** font) {
        (void)weight; (void)stretch; (void)style; *font = nullptr; return E_NOTIMPL;
    }
};

struct IDWriteFont : IUnknown {
    virtual HRESULT GetFontFamily(IDWriteFontFamily** family) { *family = nullptr; return E_NOTIMPL; }
    virtual DWRITE_FONT_WEIGHT GetWeight() { return DWRITE_FONT_WEIGHT_NORMAL; }
    virtual DWRITE_FONT_STRETCH GetStretch() { return DWRITE_FONT_STRETCH_NORMAL; }
    virtual DWRITE_FONT_STYLE GetStyle() { return DWRITE_FONT_STYLE_NORMAL; }
    virtual BOOL IsSymbolFont() { return FALSE; }
    virtual void GetMetrics(DWRITE_FONT_METRICS* metrics) { std::memset(metrics, 0, sizeof *metrics); }
    virtual HRESULT HasCharacter(UINT32 unicodeValue, BOOL* exists) { (void)unicodeValue; *exists = FALSE; return E_NOTIMPL; }
};

struct IDWriteFontCollection : IUnknown {
    virtual UINT32 GetFontFamilyCount() { return 0; }
    virtual HRESULT GetFontFamily(UINT32 index, IDWriteFontFamily** family) { (void)index; *family = nullptr; return E_NOTIMPL; }
    virtual HRESULT FindFamilyName(const WCHAR* familyName, UINT32* index, BOOL* exists) {
        (void)familyName; *index = 0; *exists = FALSE; return E_NOTIMPL;
    }
    virtual HRESULT GetFontFromFontFace(IDWriteFontFace* fontFace, IDWriteFont** font) {
        (void)fontFace; *font = nullptr; return E_NOTIMPL;
    }
};

struct IDWriteInlineObject : IUnknown {
    virtual HRESULT GetMetrics(DWRITE_INLINE_OBJECT_METRICS* metrics) { std::memset(metrics, 0, sizeof *metrics); return E_NOTIMPL; }
    virtual HRESULT GetOverhangMetrics(DWRITE_OVERHANG_METRICS* overhangs) { std::memset(overhangs, 0, sizeof *overhangs); return E_NOTIMPL; }
    virtual HRESULT GetBreakConditions(DWRITE_BREAK_CONDITION* before, DWRITE_BREAK_CONDITION* after) {
        *before = *after = DWRITE_BREAK_CONDITION_NEUTRAL; return E_NOTIMPL;
    }
};

struct IDWriteRenderingParams : IUnknown {
    virtual FLOAT GetGamma() { return 1.8f; }
    virtual FLOAT GetEnhancedContrast() { return 0.5f; }
    virtual FLOAT GetClearTypeLevel() { return 0.0f; }
    virtual DWRITE_PIXEL_GEOMETRY GetPixelGeometry() { return DWRITE_PIXEL_GEOMETRY_FLAT; }
    virtual DWRITE_RENDERING_MODE GetRenderingMode() { return DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC; }
};

struct IDWriteNumberSubstitution : IUnknown {};

struct IDWriteTypography : IUnknown {
    virtual HRESULT AddFontFeature(DWRITE_FONT_FEATURE fontFeature) = 0;
    virtual UINT32 GetFontFeatureCount() = 0;
    virtual HRESULT GetFontFeature(UINT32 index, DWRITE_FONT_FEATURE* fontFeature) = 0;
};

struct IDWriteTextAnalysisSource;

struct IDWriteFontFallback : IUnknown {
    virtual HRESULT MapCharacters(IDWriteTextAnalysisSource* analysisSource, UINT32 textPosition, UINT32 textLength,
                                  IDWriteFontCollection* baseFontCollection, const WCHAR* baseFamilyName,
                                  DWRITE_FONT_WEIGHT baseWeight, DWRITE_FONT_STYLE baseStyle,
                                  DWRITE_FONT_STRETCH baseStretch, UINT32* mappedLength, IDWriteFont** mappedFont,
                                  FLOAT* scale) = 0;
};

// ---- Glyph runs and text renderers (IDWriteTextLayout::Draw) ---------------------------
struct DWRITE_MATRIX {
    FLOAT m11, m12, m21, m22, dx, dy;
};
struct DWRITE_GLYPH_OFFSET {
    FLOAT advanceOffset;
    FLOAT ascenderOffset;
};
struct IDWriteFontFace : IUnknown {
    virtual void GetMetrics(DWRITE_FONT_METRICS* metrics) { std::memset(metrics, 0, sizeof *metrics); }
    virtual UINT16 GetGlyphCount() { return 0; }
};
struct DWRITE_GLYPH_RUN {
    IDWriteFontFace* fontFace;
    FLOAT fontEmSize;
    UINT32 glyphCount;
    const UINT16* glyphIndices;
    const FLOAT* glyphAdvances;
    const DWRITE_GLYPH_OFFSET* glyphOffsets;
    BOOL isSideways;
    UINT32 bidiLevel;
};
struct DWRITE_GLYPH_RUN_DESCRIPTION {
    const WCHAR* localeName;
    const WCHAR* string;
    UINT32 stringLength;
    const UINT16* clusterMap;
    UINT32 textPosition;
};
struct DWRITE_UNDERLINE {
    FLOAT width, thickness, offset, runHeight;
    DWRITE_READING_DIRECTION readingDirection;
    DWRITE_FLOW_DIRECTION flowDirection;
    const WCHAR* localeName;
    DWRITE_MEASURING_MODE measuringMode;
};
struct DWRITE_STRIKETHROUGH {
    FLOAT width, thickness, offset;
    DWRITE_READING_DIRECTION readingDirection;
    DWRITE_FLOW_DIRECTION flowDirection;
    const WCHAR* localeName;
    DWRITE_MEASURING_MODE measuringMode;
};
struct IDWritePixelSnapping : IUnknown {
    virtual HRESULT IsPixelSnappingDisabled(void* clientDrawingContext, BOOL* isDisabled) = 0;
    virtual HRESULT GetCurrentTransform(void* clientDrawingContext, DWRITE_MATRIX* transform) = 0;
    virtual HRESULT GetPixelsPerDip(void* clientDrawingContext, FLOAT* pixelsPerDip) = 0;
};
struct IDWriteTextRenderer : IDWritePixelSnapping {
    virtual HRESULT DrawGlyphRun(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY,
                                 DWRITE_MEASURING_MODE measuringMode, const DWRITE_GLYPH_RUN* glyphRun,
                                 const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription,
                                 IUnknown* clientDrawingEffect) = 0;
    virtual HRESULT DrawUnderline(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY,
                                  const DWRITE_UNDERLINE* underline, IUnknown* clientDrawingEffect) = 0;
    virtual HRESULT DrawStrikethrough(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY,
                                      const DWRITE_STRIKETHROUGH* strikethrough, IUnknown* clientDrawingEffect) = 0;
    virtual HRESULT DrawInlineObject(void* clientDrawingContext, FLOAT originX, FLOAT originY,
                                     IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft,
                                     IUnknown* clientDrawingEffect) = 0;
};

struct IDWriteFontFallbackBuilder : IUnknown {
    virtual HRESULT AddMapping(const DWRITE_UNICODE_RANGE* ranges, UINT32 rangesCount,
                               const WCHAR** targetFamilyNames, UINT32 targetFamilyNamesCount,
                               IDWriteFontCollection* fontCollection = nullptr,
                               const WCHAR* localeName = nullptr, const WCHAR* baseFamilyName = nullptr,
                               FLOAT scale = 1.0f) = 0;
    virtual HRESULT AddMappings(IDWriteFontFallback* fontFallback) = 0;
    virtual HRESULT CreateFontFallback(IDWriteFontFallback** fontFallback) = 0;
};

struct IDWriteTextFormat : IUnknown {
    virtual HRESULT SetTextAlignment(DWRITE_TEXT_ALIGNMENT textAlignment) = 0;
    virtual HRESULT SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment) = 0;
    virtual HRESULT SetWordWrapping(DWRITE_WORD_WRAPPING wordWrapping) = 0;
    virtual HRESULT SetReadingDirection(DWRITE_READING_DIRECTION readingDirection) = 0;
    virtual HRESULT SetFlowDirection(DWRITE_FLOW_DIRECTION flowDirection) { (void)flowDirection; return S_OK; }
    virtual HRESULT SetIncrementalTabStop(FLOAT incrementalTabStop) = 0;
    virtual HRESULT SetTrimming(const DWRITE_TRIMMING* trimmingOptions, IDWriteInlineObject* trimmingSign) = 0;
    virtual HRESULT SetLineSpacing(DWRITE_LINE_SPACING_METHOD lineSpacingMethod, FLOAT lineSpacing, FLOAT baseline) = 0;
    virtual DWRITE_TEXT_ALIGNMENT GetTextAlignment() = 0;
    virtual DWRITE_PARAGRAPH_ALIGNMENT GetParagraphAlignment() = 0;
    virtual DWRITE_WORD_WRAPPING GetWordWrapping() = 0;
    virtual DWRITE_READING_DIRECTION GetReadingDirection() = 0;
    virtual DWRITE_FLOW_DIRECTION GetFlowDirection() { return DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM; }
    virtual FLOAT GetIncrementalTabStop() = 0;
    virtual HRESULT GetTrimming(DWRITE_TRIMMING* trimmingOptions, IDWriteInlineObject** trimmingSign) = 0;
    virtual HRESULT GetLineSpacing(DWRITE_LINE_SPACING_METHOD* lineSpacingMethod, FLOAT* lineSpacing, FLOAT* baseline) = 0;
    virtual HRESULT GetFontCollection(IDWriteFontCollection** fontCollection) = 0;
    virtual UINT32 GetFontFamilyNameLength() = 0;
    virtual HRESULT GetFontFamilyName(WCHAR* fontFamilyName, UINT32 nameSize) = 0;
    virtual DWRITE_FONT_WEIGHT GetFontWeight() = 0;
    virtual DWRITE_FONT_STYLE GetFontStyle() = 0;
    virtual DWRITE_FONT_STRETCH GetFontStretch() = 0;
    virtual FLOAT GetFontSize() = 0;
    virtual UINT32 GetLocaleNameLength() = 0;
    virtual HRESULT GetLocaleName(WCHAR* localeName, UINT32 nameSize) = 0;
};

struct IDWriteTextFormat1 : IDWriteTextFormat {
    virtual HRESULT SetFontFallback(IDWriteFontFallback* fontFallback) = 0;
    virtual HRESULT GetFontFallback(IDWriteFontFallback** fontFallback) = 0;
};

struct IDWriteTextLayout : IDWriteTextFormat {
    virtual HRESULT SetMaxWidth(FLOAT maxWidth) = 0;
    virtual HRESULT SetMaxHeight(FLOAT maxHeight) = 0;
    virtual HRESULT SetFontCollection(IDWriteFontCollection* fontCollection, DWRITE_TEXT_RANGE textRange) {
        (void)fontCollection; (void)textRange; return S_OK;
    }
    virtual HRESULT SetFontFamilyName(const WCHAR* fontFamilyName, DWRITE_TEXT_RANGE textRange) = 0;
    virtual HRESULT SetFontWeight(DWRITE_FONT_WEIGHT fontWeight, DWRITE_TEXT_RANGE textRange) = 0;
    virtual HRESULT SetFontStyle(DWRITE_FONT_STYLE fontStyle, DWRITE_TEXT_RANGE textRange) = 0;
    virtual HRESULT SetFontStretch(DWRITE_FONT_STRETCH fontStretch, DWRITE_TEXT_RANGE textRange) = 0;
    virtual HRESULT SetFontSize(FLOAT fontSize, DWRITE_TEXT_RANGE textRange) = 0;
    virtual HRESULT SetUnderline(BOOL hasUnderline, DWRITE_TEXT_RANGE textRange) = 0;
    virtual HRESULT SetStrikethrough(BOOL hasStrikethrough, DWRITE_TEXT_RANGE textRange) = 0;
    virtual HRESULT SetDrawingEffect(IUnknown* drawingEffect, DWRITE_TEXT_RANGE textRange) {
        (void)drawingEffect; (void)textRange; return S_OK;
    }
    virtual HRESULT SetInlineObject(IDWriteInlineObject* inlineObject, DWRITE_TEXT_RANGE textRange) {
        (void)inlineObject; (void)textRange; return E_NOTIMPL;
    }
    virtual HRESULT SetTypography(IDWriteTypography* typography, DWRITE_TEXT_RANGE textRange) = 0;
    virtual HRESULT SetLocaleName(const WCHAR* localeName, DWRITE_TEXT_RANGE textRange) = 0;
    virtual FLOAT GetMaxWidth() = 0;
    virtual FLOAT GetMaxHeight() = 0;
    virtual HRESULT GetLineMetrics(DWRITE_LINE_METRICS* lineMetrics, UINT32 maxLineCount, UINT32* actualLineCount) = 0;
    virtual HRESULT GetMetrics(DWRITE_TEXT_METRICS* textMetrics) = 0;
    virtual HRESULT GetOverhangMetrics(DWRITE_OVERHANG_METRICS* overhangs) = 0;
    virtual HRESULT GetClusterMetrics(DWRITE_CLUSTER_METRICS* clusterMetrics, UINT32 maxClusterCount,
                                      UINT32* actualClusterCount) = 0;
    virtual HRESULT DetermineMinWidth(FLOAT* minWidth) = 0;
    virtual HRESULT HitTestPoint(FLOAT pointX, FLOAT pointY, BOOL* isTrailingHit, BOOL* isInside,
                                 DWRITE_HIT_TEST_METRICS* hitTestMetrics) = 0;
    virtual HRESULT HitTestTextPosition(UINT32 textPosition, BOOL isTrailingHit, FLOAT* pointX, FLOAT* pointY,
                                        DWRITE_HIT_TEST_METRICS* hitTestMetrics) = 0;
    virtual HRESULT HitTestTextRange(UINT32 textPosition, UINT32 textLength, FLOAT originX, FLOAT originY,
                                     DWRITE_HIT_TEST_METRICS* hitTestMetrics, UINT32 maxHitTestMetricsCount,
                                     UINT32* actualHitTestMetricsCount) = 0;
    virtual HRESULT Draw(void* clientDrawingContext, IDWriteTextRenderer* renderer, FLOAT originX, FLOAT originY) {
        (void)clientDrawingContext; (void)renderer; (void)originX; (void)originY; return E_NOTIMPL;
    }
    // The family the text sets at a position (a SetFontFamilyName range, or
    // the format's), as DirectWrite reports it: never the fallback's
    using IDWriteTextFormat::GetFontFamilyName;
    virtual HRESULT GetFontFamilyName(UINT32 currentPosition, WCHAR* fontFamilyName, UINT32 nameSize,
                                      DWRITE_TEXT_RANGE* textRange = nullptr) {
        (void)currentPosition; (void)fontFamilyName; (void)nameSize; (void)textRange; return E_NOTIMPL;
    }
};

struct IDWriteTextLayout1 : IDWriteTextLayout {
    virtual HRESULT SetPairKerning(BOOL isPairKerningEnabled, DWRITE_TEXT_RANGE textRange) = 0;
    virtual HRESULT SetCharacterSpacing(FLOAT leadingSpacing, FLOAT trailingSpacing, FLOAT minimumAdvanceWidth,
                                        DWRITE_TEXT_RANGE textRange) = 0;
};

struct IDWriteTextLayout2 : IDWriteTextLayout1 {
    virtual HRESULT SetFontFallback(IDWriteFontFallback* fontFallback) = 0;
    virtual HRESULT GetFontFallback(IDWriteFontFallback** fontFallback) = 0;
};

struct IDWriteTextAnalysisSource : IUnknown {
    virtual HRESULT GetTextAtPosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength) = 0;
    virtual HRESULT GetTextBeforePosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength) = 0;
    virtual DWRITE_READING_DIRECTION GetParagraphReadingDirection() = 0;
    virtual HRESULT GetLocaleName(UINT32 textPosition, UINT32* textLength, const WCHAR** localeName) = 0;
    virtual HRESULT GetNumberSubstitution(UINT32 textPosition, UINT32* textLength,
                                          IDWriteNumberSubstitution** numberSubstitution) = 0;
};

struct IDWriteTextAnalysisSink : IUnknown {
    virtual HRESULT SetScriptAnalysis(UINT32 textPosition, UINT32 textLength,
                                      const DWRITE_SCRIPT_ANALYSIS* scriptAnalysis) = 0;
    virtual HRESULT SetLineBreakpoints(UINT32 textPosition, UINT32 textLength,
                                       const DWRITE_LINE_BREAKPOINT* lineBreakpoints) = 0;
    virtual HRESULT SetBidiLevel(UINT32 textPosition, UINT32 textLength, UINT8 explicitLevel,
                                 UINT8 resolvedLevel) = 0;
    virtual HRESULT SetNumberSubstitution(UINT32 textPosition, UINT32 textLength,
                                          IDWriteNumberSubstitution* numberSubstitution) = 0;
};

struct IDWriteTextAnalyzer : IUnknown {
    virtual HRESULT AnalyzeLineBreakpoints(IDWriteTextAnalysisSource* analysisSource, UINT32 textPosition,
                                           UINT32 textLength, IDWriteTextAnalysisSink* analysisSink) = 0;
};

struct IDWriteFactory : IUnknown {
    virtual HRESULT GetSystemFontCollection(IDWriteFontCollection** fontCollection, BOOL checkForUpdates = FALSE) = 0;
    virtual HRESULT CreateRenderingParams(IDWriteRenderingParams** renderingParams) = 0;
    virtual HRESULT CreateCustomRenderingParams(FLOAT gamma, FLOAT enhancedContrast, FLOAT clearTypeLevel,
                                                DWRITE_PIXEL_GEOMETRY pixelGeometry,
                                                DWRITE_RENDERING_MODE renderingMode,
                                                IDWriteRenderingParams** renderingParams) = 0;
    virtual HRESULT CreateTextFormat(const WCHAR* fontFamilyName, IDWriteFontCollection* fontCollection,
                                     DWRITE_FONT_WEIGHT fontWeight, DWRITE_FONT_STYLE fontStyle,
                                     DWRITE_FONT_STRETCH fontStretch, FLOAT fontSize, const WCHAR* localeName,
                                     IDWriteTextFormat** textFormat) = 0;
    virtual HRESULT CreateTypography(IDWriteTypography** typography) = 0;
    virtual HRESULT CreateTextLayout(const WCHAR* string, UINT32 stringLength, IDWriteTextFormat* textFormat,
                                     FLOAT maxWidth, FLOAT maxHeight, IDWriteTextLayout** textLayout) = 0;
    virtual HRESULT CreateEllipsisTrimmingSign(IDWriteTextFormat* textFormat, IDWriteInlineObject** trimmingSign) = 0;
    virtual HRESULT CreateTextAnalyzer(IDWriteTextAnalyzer** textAnalyzer) = 0;
    virtual HRESULT CreateNumberSubstitution(DWRITE_NUMBER_SUBSTITUTION_METHOD method, const WCHAR* localeName,
                                             BOOL ignoreUserOverride, IDWriteNumberSubstitution** substitution) {
        (void)method; (void)localeName; (void)ignoreUserOverride; *substitution = nullptr; return E_NOTIMPL;
    }
};

struct IDWriteFactory1 : IDWriteFactory {};

struct IDWriteFactory2 : IDWriteFactory1 {
    virtual HRESULT GetSystemFontFallback(IDWriteFontFallback** fontFallback) = 0;
    virtual HRESULT CreateFontFallbackBuilder(IDWriteFontFallbackBuilder** fontFallbackBuilder) = 0;
};

HRESULT DWriteCreateFactory(DWRITE_FACTORY_TYPE factoryType, REFIID iid, IUnknown** factory);

#endif  // TINTA_LINUX_DWRITE_H
