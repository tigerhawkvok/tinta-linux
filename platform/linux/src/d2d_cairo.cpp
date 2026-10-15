// Direct2D on Cairo.
//
// One RenderTarget class backs every target interface (window, device
// context, WIC bitmap target). Its Cairo context draws into the current
// target: an image surface (windows, bitmaps), a recording surface (command
// lists, replayed later as vectors) or nothing. DIPs map to Cairo user units
// through the target DPI; Tinta renders at 96 DPI and scales itself.
#include "compat_internal.h"

#include <cairo-pdf.h>
#include <documenttarget.h>
#include <librsvg/rsvg.h>
#include <pango/pangocairo.h>
#include <wincodec.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

namespace tinta {
namespace {

void setColor(cairo_t* cr, const D2D1_COLOR_F& color, float opacity) {
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a * opacity);
}

// ---- Resources ---------------------------------------------------------------------
class Factory;
class Device;

class SolidColorBrush final : public TintaComObject<ID2D1SolidColorBrush> {
public:
    using Primary = ID2D1SolidColorBrush;
    SolidColorBrush(const D2D1_COLOR_F& color, float opacity) : color_(color), opacity_(opacity) {}
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<ID2D1SolidColorBrush, ID2D1Brush, ID2D1Resource>(this, riid, object);
    }
    void GetFactory(ID2D1Factory** factory) override { *factory = nullptr; }
    void SetOpacity(FLOAT opacity) override { opacity_ = opacity; }
    FLOAT GetOpacity() override { return opacity_; }
    void SetColor(const D2D1_COLOR_F* color) override { if (color) color_ = *color; }
    D2D1_COLOR_F GetColor() override { return color_; }
    void apply(cairo_t* cr) const { setColor(cr, color_, opacity_); }

private:
    D2D1_COLOR_F color_;
    float opacity_;
};

void applyBrush(cairo_t* cr, ID2D1Brush* brush) {
    if (auto* solid = dynamic_cast<SolidColorBrush*>(brush)) solid->apply(cr);
    else cairo_set_source_rgba(cr, 0, 0, 0, 1);
}

class StrokeStyle final : public TintaComObject<ID2D1StrokeStyle> {
public:
    using Primary = ID2D1StrokeStyle;
    StrokeStyle(const D2D1_STROKE_STYLE_PROPERTIES& props, const FLOAT* dashes, UINT32 count)
        : props_(props), dashes_(dashes, dashes + count) {}
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<ID2D1StrokeStyle, ID2D1Resource>(this, riid, object);
    }
    void GetFactory(ID2D1Factory** factory) override { *factory = nullptr; }
    D2D1_CAP_STYLE GetStartCap() override { return props_.startCap; }
    D2D1_DASH_STYLE GetDashStyle() override { return props_.dashStyle; }

    void apply(cairo_t* cr, float width) const {
        cairo_line_cap_t cap = CAIRO_LINE_CAP_BUTT;
        if (props_.startCap == D2D1_CAP_STYLE_ROUND) cap = CAIRO_LINE_CAP_ROUND;
        else if (props_.startCap == D2D1_CAP_STYLE_SQUARE) cap = CAIRO_LINE_CAP_SQUARE;
        cairo_set_line_cap(cr, cap);
        cairo_line_join_t join = CAIRO_LINE_JOIN_MITER;
        if (props_.lineJoin == D2D1_LINE_JOIN_ROUND) join = CAIRO_LINE_JOIN_ROUND;
        else if (props_.lineJoin == D2D1_LINE_JOIN_BEVEL) join = CAIRO_LINE_JOIN_BEVEL;
        cairo_set_line_join(cr, join);
        cairo_set_miter_limit(cr, props_.miterLimit > 0 ? props_.miterLimit : 10.0);
        // Dash lengths are multiples of the stroke width, as in Direct2D
        std::vector<double> pattern;
        switch (props_.dashStyle) {
            case D2D1_DASH_STYLE_DASH: pattern = {2, 2}; break;
            case D2D1_DASH_STYLE_DOT: pattern = {0, 2}; break;
            case D2D1_DASH_STYLE_DASH_DOT: pattern = {2, 2, 0, 2}; break;
            case D2D1_DASH_STYLE_DASH_DOT_DOT: pattern = {2, 2, 0, 2, 0, 2}; break;
            case D2D1_DASH_STYLE_CUSTOM: pattern.assign(dashes_.begin(), dashes_.end()); break;
            default: break;
        }
        for (double& d : pattern) d *= width;
        // Zero-length dots need a round or square cap to show
        if (props_.dashStyle == D2D1_DASH_STYLE_DOT && cap == CAIRO_LINE_CAP_BUTT) cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_dash(cr, pattern.empty() ? nullptr : pattern.data(), static_cast<int>(pattern.size()),
                       props_.dashOffset * width);
    }

private:
    D2D1_STROKE_STYLE_PROPERTIES props_;
    std::vector<float> dashes_;
};

void applyStroke(cairo_t* cr, float width, ID2D1StrokeStyle* style) {
    cairo_set_line_width(cr, width);
    if (auto* own = dynamic_cast<StrokeStyle*>(style)) {
        own->apply(cr, width);
    } else {
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
        cairo_set_dash(cr, nullptr, 0, 0);
    }
}

// ---- Path geometry -----------------------------------------------------------------
struct PathOp {
    enum Kind { Move, Line, Cubic, Arc, Close } kind;
    D2D1_POINT_2F p[3];
    D2D1_ARC_SEGMENT arc;
};

// SVG endpoint arc to Cairo arcs (F.6.5 of the SVG spec)
void appendArc(cairo_t* cr, D2D1_POINT_2F from, const D2D1_ARC_SEGMENT& arc) {
    double x1 = from.x, y1 = from.y, x2 = arc.point.x, y2 = arc.point.y;
    double rx = std::fabs(arc.size.width), ry = std::fabs(arc.size.height);
    if (rx < 1e-6 || ry < 1e-6 || (x1 == x2 && y1 == y2)) {
        cairo_line_to(cr, x2, y2);
        return;
    }
    double phi = arc.rotationAngle * M_PI / 180.0;
    double cp = std::cos(phi), sp = std::sin(phi);
    double dx = (x1 - x2) / 2, dy = (y1 - y2) / 2;
    double x1p = cp * dx + sp * dy, y1p = -sp * dx + cp * dy;
    double lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lambda > 1) {
        double s = std::sqrt(lambda);
        rx *= s;
        ry *= s;
    }
    bool large = arc.arcSize == D2D1_ARC_SIZE_LARGE;
    bool sweep = arc.sweepDirection == D2D1_SWEEP_DIRECTION_CLOCKWISE;
    double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
    double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
    double coef = den > 0 ? std::sqrt(std::max(0.0, num / den)) : 0.0;
    if (large == sweep) coef = -coef;
    double cxp = coef * rx * y1p / ry, cyp = -coef * ry * x1p / rx;
    double cx = cp * cxp - sp * cyp + (x1 + x2) / 2, cy = sp * cxp + cp * cyp + (y1 + y2) / 2;
    auto angle = [](double ux, double uy, double vx, double vy) {
        double a = std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
        return a;
    };
    double theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    double delta = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);
    if (!sweep && delta > 0) delta -= 2 * M_PI;
    else if (sweep && delta < 0) delta += 2 * M_PI;
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_rotate(cr, phi);
    cairo_scale(cr, rx, ry);
    if (sweep) cairo_arc(cr, 0, 0, 1, theta1, theta1 + delta);
    else cairo_arc_negative(cr, 0, 0, 1, theta1, theta1 + delta);
    cairo_restore(cr);
}

class PathGeometry;

class GeometrySink final : public TintaComObject<ID2D1GeometrySink> {
public:
    using Primary = ID2D1GeometrySink;
    explicit GeometrySink(PathGeometry* owner);
    ~GeometrySink() override;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<ID2D1GeometrySink, ID2D1SimplifiedGeometrySink>(this, riid, object);
    }
    void SetFillMode(D2D1_FILL_MODE mode) override;
    void SetSegmentFlags(D2D1_PATH_SEGMENT) override {}
    void BeginFigure(D2D1_POINT_2F start, D2D1_FIGURE_BEGIN) override { push({PathOp::Move, {start}, {}}); }
    void AddLines(const D2D1_POINT_2F* points, UINT32 count) override {
        for (UINT32 i = 0; i < count; i++) push({PathOp::Line, {points[i]}, {}});
    }
    void AddBeziers(const D2D1_BEZIER_SEGMENT* beziers, UINT32 count) override {
        for (UINT32 i = 0; i < count; i++) AddBezier(&beziers[i]);
    }
    void EndFigure(D2D1_FIGURE_END end) override {
        if (end == D2D1_FIGURE_END_CLOSED) push({PathOp::Close, {}, {}});
    }
    HRESULT Close() override { return S_OK; }
    void AddLine(D2D1_POINT_2F point) override { push({PathOp::Line, {point}, {}}); }
    void AddBezier(const D2D1_BEZIER_SEGMENT* b) override {
        push({PathOp::Cubic, {b->point1, b->point2, b->point3}, {}});
    }
    void AddQuadraticBezier(const D2D1_QUADRATIC_BEZIER_SEGMENT* q) override {
        // Exact cubic form of the quadratic needs the current point; keep the
        // control point and let the replay elevate it
        push({PathOp::Cubic, {q->point1, q->point1, q->point2}, {}});
    }
    void AddArc(const D2D1_ARC_SEGMENT* arc) override {
        PathOp op{PathOp::Arc, {}, *arc};
        push(op);
    }

private:
    void push(const PathOp& op);
    PathGeometry* owner_;
};

class PathGeometry final : public TintaComObject<ID2D1PathGeometry> {
public:
    using Primary = ID2D1PathGeometry;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<ID2D1PathGeometry, ID2D1Geometry, ID2D1Resource>(this, riid, object);
    }
    void GetFactory(ID2D1Factory** factory) override { *factory = nullptr; }
    HRESULT Open(ID2D1GeometrySink** sink) override {
        ops.clear();
        *sink = new GeometrySink(this);
        return S_OK;
    }
    HRESULT GetBounds(const D2D1_MATRIX_3X2_F* transform, D2D1_RECT_F* bounds) override {
        cairo_surface_t* scratch = cairo_recording_surface_create(CAIRO_CONTENT_ALPHA, nullptr);
        cairo_t* cr = cairo_create(scratch);
        if (transform) {
            cairo_matrix_t m;
            cairo_matrix_init(&m, transform->_11, transform->_12, transform->_21, transform->_22, transform->_31,
                              transform->_32);
            cairo_set_matrix(cr, &m);
        }
        replay(cr);
        double x1, y1, x2, y2;
        cairo_identity_matrix(cr);
        cairo_path_extents(cr, &x1, &y1, &x2, &y2);
        cairo_destroy(cr);
        cairo_surface_destroy(scratch);
        *bounds = D2D1::RectF(static_cast<float>(x1), static_cast<float>(y1), static_cast<float>(x2), static_cast<float>(y2));
        return S_OK;
    }

    void replay(cairo_t* cr) const {
        cairo_new_path(cr);
        cairo_set_fill_rule(cr, fillMode == D2D1_FILL_MODE_WINDING ? CAIRO_FILL_RULE_WINDING : CAIRO_FILL_RULE_EVEN_ODD);
        D2D1_POINT_2F current{0, 0}, start{0, 0};
        for (const PathOp& op : ops) {
            switch (op.kind) {
                case PathOp::Move:
                    cairo_move_to(cr, op.p[0].x, op.p[0].y);
                    current = start = op.p[0];
                    break;
                case PathOp::Line:
                    cairo_line_to(cr, op.p[0].x, op.p[0].y);
                    current = op.p[0];
                    break;
                case PathOp::Cubic:
                    if (op.p[0].x == op.p[1].x && op.p[0].y == op.p[1].y) {
                        // Quadratic: elevate to cubic from the current point
                        double c1x = current.x + 2.0 / 3.0 * (op.p[0].x - current.x);
                        double c1y = current.y + 2.0 / 3.0 * (op.p[0].y - current.y);
                        double c2x = op.p[2].x + 2.0 / 3.0 * (op.p[0].x - op.p[2].x);
                        double c2y = op.p[2].y + 2.0 / 3.0 * (op.p[0].y - op.p[2].y);
                        cairo_curve_to(cr, c1x, c1y, c2x, c2y, op.p[2].x, op.p[2].y);
                    } else {
                        cairo_curve_to(cr, op.p[0].x, op.p[0].y, op.p[1].x, op.p[1].y, op.p[2].x, op.p[2].y);
                    }
                    current = op.p[2];
                    break;
                case PathOp::Arc:
                    appendArc(cr, current, op.arc);
                    current = op.arc.point;
                    break;
                case PathOp::Close:
                    cairo_close_path(cr);
                    current = start;
                    break;
            }
        }
    }

    std::vector<PathOp> ops;
    D2D1_FILL_MODE fillMode = D2D1_FILL_MODE_ALTERNATE;
};

GeometrySink::GeometrySink(PathGeometry* owner) : owner_(owner) { owner_->AddRef(); }
GeometrySink::~GeometrySink() { owner_->Release(); }
void GeometrySink::SetFillMode(D2D1_FILL_MODE mode) { owner_->fillMode = mode; }
void GeometrySink::push(const PathOp& op) { owner_->ops.push_back(op); }

// ---- Images ----------------------------------------------------------------------
class Bitmap final : public TintaComObject<ID2D1Bitmap1> {
public:
    using Primary = ID2D1Bitmap1;
    Bitmap(int width, int height, float dpiX, float dpiY, D2D1_BITMAP_OPTIONS options)
        : surface_(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, std::max(1, width), std::max(1, height))),
          dpiX_(dpiX > 0 ? dpiX : 96.0f), dpiY_(dpiY > 0 ? dpiY : 96.0f), options_(options) {}
    ~Bitmap() override {
        invalidateLevels();
        cairo_surface_destroy(surface_);
    }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<ID2D1Bitmap1, ID2D1Bitmap, ID2D1Image, ID2D1Resource>(this, riid, object);
    }
    void GetFactory(ID2D1Factory** factory) override { *factory = nullptr; }
    D2D1_SIZE_F GetSize() override {
        D2D1_SIZE_U px = GetPixelSize();
        return D2D1::SizeF(px.width * 96.0f / dpiX_, px.height * 96.0f / dpiY_);
    }
    D2D1_SIZE_U GetPixelSize() override {
        return D2D1::SizeU(static_cast<UINT32>(cairo_image_surface_get_width(surface_)),
                           static_cast<UINT32>(cairo_image_surface_get_height(surface_)));
    }
    D2D1_PIXEL_FORMAT GetPixelFormat() override {
        return D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    }
    void GetDpi(FLOAT* dpiX, FLOAT* dpiY) override {
        *dpiX = dpiX_;
        *dpiY = dpiY_;
    }
    HRESULT CopyFromBitmap(const D2D1_POINT_2U* dest, ID2D1Bitmap* bitmap, const D2D1_RECT_U* src) override {
        auto* from = dynamic_cast<Bitmap*>(bitmap);
        if (!from) return E_INVALIDARG;
        cairo_surface_flush(from->surface_);
        D2D1_SIZE_U size = from->GetPixelSize();
        D2D1_RECT_U r = src ? *src : D2D1::RectU(0, 0, size.width, size.height);
        int dx = dest ? static_cast<int>(dest->x) : 0, dy = dest ? static_cast<int>(dest->y) : 0;
        cairo_t* cr = cairo_create(surface_);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(cr, from->surface_, dx - static_cast<double>(r.left), dy - static_cast<double>(r.top));
        cairo_rectangle(cr, dx, dy, r.right - r.left, r.bottom - r.top);
        cairo_fill(cr);
        cairo_destroy(cr);
        invalidateLevels();
        return S_OK;
    }
    HRESULT CopyFromMemory(const D2D1_RECT_U* dst, const void* data, UINT32 pitch) override {
        D2D1_SIZE_U size = GetPixelSize();
        D2D1_RECT_U r = dst ? *dst : D2D1::RectU(0, 0, size.width, size.height);
        cairo_surface_flush(surface_);
        unsigned char* pixels = cairo_image_surface_get_data(surface_);
        int stride = cairo_image_surface_get_stride(surface_);
        const auto* in = static_cast<const unsigned char*>(data);
        for (UINT32 y = r.top; y < r.bottom && y < size.height; y++) {
            std::memcpy(pixels + y * stride + r.left * 4, in + (y - r.top) * pitch,
                        std::min<size_t>(pitch, (r.right - r.left) * 4));
        }
        cairo_surface_mark_dirty(surface_);
        invalidateLevels();
        return S_OK;
    }
    D2D1_BITMAP_OPTIONS GetOptions() override { return options_; }
    HRESULT Map(D2D1_MAP_OPTIONS, D2D1_MAPPED_RECT* mapped) override {
        cairo_surface_flush(surface_);
        mapped->bits = cairo_image_surface_get_data(surface_);
        mapped->pitch = static_cast<UINT32>(cairo_image_surface_get_stride(surface_));
        return S_OK;
    }
    HRESULT Unmap() override {
        cairo_surface_mark_dirty(surface_);
        invalidateLevels();
        return S_OK;
    }
    cairo_surface_t* surface() const { return surface_; }

    // A copy halved `level` times (0: the bitmap itself), built on first
    // use. Direct2D samples a GPU texture, so Tinta draws large images small
    // on every frame; Cairo's filter would read every source pixel each
    // time (5016x3344 drawn at 900 px: ~100 ms per frame without levels).
    cairo_surface_t* level(int n) {
        if (n <= 0) return surface_;
        while (static_cast<int>(levels_.size()) < n) {
            cairo_surface_t* from = levels_.empty() ? surface_ : levels_.back();
            int w = (cairo_image_surface_get_width(from) + 1) / 2, h = (cairo_image_surface_get_height(from) + 1) / 2;
            if (w < 1 || h < 1) break;
            cairo_surface_flush(from);
            cairo_surface_t* half = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
            cairo_t* cr = cairo_create(half);
            cairo_scale(cr, static_cast<double>(w) / cairo_image_surface_get_width(from),
                        static_cast<double>(h) / cairo_image_surface_get_height(from));
            cairo_set_source_surface(cr, from, 0, 0);
            cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
            cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_paint(cr);
            cairo_destroy(cr);
            levels_.push_back(half);
        }
        return levels_.empty() ? surface_ : levels_[std::min<size_t>(n, levels_.size()) - 1];
    }
    // The pixels changed (copied in, mapped, or drawn into as a target)
    void invalidateLevels() {
        for (cairo_surface_t* s : levels_) cairo_surface_destroy(s);
        levels_.clear();
    }

private:
    cairo_surface_t* surface_;
    float dpiX_, dpiY_;
    D2D1_BITMAP_OPTIONS options_;
    std::vector<cairo_surface_t*> levels_;
};

class CommandList final : public TintaComObject<ID2D1CommandList> {
public:
    using Primary = ID2D1CommandList;
    CommandList() : surface_(cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA, nullptr)) {}
    ~CommandList() override { cairo_surface_destroy(surface_); }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<ID2D1CommandList, ID2D1Image, ID2D1Resource>(this, riid, object);
    }
    void GetFactory(ID2D1Factory** factory) override { *factory = nullptr; }
    HRESULT Close() override {
        cairo_surface_flush(surface_);
        return S_OK;
    }
    cairo_surface_t* surface() const { return surface_; }

private:
    cairo_surface_t* surface_;
};

class SvgElement final : public TintaComObject<ID2D1SvgElement> {
public:
    using Primary = ID2D1SvgElement;
    explicit SvgElement(RsvgHandle* handle) : handle_(handle) { g_object_ref(handle_); }
    ~SvgElement() override { g_object_unref(handle_); }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<ID2D1SvgElement, ID2D1Resource>(this, riid, object);
    }
    void GetFactory(ID2D1Factory** factory) override { *factory = nullptr; }
    HRESULT GetAttributeValue(const WCHAR* name, D2D1_SVG_ATTRIBUTE_POD_TYPE type, void* value, UINT32 size) override {
        if (!name || std::wcscmp(name, L"viewBox") != 0 || type != D2D1_SVG_ATTRIBUTE_POD_TYPE_VIEWBOX ||
            size < sizeof(D2D1_SVG_VIEWBOX)) {
            return E_INVALIDARG;
        }
        gboolean hasWidth, hasHeight, hasViewBox;
        RsvgLength width, height;
        RsvgRectangle viewBox;
        rsvg_handle_get_intrinsic_dimensions(handle_, &hasWidth, &width, &hasHeight, &height, &hasViewBox, &viewBox);
        if (!hasViewBox) return E_FAIL;
        auto* out = static_cast<D2D1_SVG_VIEWBOX*>(value);
        out->x = static_cast<float>(viewBox.x);
        out->y = static_cast<float>(viewBox.y);
        out->width = static_cast<float>(viewBox.width);
        out->height = static_cast<float>(viewBox.height);
        return S_OK;
    }

private:
    RsvgHandle* handle_;
};

class SvgDocument final : public TintaComObject<ID2D1SvgDocument> {
public:
    using Primary = ID2D1SvgDocument;
    SvgDocument(RsvgHandle* handle, D2D1_SIZE_F viewport) : handle_(handle), viewport_(viewport) {}
    ~SvgDocument() override { g_object_unref(handle_); }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<ID2D1SvgDocument, ID2D1Resource>(this, riid, object);
    }
    void GetFactory(ID2D1Factory** factory) override { *factory = nullptr; }
    void GetRoot(ID2D1SvgElement** root) override { *root = new SvgElement(handle_); }
    HRESULT SetViewportSize(D2D1_SIZE_F size) override {
        viewport_ = size;
        return S_OK;
    }
    D2D1_SIZE_F GetViewportSize() override { return viewport_; }
    void draw(cairo_t* cr) const {
        RsvgRectangle viewport{0, 0, viewport_.width, viewport_.height};
        rsvg_handle_render_document(handle_, cr, &viewport, nullptr);
    }

private:
    RsvgHandle* handle_;
    D2D1_SIZE_F viewport_;
};

// ---- Render target ---------------------------------------------------------------
IDWriteFactory* dwriteFactory() {
    static IDWriteFactory* factory = [] {
        IDWriteFactory* f = nullptr;
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&f));
        return f;
    }();
    return factory;
}

Device* sharedDevice();
class RenderTarget;

// Each window's current render target, for the window layer to present
std::map<HWND, RenderTarget*>& windowTargets() {
    static std::map<HWND, RenderTarget*> targets;
    return targets;
}

class RenderTarget final : public TintaComObject<ID2D1HwndRenderTarget> {
public:
    using Primary = ID2D1HwndRenderTarget;

    RenderTarget(float dpiX, float dpiY) : dpiX_(dpiX > 0 ? dpiX : 96.0f), dpiY_(dpiY > 0 ? dpiY : 96.0f) {}
    ~RenderTarget() override {
        auto it = windowTargets().find(hwnd_);
        if (hwnd_ && it != windowTargets().end() && it->second == this) windowTargets().erase(it);
        if (cr_) cairo_destroy(cr_);
        if (ownSurface_) cairo_surface_destroy(ownSurface_);
        if (targetImage_) targetImage_->Release();
    }

    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<ID2D1HwndRenderTarget, ID2D1DeviceContext5, ID2D1DeviceContext4, ID2D1DeviceContext3,
                           ID2D1DeviceContext2, ID2D1DeviceContext1, ID2D1DeviceContext, ID2D1RenderTarget,
                           ID2D1Resource>(this, riid, object);
    }
    void GetFactory(ID2D1Factory** factory) override { *factory = nullptr; }

    // A window or WIC-bitmap target: draw into `surface` (referenced)
    void attachSurface(cairo_surface_t* surface, HWND hwnd) {
        if (ownSurface_) cairo_surface_destroy(ownSurface_);
        ownSurface_ = cairo_surface_reference(surface);
        hwnd_ = hwnd;
        if (hwnd_) windowTargets()[hwnd_] = this;
        resetContext(ownSurface_);
    }
    cairo_surface_t* currentSurface() const { return cr_ ? cairo_get_target(cr_) : nullptr; }
    cairo_surface_t* windowSurface() const { return ownSurface_; }

    // -- Resource creation
    HRESULT CreateBitmap(D2D1_SIZE_U size, const void* data, UINT32 pitch, const D2D1_BITMAP_PROPERTIES* props,
                         ID2D1Bitmap** bitmap) override {
        auto* created = new Bitmap(static_cast<int>(size.width), static_cast<int>(size.height),
                                   props ? props->dpiX : 96.0f, props ? props->dpiY : 96.0f, D2D1_BITMAP_OPTIONS_NONE);
        if (data) created->CopyFromMemory(nullptr, data, pitch);
        *bitmap = created;
        return S_OK;
    }
    HRESULT CreateBitmap(D2D1_SIZE_U size, const void* data, UINT32 pitch, const D2D1_BITMAP_PROPERTIES1* props,
                         ID2D1Bitmap1** bitmap) override {
        auto* created = new Bitmap(static_cast<int>(size.width), static_cast<int>(size.height),
                                   props ? props->dpiX : 96.0f, props ? props->dpiY : 96.0f,
                                   props ? props->bitmapOptions : D2D1_BITMAP_OPTIONS_NONE);
        if (data) created->CopyFromMemory(nullptr, data, pitch);
        *bitmap = created;
        return S_OK;
    }
    HRESULT CreateBitmapFromWicBitmap(IWICBitmapSource* source, const D2D1_BITMAP_PROPERTIES* props,
                                      ID2D1Bitmap** bitmap) override {
        ID2D1Bitmap1* created = nullptr;
        D2D1_BITMAP_PROPERTIES1 p1 = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(),
                                                             props ? props->dpiX : 96.0f, props ? props->dpiY : 96.0f);
        HRESULT hr = CreateBitmapFromWicBitmap(source, &p1, &created);
        *bitmap = created;
        return hr;
    }
    HRESULT CreateBitmapFromWicBitmap(IWICBitmapSource* source, const D2D1_BITMAP_PROPERTIES1* props,
                                      ID2D1Bitmap1** bitmap) override {
        PixelBuffer pixels;
        if (!readWicSource(source, pixels)) return E_FAIL;
        auto* created = new Bitmap(pixels.width, pixels.height, props ? props->dpiX : 96.0f,
                                   props ? props->dpiY : 96.0f, props ? props->bitmapOptions : D2D1_BITMAP_OPTIONS_NONE);
        created->CopyFromMemory(nullptr, pixels.pixels.data(), static_cast<UINT32>(pixels.stride));
        *bitmap = created;
        return S_OK;
    }
    HRESULT CreateSolidColorBrush(const D2D1_COLOR_F* color, const D2D1_BRUSH_PROPERTIES* props,
                                  ID2D1SolidColorBrush** brush) override {
        *brush = new SolidColorBrush(color ? *color : D2D1::ColorF(0, 0, 0), props ? props->opacity : 1.0f);
        return S_OK;
    }
    HRESULT CreateCommandList(ID2D1CommandList** list) override {
        *list = new CommandList();
        return S_OK;
    }
    HRESULT CreateSvgDocument(IStream* stream, D2D1_SIZE_F viewport, ID2D1SvgDocument** document) override;
    void GetDevice(ID2D1Device** device) override;

    // -- Targets
    void SetTarget(ID2D1Image* image) override {
        if (image) image->AddRef();
        if (targetImage_) targetImage_->Release();
        targetImage_ = image;
        if (auto* bitmap = dynamic_cast<Bitmap*>(image)) {
            bitmap->invalidateLevels();  // about to be drawn into
            resetContext(bitmap->surface());
        }
        else if (auto* list = dynamic_cast<CommandList*>(image)) resetContext(list->surface());
        else resetContext(ownSurface_);
    }
    void GetTarget(ID2D1Image** image) override {
        if (targetImage_) targetImage_->AddRef();
        *image = targetImage_;
    }

    // -- Drawing
    void DrawLine(D2D1_POINT_2F a, D2D1_POINT_2F b, ID2D1Brush* brush, FLOAT width, ID2D1StrokeStyle* style) override {
        if (!cr_) return;
        cairo_new_path(cr_);
        cairo_move_to(cr_, a.x, a.y);
        cairo_line_to(cr_, b.x, b.y);
        stroke(brush, width, style);
    }
    void DrawRectangle(const D2D1_RECT_F* r, ID2D1Brush* brush, FLOAT width, ID2D1StrokeStyle* style) override {
        if (!cr_ || !r) return;
        cairo_new_path(cr_);
        cairo_rectangle(cr_, r->left, r->top, r->right - r->left, r->bottom - r->top);
        stroke(brush, width, style);
    }
    void FillRectangle(const D2D1_RECT_F* r, ID2D1Brush* brush) override {
        if (!cr_ || !r) return;
        cairo_new_path(cr_);
        cairo_rectangle(cr_, r->left, r->top, r->right - r->left, r->bottom - r->top);
        fill(brush);
    }
    void DrawRoundedRectangle(const D2D1_ROUNDED_RECT* rr, ID2D1Brush* brush, FLOAT width,
                              ID2D1StrokeStyle* style) override {
        if (!cr_ || !rr) return;
        roundedRectPath(*rr);
        stroke(brush, width, style);
    }
    void FillRoundedRectangle(const D2D1_ROUNDED_RECT* rr, ID2D1Brush* brush) override {
        if (!cr_ || !rr) return;
        roundedRectPath(*rr);
        fill(brush);
    }
    void DrawEllipse(const D2D1_ELLIPSE* e, ID2D1Brush* brush, FLOAT width, ID2D1StrokeStyle* style) override {
        if (!cr_ || !e) return;
        ellipsePath(*e);
        stroke(brush, width, style);
    }
    void FillEllipse(const D2D1_ELLIPSE* e, ID2D1Brush* brush) override {
        if (!cr_ || !e) return;
        ellipsePath(*e);
        fill(brush);
    }
    void DrawGeometry(ID2D1Geometry* geometry, ID2D1Brush* brush, FLOAT width, ID2D1StrokeStyle* style) override {
        auto* path = dynamic_cast<PathGeometry*>(geometry);
        if (!cr_ || !path) return;
        path->replay(cr_);
        stroke(brush, width, style);
    }
    void FillGeometry(ID2D1Geometry* geometry, ID2D1Brush* brush, ID2D1Brush*) override {
        auto* path = dynamic_cast<PathGeometry*>(geometry);
        if (!cr_ || !path) return;
        path->replay(cr_);
        fill(brush);
    }
    void DrawBitmap(ID2D1Bitmap* bitmap, const D2D1_RECT_F* dest, FLOAT opacity,
                    D2D1_BITMAP_INTERPOLATION_MODE mode, const D2D1_RECT_F* src) override {
        auto* own = dynamic_cast<Bitmap*>(bitmap);
        if (!cr_ || !own) return;
        D2D1_SIZE_F size = own->GetSize();
        D2D1_SIZE_U px = own->GetPixelSize();
        D2D1_RECT_F s = src ? *src : D2D1::RectF(0, 0, size.width, size.height);
        D2D1_RECT_F d = dest ? *dest : D2D1::RectF(0, 0, size.width, size.height);
        double sw = s.right - s.left, sh = s.bottom - s.top;
        if (sw <= 0 || sh <= 0 || d.right <= d.left || d.bottom <= d.top) return;
        // Source rect in DIPs of the bitmap -> pixels
        double pxPerDipX = px.width / std::max(1e-6, static_cast<double>(size.width));
        double pxPerDipY = px.height / std::max(1e-6, static_cast<double>(size.height));
        // Device pixels per source pixel; far below 1, draw from a halved copy
        double devW = d.right - d.left, devH = d.bottom - d.top;
        cairo_user_to_device_distance(cr_, &devW, &devH);
        double shrink = std::min(std::fabs(devW) / (sw * pxPerDipX), std::fabs(devH) / (sh * pxPerDipY));
        int levelIndex = 0;
        bool smooth = mode != D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
        static const bool levelsEnabled = [] {
            const char* v = std::getenv("TINTA_BITMAP_LEVELS");  // "0" disables (diagnostics)
            return !v || std::strcmp(v, "0") != 0;
        }();
        // Only raster targets: PDF and print pages (recording surfaces) keep
        // every pixel, and the viewer or printer does the scaling
        bool raster = cairo_surface_get_type(cairo_get_target(cr_)) == CAIRO_SURFACE_TYPE_IMAGE;
        while (levelsEnabled && raster && smooth && shrink > 0 && shrink * 2 <= 1.0 && levelIndex < 12) {
            shrink *= 2;
            levelIndex++;
        }
        cairo_surface_t* source = own->level(levelIndex);
        double levelX = cairo_image_surface_get_width(source) / static_cast<double>(px.width);
        double levelY = cairo_image_surface_get_height(source) / static_cast<double>(px.height);
        pxPerDipX *= levelX;
        pxPerDipY *= levelY;
        cairo_save(cr_);
        cairo_new_path(cr_);
        cairo_rectangle(cr_, d.left, d.top, d.right - d.left, d.bottom - d.top);
        cairo_clip(cr_);
        cairo_translate(cr_, d.left, d.top);
        cairo_scale(cr_, (d.right - d.left) / (sw * pxPerDipX), (d.bottom - d.top) / (sh * pxPerDipY));
        cairo_set_source_surface(cr_, source, -s.left * pxPerDipX, -s.top * pxPerDipY);
        cairo_pattern_set_filter(cairo_get_source(cr_), mode == D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
                                                             ? CAIRO_FILTER_NEAREST
                                                             : CAIRO_FILTER_GOOD);
        cairo_pattern_set_extend(cairo_get_source(cr_), CAIRO_EXTEND_PAD);
        cairo_paint_with_alpha(cr_, opacity);
        cairo_restore(cr_);
    }
    void DrawImage(ID2D1Image* image, const D2D1_POINT_2F* offset, const D2D1_RECT_F*, D2D1_INTERPOLATION_MODE,
                   D2D1_COMPOSITE_MODE) override {
        if (!cr_ || !image) return;
        double ox = offset ? offset->x : 0, oy = offset ? offset->y : 0;
        if (auto* list = dynamic_cast<CommandList*>(image)) {
            cairo_save(cr_);
            cairo_set_source_surface(cr_, list->surface(), ox, oy);
            cairo_paint(cr_);
            cairo_restore(cr_);
        } else if (auto* bitmap = dynamic_cast<Bitmap*>(image)) {
            D2D1_SIZE_F size = bitmap->GetSize();
            D2D1_RECT_F d = D2D1::RectF(ox, oy, ox + size.width, oy + size.height);
            DrawBitmap(bitmap, &d, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, nullptr);
        }
    }
    void DrawSvgDocument(ID2D1SvgDocument* document) override {
        auto* own = dynamic_cast<SvgDocument*>(document);
        if (cr_ && own) {
            cairo_save(cr_);
            own->draw(cr_);
            cairo_restore(cr_);
        }
    }
    void DrawText(const WCHAR* string, UINT32 length, IDWriteTextFormat* format, const D2D1_RECT_F* rect,
                  ID2D1Brush* brush, D2D1_DRAW_TEXT_OPTIONS options, DWRITE_MEASURING_MODE) override {
        if (!cr_ || !rect || !format) return;
        IDWriteTextLayout* layout = nullptr;
        dwriteFactory()->CreateTextLayout(string, length, format, rect->right - rect->left, rect->bottom - rect->top,
                                          &layout);
        if (!layout) return;
        DrawTextLayout(D2D1::Point2F(rect->left, rect->top), layout, brush, options);
        layout->Release();
    }
    void DrawTextLayout(D2D1_POINT_2F origin, IDWriteTextLayout* layout, ID2D1Brush* brush,
                        D2D1_DRAW_TEXT_OPTIONS options) override {
        TextLayoutDrawing* drawing = drawingOf(layout);
        if (!cr_ || !drawing) return;
        cairo_save(cr_);
        if (options & D2D1_DRAW_TEXT_OPTIONS_CLIP) {
            cairo_new_path(cr_);
            cairo_rectangle(cr_, origin.x, origin.y, drawing->boxWidth(), drawing->boxHeight());
            cairo_clip(cr_);
        }
        applyBrush(cr_, brush);
        cairo_new_path(cr_);
        drawing->drawTo(cr_, origin.x, origin.y);
        cairo_restore(cr_);
    }

    // -- State
    void SetTransform(const D2D1_MATRIX_3X2_F* m) override {
        if (m) transform_ = *m;
        applyMatrix();
    }
    void GetTransform(D2D1_MATRIX_3X2_F* m) override { *m = transform_; }
    void SetAntialiasMode(D2D1_ANTIALIAS_MODE mode) override {
        antialias_ = mode;
        if (cr_) cairo_set_antialias(cr_, mode == D2D1_ANTIALIAS_MODE_ALIASED ? CAIRO_ANTIALIAS_NONE : CAIRO_ANTIALIAS_DEFAULT);
    }
    D2D1_ANTIALIAS_MODE GetAntialiasMode() override { return antialias_; }
    void SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE) override {}
    void SetTextRenderingParams(IDWriteRenderingParams*) override {}
    void PushAxisAlignedClip(const D2D1_RECT_F* r, D2D1_ANTIALIAS_MODE mode) override {
        if (!r) return;
        // Axis-aligned in device space: the transformed rect's bounding box
        D2D1::Matrix3x2F m(transform_);
        D2D1_POINT_2F pts[4] = {m.TransformPoint({r->left, r->top}), m.TransformPoint({r->right, r->top}),
                                m.TransformPoint({r->left, r->bottom}), m.TransformPoint({r->right, r->bottom})};
        double x1 = pts[0].x, y1 = pts[0].y, x2 = x1, y2 = y1;
        for (const auto& p : pts) {
            x1 = std::min<double>(x1, p.x);
            y1 = std::min<double>(y1, p.y);
            x2 = std::max<double>(x2, p.x);
            y2 = std::max<double>(y2, p.y);
        }
        double sx = dpiX_ / 96.0, sy = dpiY_ / 96.0;
        Clip clip{x1 * sx, y1 * sy, x2 * sx, y2 * sy};
        if (mode == D2D1_ANTIALIAS_MODE_ALIASED) {
            clip.x1 = std::round(clip.x1);
            clip.y1 = std::round(clip.y1);
            clip.x2 = std::round(clip.x2);
            clip.y2 = std::round(clip.y2);
        }
        clips_.push_back(clip);
        applyClips();
    }
    void PopAxisAlignedClip() override {
        if (!clips_.empty()) clips_.pop_back();
        applyClips();
    }
    void Clear(const D2D1_COLOR_F* color) override {
        if (!cr_) return;
        cairo_save(cr_);
        cairo_identity_matrix(cr_);
        cairo_set_operator(cr_, CAIRO_OPERATOR_SOURCE);
        D2D1_COLOR_F c = color ? *color : D2D1::ColorF(0, 0, 0, 0);
        cairo_set_source_rgba(cr_, c.r, c.g, c.b, c.a);
        cairo_paint(cr_);
        cairo_restore(cr_);
    }
    void BeginDraw() override {
        if (!cr_) resetContext(ownSurface_);
    }
    HRESULT EndDraw(D2D1_TAG*, D2D1_TAG*) override {
        clips_.clear();
        applyClips();
        if (cr_) cairo_surface_flush(cairo_get_target(cr_));
        if (auto* bitmap = dynamic_cast<Bitmap*>(targetImage_)) bitmap->invalidateLevels();
        if (hwnd_) presentWindow(hwnd_);
        return S_OK;
    }
    HRESULT Flush(D2D1_TAG*, D2D1_TAG*) override {
        if (cr_) cairo_surface_flush(cairo_get_target(cr_));
        return S_OK;
    }
    D2D1_PIXEL_FORMAT GetPixelFormat() override {
        return D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    }
    void SetDpi(FLOAT dpiX, FLOAT dpiY) override {
        dpiX_ = dpiX > 0 ? dpiX : 96.0f;
        dpiY_ = dpiY > 0 ? dpiY : 96.0f;
        applyMatrix();
    }
    void GetDpi(FLOAT* dpiX, FLOAT* dpiY) override {
        *dpiX = dpiX_;
        *dpiY = dpiY_;
    }
    D2D1_SIZE_F GetSize() override {
        D2D1_SIZE_U px = GetPixelSize();
        return D2D1::SizeF(px.width * 96.0f / dpiX_, px.height * 96.0f / dpiY_);
    }
    D2D1_SIZE_U GetPixelSize() override {
        cairo_surface_t* s = cr_ ? cairo_get_target(cr_) : ownSurface_;
        if (!s || cairo_surface_get_type(s) != CAIRO_SURFACE_TYPE_IMAGE) return D2D1::SizeU(0, 0);
        return D2D1::SizeU(static_cast<UINT32>(cairo_image_surface_get_width(s)),
                           static_cast<UINT32>(cairo_image_surface_get_height(s)));
    }
    UINT32 GetMaximumBitmapSize() override { return 16384; }

    // -- Window target
    D2D1_WINDOW_STATE CheckWindowState() override { return D2D1_WINDOW_STATE_NONE; }
    HRESULT Resize(const D2D1_SIZE_U* size) override;
    HWND GetHwnd() override { return hwnd_; }

private:
    struct Clip {
        double x1, y1, x2, y2;
    };

    void resetContext(cairo_surface_t* surface) {
        if (cr_) cairo_destroy(cr_);
        cr_ = surface ? cairo_create(surface) : nullptr;
        if (!cr_) return;
        SetAntialiasMode(antialias_);
        applyMatrix();
        applyClips();
    }

    void applyMatrix() {
        if (!cr_) return;
        double sx = dpiX_ / 96.0, sy = dpiY_ / 96.0;
        cairo_matrix_t m;
        cairo_matrix_init(&m, transform_._11 * sx, transform_._12 * sy, transform_._21 * sx, transform_._22 * sy,
                          transform_._31 * sx, transform_._32 * sy);
        cairo_set_matrix(cr_, &m);
    }

    void applyClips() {
        if (!cr_) return;
        cairo_matrix_t saved;
        cairo_get_matrix(cr_, &saved);
        cairo_identity_matrix(cr_);
        cairo_reset_clip(cr_);
        for (const Clip& c : clips_) {
            cairo_new_path(cr_);
            cairo_rectangle(cr_, c.x1, c.y1, std::max(0.0, c.x2 - c.x1), std::max(0.0, c.y2 - c.y1));
            cairo_clip(cr_);
        }
        cairo_set_matrix(cr_, &saved);
    }

    void roundedRectPath(const D2D1_ROUNDED_RECT& rr) {
        const D2D1_RECT_F& r = rr.rect;
        double w = r.right - r.left, h = r.bottom - r.top;
        double rx = std::min<double>(rr.radiusX, w / 2), ry = std::min<double>(rr.radiusY, h / 2);
        cairo_new_path(cr_);
        if (rx <= 0 || ry <= 0) {
            cairo_rectangle(cr_, r.left, r.top, w, h);
            return;
        }
        auto corner = [&](double cx, double cy, double a1, double a2) {
            cairo_save(cr_);
            cairo_translate(cr_, cx, cy);
            cairo_scale(cr_, rx, ry);
            cairo_arc(cr_, 0, 0, 1, a1, a2);
            cairo_restore(cr_);
        };
        corner(r.right - rx, r.top + ry, -M_PI / 2, 0);
        corner(r.right - rx, r.bottom - ry, 0, M_PI / 2);
        corner(r.left + rx, r.bottom - ry, M_PI / 2, M_PI);
        corner(r.left + rx, r.top + ry, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr_);
    }

    void ellipsePath(const D2D1_ELLIPSE& e) {
        cairo_new_path(cr_);
        if (e.radiusX <= 0 || e.radiusY <= 0) return;
        cairo_save(cr_);
        cairo_translate(cr_, e.point.x, e.point.y);
        cairo_scale(cr_, e.radiusX, e.radiusY);
        cairo_arc(cr_, 0, 0, 1, 0, 2 * M_PI);
        cairo_restore(cr_);
        cairo_close_path(cr_);
    }

    void fill(ID2D1Brush* brush) {
        applyBrush(cr_, brush);
        cairo_fill(cr_);
    }

    void stroke(ID2D1Brush* brush, float width, ID2D1StrokeStyle* style) {
        applyBrush(cr_, brush);
        applyStroke(cr_, width, style);
        cairo_stroke(cr_);
    }

    cairo_t* cr_ = nullptr;
    cairo_surface_t* ownSurface_ = nullptr;
    ID2D1Image* targetImage_ = nullptr;
    HWND hwnd_ = nullptr;
    float dpiX_, dpiY_;
    D2D1_MATRIX_3X2_F transform_ = D2D1::Matrix3x2F::Identity();
    D2D1_ANTIALIAS_MODE antialias_ = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
    std::vector<Clip> clips_;

public:
    // Window targets own an image surface the window layer presents
    HRESULT resizeOwnSurface(int width, int height) {
        cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, std::max(1, width), std::max(1, height));
        attachSurface(surface, hwnd_);
        cairo_surface_destroy(surface);
        return S_OK;
    }
};

HRESULT RenderTarget::Resize(const D2D1_SIZE_U* size) {
    if (!size) return E_INVALIDARG;
    return resizeOwnSurface(static_cast<int>(size->width), static_cast<int>(size->height));
}

HRESULT RenderTarget::CreateSvgDocument(IStream* stream, D2D1_SIZE_F viewport, ID2D1SvgDocument** document) {
    if (!stream || !document) return E_INVALIDARG;
    std::vector<unsigned char> bytes;
    unsigned char buffer[65536];
    ULONG read = 0;
    while (SUCCEEDED(stream->Read(buffer, sizeof buffer, &read)) && read > 0) bytes.insert(bytes.end(), buffer, buffer + read);
    GError* error = nullptr;
    RsvgHandle* handle = rsvg_handle_new_from_data(bytes.data(), bytes.size(), &error);
    if (!handle) {
        if (error) g_error_free(error);
        return E_FAIL;
    }
    *document = new SvgDocument(handle, viewport);
    return S_OK;
}

// ---- PDF output for print controls -------------------------------------------------
class PrintControl final : public TintaComObject<ID2D1PrintControl> {
public:
    using Primary = ID2D1PrintControl;
    explicit PrintControl(IStream* output) : output_(output) {
        output_->AddRef();
        pdf_ = cairo_pdf_surface_create_for_stream(&PrintControl::write, this, 595, 842);
        cr_ = cairo_create(pdf_);
    }
    ~PrintControl() override {
        Close();
        output_->Release();
    }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<ID2D1PrintControl>(this, riid, object);
    }
    HRESULT AddPage(ID2D1CommandList* list, D2D_SIZE_F pageSize, IStream*, D2D1_TAG*, D2D1_TAG*) override {
        auto* own = dynamic_cast<CommandList*>(list);
        if (!own || !cr_) return E_INVALIDARG;
        // DIPs (1/96 in) to PDF points (1/72 in)
        cairo_pdf_surface_set_size(pdf_, pageSize.width * 0.75, pageSize.height * 0.75);
        cairo_save(cr_);
        cairo_scale(cr_, 0.75, 0.75);
        cairo_set_source_surface(cr_, own->surface(), 0, 0);
        cairo_paint(cr_);
        cairo_restore(cr_);
        cairo_show_page(cr_);
        // cairo-pdf writes recording sources when the document finishes and
        // does not keep them alive itself; callers release pages before Close
        pages_.push_back(cairo_surface_reference(own->surface()));
        return cairo_status(cr_) == CAIRO_STATUS_SUCCESS ? S_OK : E_FAIL;
    }
    HRESULT Close() override {
        if (!pdf_) return S_OK;
        cairo_destroy(cr_);
        cr_ = nullptr;
        cairo_surface_finish(pdf_);
        cairo_status_t status = cairo_surface_status(pdf_);
        cairo_surface_destroy(pdf_);
        pdf_ = nullptr;
        for (cairo_surface_t* page : pages_) cairo_surface_destroy(page);
        pages_.clear();
        return status == CAIRO_STATUS_SUCCESS && !failed_ ? S_OK : E_FAIL;
    }

private:
    static cairo_status_t write(void* closure, const unsigned char* data, unsigned int length) {
        auto* self = static_cast<PrintControl*>(closure);
        ULONG written = 0;
        if (FAILED(self->output_->Write(data, length, &written)) || written != length) {
            self->failed_ = true;
            return CAIRO_STATUS_WRITE_ERROR;
        }
        return CAIRO_STATUS_SUCCESS;
    }

    IStream* output_;
    cairo_surface_t* pdf_ = nullptr;
    cairo_t* cr_ = nullptr;
    std::vector<cairo_surface_t*> pages_;
    bool failed_ = false;
};

// The print job target: where a "Microsoft Print to PDF" job writes
class PrintTarget final : public TintaComObject<IPrintDocumentPackageTarget> {
public:
    using Primary = IPrintDocumentPackageTarget;
    explicit PrintTarget(IStream* output) : output(output) {
        if (output) output->AddRef();
    }
    ~PrintTarget() override {
        if (output) output->Release();
    }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IPrintDocumentPackageTarget>(this, riid, object);
    }
    HRESULT Cancel() override { return S_OK; }
    IStream* output;
};

class Device final : public TintaComObject<ID2D1Device> {
public:
    using Primary = ID2D1Device;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<ID2D1Device, ID2D1Resource>(this, riid, object);
    }
    void GetFactory(ID2D1Factory** factory) override { *factory = nullptr; }
    HRESULT CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS, ID2D1DeviceContext** context) override {
        *context = new RenderTarget(96.0f, 96.0f);
        return S_OK;
    }
    HRESULT CreatePrintControl(IWICImagingFactory*, IPrintDocumentPackageTarget* target,
                               const D2D1_PRINT_CONTROL_PROPERTIES*, ID2D1PrintControl** control) override {
        auto* own = dynamic_cast<PrintTarget*>(target);
        if (!own || !own->output) return E_INVALIDARG;
        *control = new PrintControl(own->output);
        return S_OK;
    }
};

Device* sharedDevice() {
    static Device* device = new Device();
    return device;
}

void RenderTarget::GetDevice(ID2D1Device** device) {
    Device* shared = sharedDevice();
    shared->AddRef();
    *device = shared;
}

class PrintTargetFactory final : public TintaComObject<IPrintDocumentPackageTargetFactory> {
public:
    using Primary = IPrintDocumentPackageTargetFactory;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IPrintDocumentPackageTargetFactory>(this, riid, object);
    }
    HRESULT CreateDocumentPackageTargetForPrintJob(LPCWSTR, LPCWSTR, IStream* output, IStream*,
                                                   IPrintDocumentPackageTarget** target) override {
        // Only file output (PDF) is supported until the GTK print path
        if (!output) return E_NOTIMPL;
        *target = new PrintTarget(output);
        return S_OK;
    }
};

class Factory final : public TintaComObject<ID2D1Factory1> {
public:
    using Primary = ID2D1Factory1;
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<ID2D1Factory1, ID2D1Factory>(this, riid, object);
    }
    void GetDesktopDpi(FLOAT* dpiX, FLOAT* dpiY) override {
        *dpiX = 96.0f;
        *dpiY = 96.0f;
    }
    HRESULT CreatePathGeometry(ID2D1PathGeometry** geometry) override {
        *geometry = new PathGeometry();
        return S_OK;
    }
    HRESULT CreateStrokeStyle(const D2D1_STROKE_STYLE_PROPERTIES* props, const FLOAT* dashes, UINT32 count,
                              ID2D1StrokeStyle** style) override {
        *style = new StrokeStyle(props ? *props : D2D1::StrokeStyleProperties(), dashes, dashes ? count : 0);
        return S_OK;
    }
    HRESULT CreateWicBitmapRenderTarget(IWICBitmap* target, const D2D1_RENDER_TARGET_PROPERTIES* props,
                                        ID2D1RenderTarget** renderTarget) override {
        cairo_surface_t* surface = surfaceOf(target);
        if (!surface) return E_INVALIDARG;
        auto* created = new RenderTarget(props ? props->dpiX : 96.0f, props ? props->dpiY : 96.0f);
        created->attachSurface(surface, nullptr);
        *renderTarget = created;
        return S_OK;
    }
    HRESULT CreateHwndRenderTarget(const D2D1_RENDER_TARGET_PROPERTIES* props,
                                   const D2D1_HWND_RENDER_TARGET_PROPERTIES* hwndProps,
                                   ID2D1HwndRenderTarget** renderTarget) override {
        auto* created = new RenderTarget(props ? props->dpiX : 96.0f, props ? props->dpiY : 96.0f);
        int width = hwndProps ? static_cast<int>(hwndProps->pixelSize.width) : 1;
        int height = hwndProps ? static_cast<int>(hwndProps->pixelSize.height) : 1;
        cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, std::max(1, width), std::max(1, height));
        created->attachSurface(surface, hwndProps ? hwndProps->hwnd : nullptr);
        cairo_surface_destroy(surface);
        *renderTarget = created;
        return S_OK;
    }
};

}  // namespace

cairo_surface_t* targetSurface(ID2D1RenderTarget* target) {
    auto* own = dynamic_cast<RenderTarget*>(target);
    return own ? own->currentSurface() : nullptr;
}

cairo_surface_t* windowSurface(HWND hwnd) {
    auto it = windowTargets().find(hwnd);
    return it == windowTargets().end() ? nullptr : it->second->windowSurface();
}

HRESULT createPrintTargetFactory(REFIID riid, void** object) {
    auto* factory = new PrintTargetFactory();
    HRESULT hr = factory->QueryInterface(riid, object);
    factory->Release();
    return hr;
}

}  // namespace tinta

HRESULT D2D1CreateFactory(D2D1_FACTORY_TYPE, REFIID riid, const D2D1_FACTORY_OPTIONS*, void** factory) {
    if (!factory) return E_POINTER;
    auto* created = new tinta::Factory();
    HRESULT hr = created->QueryInterface(riid, factory);
    created->Release();
    return hr;
}
