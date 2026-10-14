// Shared internals of the Linux compat layer: how the Direct2D-on-Cairo side
// reaches DirectWrite-on-Pango layouts, wide/UTF-8 conversion, and paths.
#ifndef TINTA_LINUX_COMPAT_INTERNAL_H
#define TINTA_LINUX_COMPAT_INTERNAL_H

#include <cairo.h>
#include <pango/pango.h>

#include <string>
#include <vector>

#include "tinta_d2d.h"

namespace tinta {

std::string toUtf8(const wchar_t* text, size_t length);
inline std::string toUtf8(const std::wstring& text) { return toUtf8(text.data(), text.size()); }
std::wstring toWide(const char* text, size_t length);
inline std::wstring toWide(const std::string& text) { return toWide(text.data(), text.size()); }

// A Windows-syntax wide path as a native UTF-8 path ('\' becomes '/')
std::string nativePath(const wchar_t* path);

// Pango context shared by every layout: DIP units (1 DIP = 1 Cairo user
// unit), unhinted metrics and unrounded glyph positions, so measurement does
// not depend on the target's scale, as DirectWrite's natural mode.
PangoContext* pangoContext();

// Windows family names resolved to a Pango family list ("Segoe UI" -> the
// system UI font, Consolas -> a monospace stack, Cambria Math -> a math font)
std::string resolveFamily(const std::wstring& family);

// What d2d_cairo.cpp needs from a DirectWrite text layout
class TextLayoutDrawing {
public:
    virtual ~TextLayoutDrawing() = default;
    // Draws at `origin` (the layout box's top-left) in the current source
    virtual void drawTo(cairo_t* cr, float originX, float originY) = 0;
    virtual float boxWidth() const = 0;
    virtual float boxHeight() const = 0;
};
TextLayoutDrawing* drawingOf(IDWriteTextLayout* layout);

// Image data behind WIC bitmaps and D2D bitmaps: 32bpp premultiplied BGRA,
// which is Cairo's ARGB32 on little-endian machines
struct PixelBuffer {
    int width = 0;
    int height = 0;
    int stride = 0;
    std::vector<unsigned char> pixels;
};
cairo_surface_t* surfaceOf(IWICBitmap* bitmap);  // the WIC bitmap's own pixels
bool readWicSource(IWICBitmapSource* source, PixelBuffer& out);

// The surface a render target currently draws into (window targets: the
// image the window layer presents)
cairo_surface_t* targetSurface(ID2D1RenderTarget* target);
// The surface of the window's current HwndRenderTarget, or null
cairo_surface_t* windowSurface(HWND hwnd);

// Headless runs (main_linux.cpp): no display; `renderPng` captures the
// window once its startup work has settled, then quits
struct HeadlessOptions {
    std::string renderPng;
    int width = 0, height = 0;  // client size in pixels; 0 keeps the saved size
    UINT dpi = 96;
    int settleMs = 400;     // quiet time (no messages, no paints) before capture
    int timeoutMs = 20000;  // capture anyway after this long
    int repaints = 0;       // full repaints to time after settling (frame cost)
};
HeadlessOptions& headless();

// CoCreateInstance helpers
HRESULT createPrintTargetFactory(REFIID riid, void** object);
HRESULT createWicFactory(REFIID riid, void** object);
// Image decodes so far (tests: a frame must not decode twice)
int wicDecodeCount();

// Streams over files and memory (IStream)
IStream* createFileStream(const std::string& path, bool write, bool create);
IStream* createMemoryStream(const void* data, size_t size);
// The memory stream's contents (for GetHGlobalFromStream and WIC)
bool memoryStreamBytes(IStream* stream, std::vector<unsigned char>& out);

}  // namespace tinta

#endif  // TINTA_LINUX_COMPAT_INTERNAL_H
