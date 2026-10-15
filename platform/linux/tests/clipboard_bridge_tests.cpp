// The desktop clipboard bridge: what Tinta puts on the Win32 clipboard
// (OpenClipboard ... CloseClipboard) reaches GdkClipboard, text as text and
// a "PNG" (copy diagram as image) as an image. Needs a display: ctest runs
// it under xvfb-run with GDK_BACKEND=x11.
#include "window_internal.h"

#include <gtk/gtk.h>
#include <cairo.h>

#include <cstring>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        failures++;
        std::cerr << "FAIL: " << what << "\n";
    }
}

HGLOBAL global(const void* data, size_t size) {
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    std::memcpy(GlobalLock(memory), data, size);
    GlobalUnlock(memory);
    return memory;
}

std::string pngOf(int width, int height) {
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    std::string bytes;
    cairo_surface_write_to_png_stream(
        surface,
        [](void* closure, const unsigned char* data, unsigned int length) {
            static_cast<std::string*>(closure)->append(reinterpret_cast<const char*>(data), length);
            return CAIRO_STATUS_SUCCESS;
        },
        &bytes);
    cairo_surface_destroy(surface);
    return bytes;
}

GdkClipboard* desktop() { return gdk_display_get_clipboard(gdk_display_get_default()); }

std::string readText() {
    struct Read {
        bool done = false;
        std::string text;
    } read;
    gdk_clipboard_read_text_async(
        desktop(), nullptr,
        [](GObject* source, GAsyncResult* result, gpointer data) {
            auto* r = static_cast<Read*>(data);
            if (char* text = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(source), result, nullptr)) {
                r->text = text;
                g_free(text);
            }
            r->done = true;
        },
        &read);
    while (!read.done) g_main_context_iteration(nullptr, TRUE);
    return read.text;
}

GdkTexture* readTexture() {
    struct Read {
        bool done = false;
        GdkTexture* texture = nullptr;
    } read;
    gdk_clipboard_read_texture_async(
        desktop(), nullptr,
        [](GObject* source, GAsyncResult* result, gpointer data) {
            auto* r = static_cast<Read*>(data);
            r->texture = gdk_clipboard_read_texture_finish(GDK_CLIPBOARD(source), result, nullptr);
            r->done = true;
        },
        &read);
    while (!read.done) g_main_context_iteration(nullptr, TRUE);
    return read.texture;
}

}  // namespace

int main() {
    if (!tinta::gtk::start()) {
        std::cerr << "no display\n";
        return 2;
    }

    // Text
    const wchar_t text[] = L"Tinta été \U0001F3A8";
    OpenClipboard(nullptr);
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, global(text, sizeof text));
    CloseClipboard();
    check(readText() == "Tinta \xc3\xa9t\xc3\xa9 \xf0\x9f\x8e\xa8", "text reaches the desktop clipboard");

    // An image: copy diagram as image puts a DIB and a "PNG" there
    std::string png = pngOf(3, 2);
    OpenClipboard(nullptr);
    EmptyClipboard();
    SetClipboardData(CF_DIB, global("dib", 3));
    SetClipboardData(RegisterClipboardFormatW(L"PNG"), global(png.data(), png.size()));
    CloseClipboard();
    GdkContentFormats* formats = gdk_clipboard_get_formats(desktop());
    check(formats && gdk_content_formats_contain_mime_type(formats, "image/png"), "offered as image/png");
    GdkTexture* texture = readTexture();
    check(texture && gdk_texture_get_width(texture) == 3 && gdk_texture_get_height(texture) == 2,
          "the image reads back at 3x2");
    if (texture) g_object_unref(texture);

    std::cout << "clipboard bridge: " << failures << " failures\n";
    return failures ? 1 : 0;
}
