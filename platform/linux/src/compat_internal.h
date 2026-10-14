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

}  // namespace tinta

#endif  // TINTA_LINUX_COMPAT_INTERNAL_H
