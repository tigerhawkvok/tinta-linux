// Direct2D declarations for the Linux compat layer (implemented on Cairo in
// platform/linux/src/d2d_cairo.cpp). Signatures and the SDK's reference
// overloads and default arguments follow d2d1.h / d2d1_1.h / d2d1_3.h.
//
// One deliberate difference: the render-target interfaces form a single
// chain, ID2D1RenderTarget -> ID2D1DeviceContext ... -> ID2D1DeviceContext5
// -> ID2D1HwndRenderTarget, so one Cairo object implements all of them
// without diamond inheritance. Every conversion code written against the
// SDK can rely on still exists.
#ifndef TINTA_LINUX_D2D_H
#define TINTA_LINUX_D2D_H

#include <cmath>

#include "tinta_dwrite.h"

struct IWICBitmapSource;
struct IWICBitmap;
struct IWICImagingFactory;
struct IPrintDocumentPackageTarget;

// ---- Basic types ------------------------------------------------------------
struct D2D_POINT_2F { FLOAT x; FLOAT y; };
struct D2D_POINT_2U { UINT32 x; UINT32 y; };
struct D2D_SIZE_F { FLOAT width; FLOAT height; };
struct D2D_SIZE_U { UINT32 width; UINT32 height; };
struct D2D_RECT_F { FLOAT left; FLOAT top; FLOAT right; FLOAT bottom; };
struct D2D_RECT_U { UINT32 left; UINT32 top; UINT32 right; UINT32 bottom; };
struct D2D_COLOR_F { FLOAT r; FLOAT g; FLOAT b; FLOAT a; };
struct D2D_MATRIX_3X2_F {
    FLOAT _11, _12;
    FLOAT _21, _22;
    FLOAT _31, _32;
};
typedef D2D_POINT_2F D2D1_POINT_2F;
typedef D2D_POINT_2U D2D1_POINT_2U;
typedef D2D_SIZE_F D2D1_SIZE_F;
typedef D2D_SIZE_U D2D1_SIZE_U;
typedef D2D_RECT_F D2D1_RECT_F;
typedef D2D_RECT_U D2D1_RECT_U;
typedef D2D_COLOR_F D2D1_COLOR_F;
typedef D2D_MATRIX_3X2_F D2D1_MATRIX_3X2_F;
typedef UINT64 D2D1_TAG;

struct D2D1_ROUNDED_RECT { D2D1_RECT_F rect; FLOAT radiusX; FLOAT radiusY; };
struct D2D1_ELLIPSE { D2D1_POINT_2F point; FLOAT radiusX; FLOAT radiusY; };

enum D2D1_ALPHA_MODE {
    D2D1_ALPHA_MODE_UNKNOWN = 0,
    D2D1_ALPHA_MODE_PREMULTIPLIED = 1,
    D2D1_ALPHA_MODE_STRAIGHT = 2,
    D2D1_ALPHA_MODE_IGNORE = 3,
};
enum DXGI_FORMAT {
    DXGI_FORMAT_UNKNOWN = 0,
    DXGI_FORMAT_R8G8B8A8_UNORM = 28,
    DXGI_FORMAT_B8G8R8A8_UNORM = 87,
};
struct D2D1_PIXEL_FORMAT { DXGI_FORMAT format; D2D1_ALPHA_MODE alphaMode; };

enum D2D1_FACTORY_TYPE { D2D1_FACTORY_TYPE_SINGLE_THREADED, D2D1_FACTORY_TYPE_MULTI_THREADED };
enum D2D1_DEBUG_LEVEL { D2D1_DEBUG_LEVEL_NONE, D2D1_DEBUG_LEVEL_ERROR, D2D1_DEBUG_LEVEL_WARNING, D2D1_DEBUG_LEVEL_INFORMATION };
struct D2D1_FACTORY_OPTIONS { D2D1_DEBUG_LEVEL debugLevel; };

enum D2D1_RENDER_TARGET_TYPE {
    D2D1_RENDER_TARGET_TYPE_DEFAULT,
    D2D1_RENDER_TARGET_TYPE_SOFTWARE,
    D2D1_RENDER_TARGET_TYPE_HARDWARE,
};
enum D2D1_RENDER_TARGET_USAGE {
    D2D1_RENDER_TARGET_USAGE_NONE = 0,
    D2D1_RENDER_TARGET_USAGE_FORCE_BITMAP_REMOTING = 1,
    D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE = 2,
};
enum D2D1_FEATURE_LEVEL { D2D1_FEATURE_LEVEL_DEFAULT = 0, D2D1_FEATURE_LEVEL_9 = 0x9100, D2D1_FEATURE_LEVEL_10 = 0xa000 };
struct D2D1_RENDER_TARGET_PROPERTIES {
    D2D1_RENDER_TARGET_TYPE type;
    D2D1_PIXEL_FORMAT pixelFormat;
    FLOAT dpiX;
    FLOAT dpiY;
    D2D1_RENDER_TARGET_USAGE usage;
    D2D1_FEATURE_LEVEL minLevel;
};
enum D2D1_PRESENT_OPTIONS {
    D2D1_PRESENT_OPTIONS_NONE = 0,
    D2D1_PRESENT_OPTIONS_RETAIN_CONTENTS = 1,
    D2D1_PRESENT_OPTIONS_IMMEDIATELY = 2,
};
struct D2D1_HWND_RENDER_TARGET_PROPERTIES {
    HWND hwnd;
    D2D1_SIZE_U pixelSize;
    D2D1_PRESENT_OPTIONS presentOptions;
};
enum D2D1_WINDOW_STATE { D2D1_WINDOW_STATE_NONE = 0, D2D1_WINDOW_STATE_OCCLUDED = 1 };

enum D2D1_ANTIALIAS_MODE { D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1_ANTIALIAS_MODE_ALIASED };
enum D2D1_TEXT_ANTIALIAS_MODE {
    D2D1_TEXT_ANTIALIAS_MODE_DEFAULT,
    D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE,
    D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE,
    D2D1_TEXT_ANTIALIAS_MODE_ALIASED,
};
enum D2D1_DRAW_TEXT_OPTIONS {
    D2D1_DRAW_TEXT_OPTIONS_NO_SNAP = 0x1,
    D2D1_DRAW_TEXT_OPTIONS_CLIP = 0x2,
    D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT = 0x4,
    D2D1_DRAW_TEXT_OPTIONS_DISABLE_COLOR_BITMAP_SNAPPING = 0x8,
    D2D1_DRAW_TEXT_OPTIONS_NONE = 0x0,
};
inline D2D1_DRAW_TEXT_OPTIONS operator|(D2D1_DRAW_TEXT_OPTIONS a, D2D1_DRAW_TEXT_OPTIONS b) {
    return static_cast<D2D1_DRAW_TEXT_OPTIONS>(static_cast<int>(a) | static_cast<int>(b));
}
enum D2D1_BITMAP_INTERPOLATION_MODE {
    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
};
enum D2D1_INTERPOLATION_MODE {
    D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
    D2D1_INTERPOLATION_MODE_LINEAR,
    D2D1_INTERPOLATION_MODE_CUBIC,
    D2D1_INTERPOLATION_MODE_MULTI_SAMPLE_LINEAR,
    D2D1_INTERPOLATION_MODE_ANISOTROPIC,
    D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC,
};
enum D2D1_COMPOSITE_MODE { D2D1_COMPOSITE_MODE_SOURCE_OVER = 0 };

enum D2D1_CAP_STYLE { D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_SQUARE, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_TRIANGLE };
enum D2D1_LINE_JOIN { D2D1_LINE_JOIN_MITER, D2D1_LINE_JOIN_BEVEL, D2D1_LINE_JOIN_ROUND, D2D1_LINE_JOIN_MITER_OR_BEVEL };
enum D2D1_DASH_STYLE {
    D2D1_DASH_STYLE_SOLID,
    D2D1_DASH_STYLE_DASH,
    D2D1_DASH_STYLE_DOT,
    D2D1_DASH_STYLE_DASH_DOT,
    D2D1_DASH_STYLE_DASH_DOT_DOT,
    D2D1_DASH_STYLE_CUSTOM,
};
struct D2D1_STROKE_STYLE_PROPERTIES {
    D2D1_CAP_STYLE startCap;
    D2D1_CAP_STYLE endCap;
    D2D1_CAP_STYLE dashCap;
    D2D1_LINE_JOIN lineJoin;
    FLOAT miterLimit;
    D2D1_DASH_STYLE dashStyle;
    FLOAT dashOffset;
};

enum D2D1_FIGURE_BEGIN { D2D1_FIGURE_BEGIN_FILLED, D2D1_FIGURE_BEGIN_HOLLOW };
enum D2D1_FIGURE_END { D2D1_FIGURE_END_OPEN, D2D1_FIGURE_END_CLOSED };
enum D2D1_FILL_MODE { D2D1_FILL_MODE_ALTERNATE, D2D1_FILL_MODE_WINDING };
enum D2D1_PATH_SEGMENT { D2D1_PATH_SEGMENT_NONE = 0 };
enum D2D1_SWEEP_DIRECTION { D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_SWEEP_DIRECTION_CLOCKWISE };
enum D2D1_ARC_SIZE { D2D1_ARC_SIZE_SMALL, D2D1_ARC_SIZE_LARGE };
struct D2D1_ARC_SEGMENT {
    D2D1_POINT_2F point;
    D2D1_SIZE_F size;
    FLOAT rotationAngle;
    D2D1_SWEEP_DIRECTION sweepDirection;
    D2D1_ARC_SIZE arcSize;
};
struct D2D1_BEZIER_SEGMENT { D2D1_POINT_2F point1; D2D1_POINT_2F point2; D2D1_POINT_2F point3; };
struct D2D1_QUADRATIC_BEZIER_SEGMENT { D2D1_POINT_2F point1; D2D1_POINT_2F point2; };

struct D2D1_BRUSH_PROPERTIES { FLOAT opacity; D2D1_MATRIX_3X2_F transform; };

struct D2D1_BITMAP_PROPERTIES { D2D1_PIXEL_FORMAT pixelFormat; FLOAT dpiX; FLOAT dpiY; };
enum D2D1_BITMAP_OPTIONS {
    D2D1_BITMAP_OPTIONS_NONE = 0x0,
    D2D1_BITMAP_OPTIONS_TARGET = 0x1,
    D2D1_BITMAP_OPTIONS_CANNOT_DRAW = 0x2,
    D2D1_BITMAP_OPTIONS_CPU_READ = 0x4,
    D2D1_BITMAP_OPTIONS_GDI_COMPATIBLE = 0x8,
};
inline D2D1_BITMAP_OPTIONS operator|(D2D1_BITMAP_OPTIONS a, D2D1_BITMAP_OPTIONS b) {
    return static_cast<D2D1_BITMAP_OPTIONS>(static_cast<int>(a) | static_cast<int>(b));
}
struct ID2D1ColorContext;
struct D2D1_BITMAP_PROPERTIES1 {
    D2D1_PIXEL_FORMAT pixelFormat;
    FLOAT dpiX;
    FLOAT dpiY;
    D2D1_BITMAP_OPTIONS bitmapOptions;
    ID2D1ColorContext* colorContext;
};
enum D2D1_MAP_OPTIONS {
    D2D1_MAP_OPTIONS_NONE = 0,
    D2D1_MAP_OPTIONS_READ = 1,
    D2D1_MAP_OPTIONS_WRITE = 2,
    D2D1_MAP_OPTIONS_DISCARD = 4,
};
struct D2D1_MAPPED_RECT { UINT32 pitch; BYTE* bits; };

enum D2D1_DEVICE_CONTEXT_OPTIONS {
    D2D1_DEVICE_CONTEXT_OPTIONS_NONE = 0,
    D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS = 1,
};
enum D2D1_PRINT_FONT_SUBSET_MODE {
    D2D1_PRINT_FONT_SUBSET_MODE_DEFAULT,
    D2D1_PRINT_FONT_SUBSET_MODE_EACHPAGE,
    D2D1_PRINT_FONT_SUBSET_MODE_NONE,
};
enum D2D1_COLOR_SPACE { D2D1_COLOR_SPACE_CUSTOM, D2D1_COLOR_SPACE_SRGB, D2D1_COLOR_SPACE_SCRGB };
struct D2D1_PRINT_CONTROL_PROPERTIES {
    D2D1_PRINT_FONT_SUBSET_MODE fontSubset;
    FLOAT rasterDPI;
    D2D1_COLOR_SPACE colorSpace;
};

enum D2D1_SVG_ATTRIBUTE_POD_TYPE {
    D2D1_SVG_ATTRIBUTE_POD_TYPE_FLOAT = 0,
    D2D1_SVG_ATTRIBUTE_POD_TYPE_COLOR = 1,
    D2D1_SVG_ATTRIBUTE_POD_TYPE_VIEWBOX = 6,
};
struct D2D1_SVG_VIEWBOX { FLOAT x; FLOAT y; FLOAT width; FLOAT height; };

// ---- Helpers (d2d1helper.h) ---------------------------------------------------
namespace D2D1 {

inline D2D1_POINT_2F Point2F(FLOAT x = 0.0f, FLOAT y = 0.0f) { return {x, y}; }
inline D2D1_POINT_2U Point2U(UINT32 x = 0, UINT32 y = 0) { return {x, y}; }
inline D2D1_SIZE_F SizeF(FLOAT width = 0.0f, FLOAT height = 0.0f) { return {width, height}; }
inline D2D1_SIZE_U SizeU(UINT32 width = 0, UINT32 height = 0) { return {width, height}; }
inline D2D1_RECT_F RectF(FLOAT left = 0.0f, FLOAT top = 0.0f, FLOAT right = 0.0f, FLOAT bottom = 0.0f) {
    return {left, top, right, bottom};
}
inline D2D1_RECT_U RectU(UINT32 left = 0, UINT32 top = 0, UINT32 right = 0, UINT32 bottom = 0) {
    return {left, top, right, bottom};
}
inline D2D1_RECT_F InfiniteRect() { return {-3.4e38f, -3.4e38f, 3.4e38f, 3.4e38f}; }
inline D2D1_ROUNDED_RECT RoundedRect(const D2D1_RECT_F& rect, FLOAT radiusX, FLOAT radiusY) {
    return {rect, radiusX, radiusY};
}
inline D2D1_ELLIPSE Ellipse(const D2D1_POINT_2F& center, FLOAT radiusX, FLOAT radiusY) {
    return {center, radiusX, radiusY};
}
inline D2D1_ARC_SEGMENT ArcSegment(const D2D1_POINT_2F& point, const D2D1_SIZE_F& size, FLOAT rotationAngle,
                                   D2D1_SWEEP_DIRECTION sweepDirection, D2D1_ARC_SIZE arcSize) {
    return {point, size, rotationAngle, sweepDirection, arcSize};
}
inline D2D1_BEZIER_SEGMENT BezierSegment(const D2D1_POINT_2F& a, const D2D1_POINT_2F& b, const D2D1_POINT_2F& c) {
    return {a, b, c};
}
inline D2D1_PIXEL_FORMAT PixelFormat(DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
                                     D2D1_ALPHA_MODE alphaMode = D2D1_ALPHA_MODE_UNKNOWN) {
    return {format, alphaMode};
}
inline D2D1_RENDER_TARGET_PROPERTIES RenderTargetProperties(
    D2D1_RENDER_TARGET_TYPE type = D2D1_RENDER_TARGET_TYPE_DEFAULT,
    const D2D1_PIXEL_FORMAT& pixelFormat = PixelFormat(), FLOAT dpiX = 0.0f, FLOAT dpiY = 0.0f,
    D2D1_RENDER_TARGET_USAGE usage = D2D1_RENDER_TARGET_USAGE_NONE,
    D2D1_FEATURE_LEVEL minLevel = D2D1_FEATURE_LEVEL_DEFAULT) {
    return {type, pixelFormat, dpiX, dpiY, usage, minLevel};
}
inline D2D1_HWND_RENDER_TARGET_PROPERTIES HwndRenderTargetProperties(
    HWND hwnd, D2D1_SIZE_U pixelSize = SizeU(0, 0),
    D2D1_PRESENT_OPTIONS presentOptions = D2D1_PRESENT_OPTIONS_NONE) {
    return {hwnd, pixelSize, presentOptions};
}
inline D2D1_BITMAP_PROPERTIES BitmapProperties(const D2D1_PIXEL_FORMAT& pixelFormat = PixelFormat(),
                                               FLOAT dpiX = 96.0f, FLOAT dpiY = 96.0f) {
    return {pixelFormat, dpiX, dpiY};
}
inline D2D1_BITMAP_PROPERTIES1 BitmapProperties1(D2D1_BITMAP_OPTIONS bitmapOptions = D2D1_BITMAP_OPTIONS_NONE,
                                                 const D2D1_PIXEL_FORMAT& pixelFormat = PixelFormat(),
                                                 FLOAT dpiX = 96.0f, FLOAT dpiY = 96.0f,
                                                 ID2D1ColorContext* colorContext = nullptr) {
    return {pixelFormat, dpiX, dpiY, bitmapOptions, colorContext};
}
inline D2D1_STROKE_STYLE_PROPERTIES StrokeStyleProperties(
    D2D1_CAP_STYLE startCap = D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE endCap = D2D1_CAP_STYLE_FLAT,
    D2D1_CAP_STYLE dashCap = D2D1_CAP_STYLE_FLAT, D2D1_LINE_JOIN lineJoin = D2D1_LINE_JOIN_MITER,
    FLOAT miterLimit = 10.0f, D2D1_DASH_STYLE dashStyle = D2D1_DASH_STYLE_SOLID, FLOAT dashOffset = 0.0f) {
    return {startCap, endCap, dashCap, lineJoin, miterLimit, dashStyle, dashOffset};
}

class ColorF : public D2D1_COLOR_F {
public:
    enum Enum {
        Black = 0x000000,
        White = 0xFFFFFF,
        Red = 0xFF0000,
        Green = 0x008000,
        Blue = 0x0000FF,
        Gray = 0x808080,
        LightGray = 0xD3D3D3,
        DarkGray = 0xA9A9A9,
        Yellow = 0xFFFF00,
        Orange = 0xFFA500,
        CornflowerBlue = 0x6495ED,
    };
    ColorF(UINT32 rgb, FLOAT alpha = 1.0f) {
        r = static_cast<FLOAT>((rgb >> 16) & 0xff) / 255.0f;
        g = static_cast<FLOAT>((rgb >> 8) & 0xff) / 255.0f;
        b = static_cast<FLOAT>(rgb & 0xff) / 255.0f;
        a = alpha;
    }
    ColorF(Enum known, FLOAT alpha = 1.0f) : ColorF(static_cast<UINT32>(known), alpha) {}
    ColorF(FLOAT red, FLOAT green, FLOAT blue, FLOAT alpha = 1.0f) {
        r = red;
        g = green;
        b = blue;
        a = alpha;
    }
};

class Matrix3x2F : public D2D1_MATRIX_3X2_F {
public:
    Matrix3x2F() { *this = Identity(); }
    Matrix3x2F(FLOAT m11, FLOAT m12, FLOAT m21, FLOAT m22, FLOAT m31, FLOAT m32) {
        _11 = m11; _12 = m12; _21 = m21; _22 = m22; _31 = m31; _32 = m32;
    }
    Matrix3x2F(const D2D1_MATRIX_3X2_F& m) : D2D1_MATRIX_3X2_F(m) {}

    static Matrix3x2F Identity() { return Matrix3x2F(1, 0, 0, 1, 0, 0); }
    static Matrix3x2F Translation(FLOAT x, FLOAT y) { return Matrix3x2F(1, 0, 0, 1, x, y); }
    static Matrix3x2F Translation(D2D1_SIZE_F size) { return Translation(size.width, size.height); }
    static Matrix3x2F Scale(FLOAT x, FLOAT y, D2D1_POINT_2F center = Point2F()) {
        return Matrix3x2F(x, 0, 0, y, center.x - x * center.x, center.y - y * center.y);
    }
    static Matrix3x2F Scale(D2D1_SIZE_F size, D2D1_POINT_2F center = Point2F()) {
        return Scale(size.width, size.height, center);
    }
    static Matrix3x2F Rotation(FLOAT angle, D2D1_POINT_2F center = Point2F()) {
        const FLOAT radians = angle * 3.14159265358979323846f / 180.0f;
        const FLOAT c = std::cos(radians), s = std::sin(radians);
        return Matrix3x2F(c, s, -s, c, center.x - c * center.x + s * center.y,
                          center.y - s * center.x - c * center.y);
    }
    static const Matrix3x2F* ReinterpretBaseType(const D2D1_MATRIX_3X2_F* m) {
        return static_cast<const Matrix3x2F*>(m);
    }

    FLOAT Determinant() const { return _11 * _22 - _12 * _21; }
    bool IsInvertible() const { return Determinant() != 0.0f; }
    bool Invert() {
        FLOAT det = Determinant();
        if (det == 0.0f) return false;
        Matrix3x2F inv(_22 / det, -_12 / det, -_21 / det, _11 / det,
                       (_21 * _32 - _22 * _31) / det, (_12 * _31 - _11 * _32) / det);
        *this = inv;
        return true;
    }
    bool IsIdentity() const { return _11 == 1 && _12 == 0 && _21 == 0 && _22 == 1 && _31 == 0 && _32 == 0; }
    void SetProduct(const Matrix3x2F& a, const Matrix3x2F& b) {
        *this = Matrix3x2F(a._11 * b._11 + a._12 * b._21, a._11 * b._12 + a._12 * b._22,
                           a._21 * b._11 + a._22 * b._21, a._21 * b._12 + a._22 * b._22,
                           a._31 * b._11 + a._32 * b._21 + b._31, a._31 * b._12 + a._32 * b._22 + b._32);
    }
    D2D1_POINT_2F TransformPoint(D2D1_POINT_2F p) const {
        return {p.x * _11 + p.y * _21 + _31, p.x * _12 + p.y * _22 + _32};
    }
    Matrix3x2F operator*(const Matrix3x2F& other) const {
        Matrix3x2F result;
        result.SetProduct(*this, other);
        return result;
    }
};

}  // namespace D2D1

inline D2D1_MATRIX_3X2_F operator*(const D2D1_MATRIX_3X2_F& a, const D2D1_MATRIX_3X2_F& b) {
    return D2D1::Matrix3x2F(a) * D2D1::Matrix3x2F(b);
}

// ---- Interfaces ---------------------------------------------------------------
TINTA_UUID(ID2D1Factory, 0x0d200001)
TINTA_UUID(ID2D1Resource, 0x0d200002)
TINTA_UUID(ID2D1Image, 0x0d200003)
TINTA_UUID(ID2D1Bitmap, 0x0d200004)
TINTA_UUID(ID2D1Bitmap1, 0x0d200005)
TINTA_UUID(ID2D1Brush, 0x0d200006)
TINTA_UUID(ID2D1SolidColorBrush, 0x0d200007)
TINTA_UUID(ID2D1StrokeStyle, 0x0d200008)
TINTA_UUID(ID2D1Geometry, 0x0d200009)
TINTA_UUID(ID2D1PathGeometry, 0x0d20000a)
TINTA_UUID(ID2D1SimplifiedGeometrySink, 0x0d20000b)
TINTA_UUID(ID2D1GeometrySink, 0x0d20000c)
TINTA_UUID(ID2D1RenderTarget, 0x0d20000d)
TINTA_UUID(ID2D1DeviceContext, 0x0d20000e)
TINTA_UUID(ID2D1DeviceContext1, 0x0d20000f)
TINTA_UUID(ID2D1DeviceContext2, 0x0d200010)
TINTA_UUID(ID2D1DeviceContext3, 0x0d200011)
TINTA_UUID(ID2D1DeviceContext4, 0x0d200012)
TINTA_UUID(ID2D1DeviceContext5, 0x0d200013)
TINTA_UUID(ID2D1HwndRenderTarget, 0x0d200014)
TINTA_UUID(ID2D1CommandList, 0x0d200015)
TINTA_UUID(ID2D1Device, 0x0d200016)
TINTA_UUID(ID2D1PrintControl, 0x0d200017)
TINTA_UUID(ID2D1SvgDocument, 0x0d200018)
TINTA_UUID(ID2D1SvgElement, 0x0d200019)
TINTA_UUID(ID2D1Factory1, 0x0d20001a)

struct ID2D1Resource : IUnknown {
    virtual void GetFactory(ID2D1Factory** factory) = 0;
};

struct ID2D1Image : ID2D1Resource {};

struct ID2D1Bitmap : ID2D1Image {
    virtual D2D1_SIZE_F GetSize() = 0;
    virtual D2D1_SIZE_U GetPixelSize() = 0;
    virtual D2D1_PIXEL_FORMAT GetPixelFormat() = 0;
    virtual void GetDpi(FLOAT* dpiX, FLOAT* dpiY) = 0;
    virtual HRESULT CopyFromBitmap(const D2D1_POINT_2U* destPoint, ID2D1Bitmap* bitmap, const D2D1_RECT_U* srcRect) = 0;
    virtual HRESULT CopyFromMemory(const D2D1_RECT_U* dstRect, const void* srcData, UINT32 pitch) = 0;
};

struct ID2D1Bitmap1 : ID2D1Bitmap {
    virtual D2D1_BITMAP_OPTIONS GetOptions() = 0;
    virtual HRESULT Map(D2D1_MAP_OPTIONS options, D2D1_MAPPED_RECT* mappedRect) = 0;
    virtual HRESULT Unmap() = 0;
};

struct ID2D1Brush : ID2D1Resource {
    virtual void SetOpacity(FLOAT opacity) = 0;
    virtual FLOAT GetOpacity() = 0;
};

struct ID2D1SolidColorBrush : ID2D1Brush {
    virtual void SetColor(const D2D1_COLOR_F* color) = 0;
    void SetColor(const D2D1_COLOR_F& color) { SetColor(&color); }
    virtual D2D1_COLOR_F GetColor() = 0;
};

struct ID2D1StrokeStyle : ID2D1Resource {
    virtual D2D1_CAP_STYLE GetStartCap() = 0;
    virtual D2D1_DASH_STYLE GetDashStyle() = 0;
};

struct ID2D1Geometry : ID2D1Resource {
    virtual HRESULT GetBounds(const D2D1_MATRIX_3X2_F* worldTransform, D2D1_RECT_F* bounds) = 0;
};

struct ID2D1SimplifiedGeometrySink : IUnknown {
    virtual void SetFillMode(D2D1_FILL_MODE fillMode) = 0;
    virtual void SetSegmentFlags(D2D1_PATH_SEGMENT vertexFlags) = 0;
    virtual void BeginFigure(D2D1_POINT_2F startPoint, D2D1_FIGURE_BEGIN figureBegin) = 0;
    virtual void AddLines(const D2D1_POINT_2F* points, UINT32 pointsCount) = 0;
    virtual void AddBeziers(const D2D1_BEZIER_SEGMENT* beziers, UINT32 beziersCount) = 0;
    virtual void EndFigure(D2D1_FIGURE_END figureEnd) = 0;
    virtual HRESULT Close() = 0;
};

struct ID2D1GeometrySink : ID2D1SimplifiedGeometrySink {
    virtual void AddLine(D2D1_POINT_2F point) = 0;
    virtual void AddBezier(const D2D1_BEZIER_SEGMENT* bezier) = 0;
    void AddBezier(const D2D1_BEZIER_SEGMENT& bezier) { AddBezier(&bezier); }
    virtual void AddQuadraticBezier(const D2D1_QUADRATIC_BEZIER_SEGMENT* bezier) = 0;
    virtual void AddArc(const D2D1_ARC_SEGMENT* arc) = 0;
    void AddArc(const D2D1_ARC_SEGMENT& arc) { AddArc(&arc); }
};

struct ID2D1PathGeometry : ID2D1Geometry {
    virtual HRESULT Open(ID2D1GeometrySink** geometrySink) = 0;
};

struct ID2D1CommandList : ID2D1Image {
    virtual HRESULT Close() = 0;
};

struct ID2D1RenderTarget : ID2D1Resource {
    virtual HRESULT CreateBitmap(D2D1_SIZE_U size, const void* srcData, UINT32 pitch,
                                 const D2D1_BITMAP_PROPERTIES* bitmapProperties, ID2D1Bitmap** bitmap) = 0;
    HRESULT CreateBitmap(D2D1_SIZE_U size, const void* srcData, UINT32 pitch,
                         const D2D1_BITMAP_PROPERTIES& bitmapProperties, ID2D1Bitmap** bitmap) {
        return CreateBitmap(size, srcData, pitch, &bitmapProperties, bitmap);
    }
    HRESULT CreateBitmap(D2D1_SIZE_U size, const D2D1_BITMAP_PROPERTIES& bitmapProperties, ID2D1Bitmap** bitmap) {
        return CreateBitmap(size, nullptr, 0, &bitmapProperties, bitmap);
    }
    virtual HRESULT CreateBitmapFromWicBitmap(IWICBitmapSource* wicBitmapSource,
                                              const D2D1_BITMAP_PROPERTIES* bitmapProperties,
                                              ID2D1Bitmap** bitmap) = 0;
    HRESULT CreateBitmapFromWicBitmap(IWICBitmapSource* wicBitmapSource, const D2D1_BITMAP_PROPERTIES& props,
                                      ID2D1Bitmap** bitmap) {
        return CreateBitmapFromWicBitmap(wicBitmapSource, &props, bitmap);
    }
    HRESULT CreateBitmapFromWicBitmap(IWICBitmapSource* wicBitmapSource, ID2D1Bitmap** bitmap) {
        return CreateBitmapFromWicBitmap(wicBitmapSource, nullptr, bitmap);
    }
    virtual HRESULT CreateSolidColorBrush(const D2D1_COLOR_F* color, const D2D1_BRUSH_PROPERTIES* brushProperties,
                                          ID2D1SolidColorBrush** solidColorBrush) = 0;
    HRESULT CreateSolidColorBrush(const D2D1_COLOR_F& color, ID2D1SolidColorBrush** brush) {
        return CreateSolidColorBrush(&color, nullptr, brush);
    }
    HRESULT CreateSolidColorBrush(const D2D1_COLOR_F& color, const D2D1_BRUSH_PROPERTIES& props,
                                  ID2D1SolidColorBrush** brush) {
        return CreateSolidColorBrush(&color, &props, brush);
    }

    virtual void DrawLine(D2D1_POINT_2F point0, D2D1_POINT_2F point1, ID2D1Brush* brush,
                          FLOAT strokeWidth = 1.0f, ID2D1StrokeStyle* strokeStyle = nullptr) = 0;
    virtual void DrawRectangle(const D2D1_RECT_F* rect, ID2D1Brush* brush, FLOAT strokeWidth = 1.0f,
                               ID2D1StrokeStyle* strokeStyle = nullptr) = 0;
    void DrawRectangle(const D2D1_RECT_F& rect, ID2D1Brush* brush, FLOAT strokeWidth = 1.0f,
                       ID2D1StrokeStyle* strokeStyle = nullptr) {
        DrawRectangle(&rect, brush, strokeWidth, strokeStyle);
    }
    virtual void FillRectangle(const D2D1_RECT_F* rect, ID2D1Brush* brush) = 0;
    void FillRectangle(const D2D1_RECT_F& rect, ID2D1Brush* brush) { FillRectangle(&rect, brush); }
    virtual void DrawRoundedRectangle(const D2D1_ROUNDED_RECT* roundedRect, ID2D1Brush* brush,
                                      FLOAT strokeWidth = 1.0f, ID2D1StrokeStyle* strokeStyle = nullptr) = 0;
    void DrawRoundedRectangle(const D2D1_ROUNDED_RECT& roundedRect, ID2D1Brush* brush, FLOAT strokeWidth = 1.0f,
                              ID2D1StrokeStyle* strokeStyle = nullptr) {
        DrawRoundedRectangle(&roundedRect, brush, strokeWidth, strokeStyle);
    }
    virtual void FillRoundedRectangle(const D2D1_ROUNDED_RECT* roundedRect, ID2D1Brush* brush) = 0;
    void FillRoundedRectangle(const D2D1_ROUNDED_RECT& roundedRect, ID2D1Brush* brush) {
        FillRoundedRectangle(&roundedRect, brush);
    }
    virtual void DrawEllipse(const D2D1_ELLIPSE* ellipse, ID2D1Brush* brush, FLOAT strokeWidth = 1.0f,
                             ID2D1StrokeStyle* strokeStyle = nullptr) = 0;
    void DrawEllipse(const D2D1_ELLIPSE& ellipse, ID2D1Brush* brush, FLOAT strokeWidth = 1.0f,
                     ID2D1StrokeStyle* strokeStyle = nullptr) {
        DrawEllipse(&ellipse, brush, strokeWidth, strokeStyle);
    }
    virtual void FillEllipse(const D2D1_ELLIPSE* ellipse, ID2D1Brush* brush) = 0;
    void FillEllipse(const D2D1_ELLIPSE& ellipse, ID2D1Brush* brush) { FillEllipse(&ellipse, brush); }
    virtual void DrawGeometry(ID2D1Geometry* geometry, ID2D1Brush* brush, FLOAT strokeWidth = 1.0f,
                              ID2D1StrokeStyle* strokeStyle = nullptr) = 0;
    virtual void FillGeometry(ID2D1Geometry* geometry, ID2D1Brush* brush, ID2D1Brush* opacityBrush = nullptr) = 0;
    virtual void DrawBitmap(ID2D1Bitmap* bitmap, const D2D1_RECT_F* destinationRectangle = nullptr,
                            FLOAT opacity = 1.0f,
                            D2D1_BITMAP_INTERPOLATION_MODE interpolationMode = D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                            const D2D1_RECT_F* sourceRectangle = nullptr) = 0;
    void DrawBitmap(ID2D1Bitmap* bitmap, const D2D1_RECT_F& destinationRectangle, FLOAT opacity = 1.0f,
                    D2D1_BITMAP_INTERPOLATION_MODE interpolationMode = D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                    const D2D1_RECT_F* sourceRectangle = nullptr) {
        DrawBitmap(bitmap, &destinationRectangle, opacity, interpolationMode, sourceRectangle);
    }
    void DrawBitmap(ID2D1Bitmap* bitmap, const D2D1_RECT_F& destinationRectangle, FLOAT opacity,
                    D2D1_BITMAP_INTERPOLATION_MODE interpolationMode, const D2D1_RECT_F& sourceRectangle) {
        DrawBitmap(bitmap, &destinationRectangle, opacity, interpolationMode, &sourceRectangle);
    }
    virtual void DrawText(const WCHAR* string, UINT32 stringLength, IDWriteTextFormat* textFormat,
                          const D2D1_RECT_F* layoutRect, ID2D1Brush* defaultFillBrush,
                          D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE,
                          DWRITE_MEASURING_MODE measuringMode = DWRITE_MEASURING_MODE_NATURAL) = 0;
    void DrawText(const WCHAR* string, UINT32 stringLength, IDWriteTextFormat* textFormat,
                  const D2D1_RECT_F& layoutRect, ID2D1Brush* defaultFillBrush,
                  D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE,
                  DWRITE_MEASURING_MODE measuringMode = DWRITE_MEASURING_MODE_NATURAL) {
        DrawText(string, stringLength, textFormat, &layoutRect, defaultFillBrush, options, measuringMode);
    }
    // On Windows <winuser.h> turns DrawText into DrawTextW, method included
    void DrawTextW(const WCHAR* string, UINT32 stringLength, IDWriteTextFormat* textFormat,
                   const D2D1_RECT_F* layoutRect, ID2D1Brush* defaultFillBrush,
                   D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE,
                   DWRITE_MEASURING_MODE measuringMode = DWRITE_MEASURING_MODE_NATURAL) {
        DrawText(string, stringLength, textFormat, layoutRect, defaultFillBrush, options, measuringMode);
    }
    void DrawTextW(const WCHAR* string, UINT32 stringLength, IDWriteTextFormat* textFormat,
                   const D2D1_RECT_F& layoutRect, ID2D1Brush* defaultFillBrush,
                   D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE,
                   DWRITE_MEASURING_MODE measuringMode = DWRITE_MEASURING_MODE_NATURAL) {
        DrawText(string, stringLength, textFormat, &layoutRect, defaultFillBrush, options, measuringMode);
    }
    virtual void DrawTextLayout(D2D1_POINT_2F origin, IDWriteTextLayout* textLayout, ID2D1Brush* defaultFillBrush,
                                D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE) = 0;

    virtual void SetTransform(const D2D1_MATRIX_3X2_F* transform) = 0;
    void SetTransform(const D2D1_MATRIX_3X2_F& transform) { SetTransform(&transform); }
    virtual void GetTransform(D2D1_MATRIX_3X2_F* transform) = 0;
    virtual void SetAntialiasMode(D2D1_ANTIALIAS_MODE antialiasMode) = 0;
    virtual D2D1_ANTIALIAS_MODE GetAntialiasMode() = 0;
    virtual void SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE textAntialiasMode) = 0;
    virtual void SetTextRenderingParams(IDWriteRenderingParams* textRenderingParams = nullptr) = 0;
    virtual void PushAxisAlignedClip(const D2D1_RECT_F* clipRect, D2D1_ANTIALIAS_MODE antialiasMode) = 0;
    void PushAxisAlignedClip(const D2D1_RECT_F& clipRect, D2D1_ANTIALIAS_MODE antialiasMode) {
        PushAxisAlignedClip(&clipRect, antialiasMode);
    }
    virtual void PopAxisAlignedClip() = 0;
    virtual void Clear(const D2D1_COLOR_F* clearColor = nullptr) = 0;
    void Clear(const D2D1_COLOR_F& clearColor) { Clear(&clearColor); }
    virtual void BeginDraw() = 0;
    virtual HRESULT EndDraw(D2D1_TAG* tag1 = nullptr, D2D1_TAG* tag2 = nullptr) = 0;
    virtual HRESULT Flush(D2D1_TAG* tag1 = nullptr, D2D1_TAG* tag2 = nullptr) = 0;
    virtual D2D1_PIXEL_FORMAT GetPixelFormat() = 0;
    virtual void SetDpi(FLOAT dpiX, FLOAT dpiY) = 0;
    virtual void GetDpi(FLOAT* dpiX, FLOAT* dpiY) = 0;
    virtual D2D1_SIZE_F GetSize() = 0;
    virtual D2D1_SIZE_U GetPixelSize() = 0;
    virtual UINT32 GetMaximumBitmapSize() = 0;
};

struct ID2D1Device;
struct ID2D1SvgDocument;

struct ID2D1DeviceContext : ID2D1RenderTarget {
    using ID2D1RenderTarget::CreateBitmap;
    using ID2D1RenderTarget::CreateBitmapFromWicBitmap;
    virtual HRESULT CreateBitmap(D2D1_SIZE_U size, const void* sourceData, UINT32 pitch,
                                 const D2D1_BITMAP_PROPERTIES1* bitmapProperties, ID2D1Bitmap1** bitmap) = 0;
    HRESULT CreateBitmap(D2D1_SIZE_U size, const void* sourceData, UINT32 pitch,
                         const D2D1_BITMAP_PROPERTIES1& bitmapProperties, ID2D1Bitmap1** bitmap) {
        return CreateBitmap(size, sourceData, pitch, &bitmapProperties, bitmap);
    }
    virtual HRESULT CreateBitmapFromWicBitmap(IWICBitmapSource* wicBitmapSource,
                                              const D2D1_BITMAP_PROPERTIES1* bitmapProperties,
                                              ID2D1Bitmap1** bitmap) = 0;
    HRESULT CreateBitmapFromWicBitmap(IWICBitmapSource* wicBitmapSource, const D2D1_BITMAP_PROPERTIES1& props,
                                      ID2D1Bitmap1** bitmap) {
        return CreateBitmapFromWicBitmap(wicBitmapSource, &props, bitmap);
    }
    virtual HRESULT CreateCommandList(ID2D1CommandList** commandList) = 0;
    virtual void GetDevice(ID2D1Device** device) = 0;
    virtual void SetTarget(ID2D1Image* image) = 0;
    virtual void GetTarget(ID2D1Image** image) = 0;
    virtual void DrawImage(ID2D1Image* image, const D2D1_POINT_2F* targetOffset = nullptr,
                           const D2D1_RECT_F* imageRectangle = nullptr,
                           D2D1_INTERPOLATION_MODE interpolationMode = D2D1_INTERPOLATION_MODE_LINEAR,
                           D2D1_COMPOSITE_MODE compositeMode = D2D1_COMPOSITE_MODE_SOURCE_OVER) = 0;
};
struct ID2D1DeviceContext1 : ID2D1DeviceContext {};
struct ID2D1DeviceContext2 : ID2D1DeviceContext1 {};
struct ID2D1DeviceContext3 : ID2D1DeviceContext2 {};
struct ID2D1DeviceContext4 : ID2D1DeviceContext3 {};
struct ID2D1DeviceContext5 : ID2D1DeviceContext4 {
    virtual HRESULT CreateSvgDocument(IStream* inputXmlStream, D2D1_SIZE_F viewportSize,
                                      ID2D1SvgDocument** svgDocument) = 0;
    virtual void DrawSvgDocument(ID2D1SvgDocument* svgDocument) = 0;
};

struct ID2D1HwndRenderTarget : ID2D1DeviceContext5 {
    virtual D2D1_WINDOW_STATE CheckWindowState() = 0;
    virtual HRESULT Resize(const D2D1_SIZE_U* pixelSize) = 0;
    HRESULT Resize(const D2D1_SIZE_U& pixelSize) { return Resize(&pixelSize); }
    virtual HWND GetHwnd() = 0;
};

struct ID2D1PrintControl : IUnknown {
    virtual HRESULT AddPage(ID2D1CommandList* commandList, D2D_SIZE_F pageSize, IStream* pagePrintTicketStream,
                            D2D1_TAG* tag1 = nullptr, D2D1_TAG* tag2 = nullptr) = 0;
    virtual HRESULT Close() = 0;
};

struct ID2D1Device : ID2D1Resource {
    virtual HRESULT CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS options,
                                        ID2D1DeviceContext** deviceContext) = 0;
    virtual HRESULT CreatePrintControl(IWICImagingFactory* wicFactory,
                                       IPrintDocumentPackageTarget* documentTarget,
                                       const D2D1_PRINT_CONTROL_PROPERTIES* printControlProperties,
                                       ID2D1PrintControl** printControl) = 0;
};

struct ID2D1SvgElement : ID2D1Resource {
    virtual HRESULT GetAttributeValue(const WCHAR* name, D2D1_SVG_ATTRIBUTE_POD_TYPE type, void* value,
                                      UINT32 valueSizeInBytes) = 0;
    HRESULT GetAttributeValue(const WCHAR* name, FLOAT* value) {
        return GetAttributeValue(name, D2D1_SVG_ATTRIBUTE_POD_TYPE_FLOAT, value, sizeof(FLOAT));
    }
    HRESULT GetAttributeValue(const WCHAR* name, D2D1_SVG_VIEWBOX* value) {
        return GetAttributeValue(name, D2D1_SVG_ATTRIBUTE_POD_TYPE_VIEWBOX, value, sizeof(D2D1_SVG_VIEWBOX));
    }
};

struct ID2D1SvgDocument : ID2D1Resource {
    virtual void GetRoot(ID2D1SvgElement** root) = 0;
    virtual HRESULT SetViewportSize(D2D1_SIZE_F viewportSize) = 0;
    virtual D2D1_SIZE_F GetViewportSize() = 0;
};

struct ID2D1Factory : IUnknown {
    virtual HRESULT ReloadSystemMetrics() { return S_OK; }
    virtual void GetDesktopDpi(FLOAT* dpiX, FLOAT* dpiY) = 0;
    virtual HRESULT CreatePathGeometry(ID2D1PathGeometry** pathGeometry) = 0;
    virtual HRESULT CreateStrokeStyle(const D2D1_STROKE_STYLE_PROPERTIES* strokeStyleProperties,
                                      const FLOAT* dashes, UINT32 dashesCount, ID2D1StrokeStyle** strokeStyle) = 0;
    HRESULT CreateStrokeStyle(const D2D1_STROKE_STYLE_PROPERTIES& props, const FLOAT* dashes,
                              UINT32 dashesCount, ID2D1StrokeStyle** strokeStyle) {
        return CreateStrokeStyle(&props, dashes, dashesCount, strokeStyle);
    }
    virtual HRESULT CreateWicBitmapRenderTarget(IWICBitmap* target,
                                                const D2D1_RENDER_TARGET_PROPERTIES* renderTargetProperties,
                                                ID2D1RenderTarget** renderTarget) = 0;
    HRESULT CreateWicBitmapRenderTarget(IWICBitmap* target, const D2D1_RENDER_TARGET_PROPERTIES& props,
                                        ID2D1RenderTarget** renderTarget) {
        return CreateWicBitmapRenderTarget(target, &props, renderTarget);
    }
    virtual HRESULT CreateHwndRenderTarget(const D2D1_RENDER_TARGET_PROPERTIES* renderTargetProperties,
                                           const D2D1_HWND_RENDER_TARGET_PROPERTIES* hwndProperties,
                                           ID2D1HwndRenderTarget** hwndRenderTarget) = 0;
    HRESULT CreateHwndRenderTarget(const D2D1_RENDER_TARGET_PROPERTIES& props,
                                   const D2D1_HWND_RENDER_TARGET_PROPERTIES& hwndProps,
                                   ID2D1HwndRenderTarget** hwndRenderTarget) {
        return CreateHwndRenderTarget(&props, &hwndProps, hwndRenderTarget);
    }
};

struct ID2D1Factory1 : ID2D1Factory {};

HRESULT D2D1CreateFactory(D2D1_FACTORY_TYPE factoryType, REFIID riid,
                          const D2D1_FACTORY_OPTIONS* factoryOptions, void** factory);
template <class Factory>
HRESULT D2D1CreateFactory(D2D1_FACTORY_TYPE factoryType, Factory** factory) {
    return D2D1CreateFactory(factoryType, __uuidof(Factory), nullptr, reinterpret_cast<void**>(factory));
}
template <class Factory>
HRESULT D2D1CreateFactory(D2D1_FACTORY_TYPE factoryType, const D2D1_FACTORY_OPTIONS& options, Factory** factory) {
    return D2D1CreateFactory(factoryType, __uuidof(Factory), &options, reinterpret_cast<void**>(factory));
}

#endif  // TINTA_LINUX_D2D_H
