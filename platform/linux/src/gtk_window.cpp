// GTK4 behind Tinta's main window: an undecorated GtkWindow (Tinta draws its
// own caption) holding a drawing area that presents the window's Cairo
// surface. Input becomes the Win32 messages WndProc handles, posted to the
// queue as Windows would; the GLib main loop runs inside GetMessage.
#include "window_internal.h"

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <unistd.h>

namespace tinta {
namespace gtk {
namespace {

bool g_active = false;
bool g_inDraw = false;  // presenting from the draw callback: no new frame
int g_modal = 0;        // nested loops (dialogs, clipboard): no app code runs
POINT g_pointer{0, 0};  // last pointer position, client pixels
std::map<UINT32, wchar_t> g_keyChars;  // WM_KEYDOWN sequence -> its WM_CHAR
std::set<guint> g_keysDown;            // hardware keycodes held (autorepeat bit)
UINT32 g_keySeq = 0;
bool g_imFiltering = false;  // inside filter_keypress: commits are the key's
std::string g_imCommitted;   // what the input method committed meanwhile

struct Native {
    HWND hwnd = nullptr;
    GtkWindow* window = nullptr;
    GtkWidget* area = nullptr;
    double scale = 1.0;
    // The press a caption move or resize starts from (surface coordinates)
    GdkDevice* device = nullptr;
    int button = 0;
    double pressX = 0, pressY = 0;
    guint32 pressTime = 0;
    bool nonClientPress = false;
    // Non-client double clicks (Windows sends them without CS_DBLCLKS)
    guint32 lastNcPressTime = 0;
    double lastNcX = -100, lastNcY = -100;
    bool overClient = false;  // last hit test, for WM_NCMOUSELEAVE / WM_MOUSELEAVE
    WPARAM buttons = 0;       // MK_ flags held
    GtkIMContext* im = nullptr;  // input methods: dead keys, compose, IBus/Fcitx
};

std::vector<Native*> g_natives;

Native* nativeOf(HWND hwnd) { return hwnd ? static_cast<Native*>(hwnd->native) : nullptr; }

std::string utf8(const wchar_t* text) { return text ? toUtf8(text, wcslen(text)) : std::string(); }

double surfaceScale(Native* n) {
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(n->window));
    return surface ? gdk_surface_get_scale(surface) : n->scale;
}

LPARAM pointLParam(Native* n, double x, double y) {
    int px = static_cast<int>(std::lround(x * n->scale)), py = static_cast<int>(std::lround(y * n->scale));
    g_pointer = {px, py};
    return MAKELPARAM(px, py);
}

WPARAM modifierFlags(GdkModifierType state) {
    WPARAM flags = 0;
    if (state & GDK_SHIFT_MASK) flags |= MK_SHIFT;
    if (state & GDK_CONTROL_MASK) flags |= MK_CONTROL;
    return flags;
}

// Key state from the event's modifiers (GetKeyState reads it)
void syncModifiers(GdkModifierType state) {
    tintaSetKeyState(VK_SHIFT, (state & GDK_SHIFT_MASK) != 0);
    tintaSetKeyState(VK_CONTROL, (state & GDK_CONTROL_MASK) != 0);
    tintaSetKeyState(VK_MENU, (state & GDK_ALT_MASK) != 0);
}

// ---- Painting and size -----------------------------------------------------------------
void draw(GtkDrawingArea*, cairo_t* cr, int, int, gpointer data) {
    auto* n = static_cast<Native*>(data);
    g_inDraw = true;
    if (n->hwnd->dirty && g_modal == 0) SendMessageW(n->hwnd, WM_PAINT, 0, 0);
    g_inDraw = false;
    cairo_surface_t* surface = windowSurface(n->hwnd);
    if (!surface) return;
    cairo_save(cr);
    cairo_scale(cr, 1.0 / n->scale, 1.0 / n->scale);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
}

void resized(GtkDrawingArea*, int width, int height, gpointer data) {
    auto* n = static_cast<Native*>(data);
    double scale = surfaceScale(n);
    if (g_modal == 0 && std::fabs(scale - n->scale) > 0.001) {
        n->scale = scale;
        UINT dpi = static_cast<UINT>(std::lround(96.0 * scale));
        RECT suggested = n->hwnd->rect;
        SendMessageW(n->hwnd, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), reinterpret_cast<LPARAM>(&suggested));
    }
    int px = static_cast<int>(std::ceil(width * n->scale)), py = static_cast<int>(std::ceil(height * n->scale));
    n->hwnd->rect = {0, 0, px, py};
    if (g_getenv("TINTA_TRACE_GTK")) {
        std::fprintf(stderr, "tinta-gtk: resize %dx%d logical, scale %.3f, %dx%d pixels\n", width, height, n->scale,
                     px, py);
    }
    WPARAM kind = gtk_window_is_maximized(n->window) ? SIZE_MAXIMIZED : SIZE_RESTORED;
    if (g_modal) postFromUi(n->hwnd, WM_SIZE, kind, MAKELPARAM(px, py));
    else SendMessageW(n->hwnd, WM_SIZE, kind, MAKELPARAM(px, py));
    InvalidateRect(n->hwnd, nullptr, FALSE);
}

// ---- Mouse -----------------------------------------------------------------------------------
UINT hitTest(Native* n, LPARAM point) {
    return static_cast<UINT>(SendMessageW(n->hwnd, WM_NCHITTEST, 0, point));
}

// Surface coordinates to the drawing area's (the window's only child)
void toArea(Native* n, double& x, double& y) {
    double sx = 0, sy = 0;
    gtk_native_get_surface_transform(GTK_NATIVE(n->window), &sx, &sy);
    x -= sx;
    y -= sy;
}

void buttonPressed(Native* n, GdkEvent* event, int button, double sx, double sy) {
    GdkModifierType state = gdk_event_get_modifier_state(event);
    syncModifiers(state);
    gtk_widget_grab_focus(n->area);
    double x = sx, y = sy;
    toArea(n, x, y);
    LPARAM point = pointLParam(n, x, y);
    n->device = gdk_event_get_device(event);
    n->button = button;
    n->pressX = sx;
    n->pressY = sy;
    n->pressTime = gdk_event_get_time(event);
    UINT hit = hitTest(n, point);
    n->nonClientPress = hit != HTCLIENT;
    if (n->nonClientPress) {
        if (button != 1) return;
        int doubleTime = 400, doubleDistance = 5;
        g_object_get(gtk_settings_get_default(), "gtk-double-click-time", &doubleTime, "gtk-double-click-distance",
                     &doubleDistance, nullptr);
        bool twice = n->pressTime - n->lastNcPressTime <= static_cast<guint32>(doubleTime) &&
                     std::fabs(x - n->lastNcX) <= doubleDistance && std::fabs(y - n->lastNcY) <= doubleDistance;
        n->lastNcPressTime = twice ? 0 : n->pressTime;
        n->lastNcX = x;
        n->lastNcY = y;
        postFromUi(n->hwnd, twice ? WM_NCLBUTTONDBLCLK : WM_NCLBUTTONDOWN, hit, point);
        return;
    }
    WPARAM mods = modifierFlags(state);
    switch (button) {
        case 1: n->buttons |= MK_LBUTTON; postFromUi(n->hwnd, WM_LBUTTONDOWN, mods | n->buttons, point); break;
        case 2: n->buttons |= MK_MBUTTON; postFromUi(n->hwnd, WM_MBUTTONDOWN, mods | n->buttons, point); break;
        case 3: n->buttons |= MK_RBUTTON; postFromUi(n->hwnd, WM_RBUTTONDOWN, mods | n->buttons, point); break;
        case 8:
        case 9:
            postFromUi(n->hwnd, WM_XBUTTONDOWN, MAKEWPARAM(mods, button == 8 ? XBUTTON1 : XBUTTON2), point);
            break;
        default: break;
    }
}

void buttonReleased(Native* n, GdkEvent* event, int button, double x, double y) {
    GdkModifierType state = gdk_event_get_modifier_state(event);
    syncModifiers(state);
    toArea(n, x, y);
    LPARAM point = pointLParam(n, x, y);
    if (n->nonClientPress) {
        n->nonClientPress = false;
        UINT hit = hitTest(n, point);
        if (button == 1) postFromUi(n->hwnd, WM_NCLBUTTONUP, hit, point);
        if (button == 3) postFromUi(n->hwnd, WM_NCRBUTTONUP, hit, point);
        return;
    }
    WPARAM mods = modifierFlags(state);
    switch (button) {
        case 1:
            n->buttons &= ~static_cast<WPARAM>(MK_LBUTTON);
            postFromUi(n->hwnd, WM_LBUTTONUP, mods | n->buttons, point);
            break;
        case 2:
            n->buttons &= ~static_cast<WPARAM>(MK_MBUTTON);
            postFromUi(n->hwnd, WM_MBUTTONUP, mods | n->buttons, point);
            break;
        case 3:
            n->buttons &= ~static_cast<WPARAM>(MK_RBUTTON);
            postFromUi(n->hwnd, WM_RBUTTONUP, mods | n->buttons, point);
            break;
        case 8:
        case 9:
            postFromUi(n->hwnd, WM_XBUTTONUP, MAKEWPARAM(mods, button == 8 ? XBUTTON1 : XBUTTON2), point);
            break;
        default: break;
    }
}

void pointerMoved(Native* n, GdkEvent* event, double x, double y) {
    GdkModifierType state = gdk_event_get_modifier_state(event);
    toArea(n, x, y);
    LPARAM point = pointLParam(n, x, y);
    if (n->buttons) {  // a drag: the client keeps the pointer, as under capture
        postFromUi(n->hwnd, WM_MOUSEMOVE, modifierFlags(state) | n->buttons, point);
        return;
    }
    UINT hit = hitTest(n, point);
    SendMessageW(n->hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(n->hwnd), MAKELPARAM(hit, WM_MOUSEMOVE));
    bool client = hit == HTCLIENT;
    if (client != n->overClient) postFromUi(n->hwnd, client ? WM_NCMOUSELEAVE : WM_MOUSELEAVE, 0, 0);
    n->overClient = client;
    if (client) postFromUi(n->hwnd, WM_MOUSEMOVE, modifierFlags(state), point);
    else postFromUi(n->hwnd, WM_NCMOUSEMOVE, hit, point);
}

// Raw button and motion events: a click gesture can swallow the release that
// ends a drag
gboolean pointerEvent(GtkEventControllerLegacy*, GdkEvent* event, gpointer data) {
    auto* n = static_cast<Native*>(data);
    if (g_modal) return FALSE;
    GdkEventType type = gdk_event_get_event_type(event);
    if (type != GDK_BUTTON_PRESS && type != GDK_BUTTON_RELEASE && type != GDK_MOTION_NOTIFY) return FALSE;
    double x = 0, y = 0;
    if (!gdk_event_get_position(event, &x, &y)) return FALSE;
    if (type == GDK_MOTION_NOTIFY) {
        pointerMoved(n, event, x, y);
        return FALSE;
    }
    int button = static_cast<int>(gdk_button_event_get_button(event));
    if (type == GDK_BUTTON_PRESS) buttonPressed(n, event, button, x, y);
    else buttonReleased(n, event, button, x, y);
    return TRUE;
}

void left(GtkEventControllerMotion*, gpointer data) {
    auto* n = static_cast<Native*>(data);
    if (n->buttons) return;  // the drag still owns the pointer
    postFromUi(n->hwnd, n->overClient ? WM_MOUSELEAVE : WM_NCMOUSELEAVE, 0, 0);
    n->overClient = false;
}

// ---- Tab drags (tintaBeginTabDrag) ----------------------------------------------------------------
// The payload: the tab's path (UTF-8), a newline, a receipt path. A Tinta
// window that takes the tab creates the receipt; only then has the tab
// moved. Other applications may accept any drop (Chromium-based ones
// report a move), and a tab must not vanish into them.
const char kTabMime[] = "application/x-tinta-tab";

struct OutgoingTabDrag {
    GdkDrag* drag = nullptr;
    Native* source = nullptr;
    UINT message = 0;
    int stripHeight = 0;    // logical px: the source window's strip
    bool performed = false;
    bool returned = false;  // dropped back on its own strip
    bool escaped = false;   // Esc pressed while it ran
    std::string receipt;    // created by the Tinta window that takes the tab
} g_tabDrag;

void cairoColour(cairo_t* cr, COLORREF c, double alpha) {
    cairo_set_source_rgba(cr, GetRValue(c) / 255.0, GetGValue(c) / 255.0, GetBValue(c) / 255.0, alpha);
}

// The tab as a floating card, drawn at the window's scale
GdkTexture* tabCardTexture(const TintaTabDragCard& card, double scale) {
    int width = static_cast<int>(std::ceil(card.width * scale));
    int height = static_cast<int>(std::ceil(card.height * scale));
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(surface);
    cairo_scale(cr, scale, scale);
    double w = card.width, h = card.height, r = card.radius;
    cairo_new_sub_path(cr);
    cairo_arc(cr, w - r - 0.5, r + 0.5, r, -G_PI / 2, 0);
    cairo_arc(cr, w - r - 0.5, h - r - 0.5, r, 0, G_PI / 2);
    cairo_arc(cr, r + 0.5, h - r - 0.5, r, G_PI / 2, G_PI);
    cairo_arc(cr, r + 0.5, r + 0.5, r, G_PI, 3 * G_PI / 2);
    cairo_close_path(cr);
    cairoColour(cr, card.fill, 0.94);
    cairo_fill_preserve(cr);
    cairoColour(cr, card.border, 0.35);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* font = pango_font_description_from_string("Selawik, Segoe UI, Noto Sans, sans-serif");
    pango_font_description_set_absolute_size(font, 13.0 * PANGO_SCALE);
    pango_layout_set_font_description(layout, font);
    pango_font_description_free(font);
    pango_layout_set_text(layout, utf8(card.title).c_str(), -1);
    pango_layout_set_width(layout, static_cast<int>((w - 24) * PANGO_SCALE));
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    pango_layout_set_single_paragraph_mode(layout, TRUE);
    int textW = 0, textH = 0;
    pango_layout_get_pixel_size(layout, &textW, &textH);
    cairoColour(cr, card.text, 1.0);
    cairo_move_to(cr, 12, (h - textH) / 2.0);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_flush(surface);
    int stride = cairo_image_surface_get_stride(surface);
    GBytes* bytes = g_bytes_new(cairo_image_surface_get_data(surface), static_cast<gsize>(stride) * height);
    cairo_surface_destroy(surface);
    // Cairo's ARGB32 is premultiplied BGRA in memory: GDK_MEMORY_DEFAULT
    GdkTexture* texture = gdk_memory_texture_new(width, height, GDK_MEMORY_DEFAULT, bytes, stride);
    g_bytes_unref(bytes);
    return texture;
}

void tabDragFinished(int outcome) {
    if (!g_tabDrag.drag) return;
    if (!g_tabDrag.receipt.empty()) g_remove(g_tabDrag.receipt.c_str());
    if (std::find(g_natives.begin(), g_natives.end(), g_tabDrag.source) != g_natives.end()) {
        postFromUi(g_tabDrag.source->hwnd, g_tabDrag.message, static_cast<WPARAM>(outcome), 0);
    }
    g_object_unref(g_tabDrag.drag);
    g_tabDrag = OutgoingTabDrag{};
}

// The drop was taken. A Tinta window writes its receipt within moments;
// an application that swallows any drop may keep the drag open for many
// seconds before it finishes, so the outcome does not wait for it
void tabDragPerformed(GdkDrag*, gpointer) {
    g_tabDrag.performed = true;
    if (g_tabDrag.returned) return;
    struct Poll {
        GdkDrag* drag;
        int ticks = 0;
    };
    g_timeout_add(100, +[](gpointer data) -> gboolean {
        auto* poll = static_cast<Poll*>(data);
        if (g_tabDrag.drag != poll->drag) {  // already decided
            delete poll;
            return G_SOURCE_REMOVE;
        }
        bool taken = g_file_test(g_tabDrag.receipt.c_str(), G_FILE_TEST_EXISTS);
        if (taken || ++poll->ticks >= 20) {
            tabDragFinished(taken ? TINTA_TAB_DRAG_MOVED : TINTA_TAB_DRAG_NO_TARGET);
            delete poll;
            return G_SOURCE_REMOVE;
        }
        return G_SOURCE_CONTINUE;
    }, new Poll{g_tabDrag.drag});
}

void tabDragDone(GdkDrag* drag, gpointer) {
    if (g_tabDrag.drag != drag) {  // decided already (tabDragPerformed)
        gdk_drag_drop_done(drag, TRUE);
        return;
    }
    if (g_getenv("TINTA_TRACE_GTK")) {
        std::fprintf(stderr, "tinta-gtk: tab drag finished, performed %d returned %d\n", g_tabDrag.performed,
                     g_tabDrag.returned);
    }
    gdk_drag_drop_done(drag, TRUE);
    // Taken by a Tinta window only with its receipt; dropped on any other
    // application, the tab tears off as if released over nothing
    bool taken = g_tabDrag.performed && !g_tabDrag.returned &&
                 g_file_test(g_tabDrag.receipt.c_str(), G_FILE_TEST_EXISTS);
    int outcome = g_tabDrag.returned ? TINTA_TAB_DRAG_CANCELLED
                  : taken            ? TINTA_TAB_DRAG_MOVED
                                     : TINTA_TAB_DRAG_NO_TARGET;
    tabDragFinished(outcome);
}

bool ownTabDrag(GdkDrop* drop, Native* n) {
    return g_tabDrag.drag && gdk_drop_get_drag(drop) == g_tabDrag.drag && g_tabDrag.source == n;
}

void tabDragCancelled(GdkDrag* drag, GdkDragCancelReason reason, gpointer) {
    if (g_tabDrag.drag != drag) return;  // decided already
    if (g_getenv("TINTA_TRACE_GTK")) std::fprintf(stderr, "tinta-gtk: tab drag cancelled, reason %d\n", reason);
    // Nowhere to land is a tear-off, so no animation back to the strip.
    // Wayland reports a drop on nothing as an error, as it does Esc: only an
    // Esc seen here keeps the tab
    bool tearOff = reason == GDK_DRAG_CANCEL_NO_TARGET || (reason == GDK_DRAG_CANCEL_ERROR && !g_tabDrag.escaped);
    gdk_drag_drop_done(drag, tearOff ? TRUE : FALSE);
    tabDragFinished(tearOff ? TINTA_TAB_DRAG_NO_TARGET : TINTA_TAB_DRAG_CANCELLED);
}

// Every Tinta window takes tabs. The one a drag came from takes it back
// only over its strip (the tab stays put); released anywhere else on it,
// the tab tears off, as on Windows
gboolean tabDropAccept(GtkDropTargetAsync*, GdkDrop* drop, gpointer) {
    return !g_modal && gdk_content_formats_contain_mime_type(gdk_drop_get_formats(drop), kTabMime);
}

GdkDragAction tabDropMotion(GtkDropTargetAsync*, GdkDrop* drop, double, double y, gpointer data) {
    auto* n = static_cast<Native*>(data);
    if (ownTabDrag(drop, n) && y > g_tabDrag.stripHeight) return static_cast<GdkDragAction>(0);
    return GDK_ACTION_MOVE;
}

struct TabDropRead {
    Native* n;
    GdkDrop* drop;
    double x, y;
};

// The payload has arrived: hand the tab to the window
void tabDropDeliver(TabDropRead* read, const std::string& payload) {
    bool taken = false;
    size_t newline = payload.find('\n');
    std::string path = payload.substr(0, newline);
    std::string receipt = newline == std::string::npos ? std::string() : payload.substr(newline + 1);
    if (!path.empty() && std::find(g_natives.begin(), g_natives.end(), read->n) != g_natives.end()) {
        // As tabSendDrop does on Windows: the point, in screen pixels, then
        // the path and its NUL
        POINT point{static_cast<LONG>(std::lround(read->x * read->n->scale)),
                    static_cast<LONG>(std::lround(read->y * read->n->scale))};
        ClientToScreen(read->n->hwnd, &point);
        std::string bytes(sizeof point, '\0');
        std::memcpy(bytes.data(), &point, sizeof point);
        bytes += path;
        bytes += '\0';
        COPYDATASTRUCT copy{};
        copy.dwData = 2;
        copy.cbData = static_cast<DWORD>(bytes.size());
        copy.lpData = bytes.data();
        taken = SendMessageW(read->n->hwnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&copy)) != FALSE;
        if (taken) {
            // The source closes its tab only on this receipt
            if (receipt.rfind(g_get_user_runtime_dir(), 0) == 0) g_file_set_contents(receipt.c_str(), "", 0, nullptr);
            gtk_window_present(read->n->window);
        }
    }
    gdk_drop_finish(read->drop, taken ? GDK_ACTION_MOVE : static_cast<GdkDragAction>(0));
    g_object_unref(read->drop);
    delete read;
}

// Read asynchronously: on X11 the bytes arrive through this loop's own
// events, so a blocking read would wait on itself
void tabDropSpliced(GObject* source, GAsyncResult* result, gpointer data) {
    auto* read = static_cast<TabDropRead*>(data);
    auto* memory = G_MEMORY_OUTPUT_STREAM(source);
    std::string payload;
    if (g_output_stream_splice_finish(G_OUTPUT_STREAM(memory), result, nullptr) >= 0) {
        payload.assign(static_cast<const char*>(g_memory_output_stream_get_data(memory)),
                       g_memory_output_stream_get_data_size(memory));
    }
    g_object_unref(memory);
    tabDropDeliver(read, payload);
}

void tabDropRead(GObject* source, GAsyncResult* result, gpointer data) {
    auto* read = static_cast<TabDropRead*>(data);
    GInputStream* stream = gdk_drop_read_finish(GDK_DROP(source), result, nullptr, nullptr);
    if (!stream) {
        tabDropDeliver(read, std::string());
        return;
    }
    GOutputStream* memory = g_memory_output_stream_new_resizable();
    g_output_stream_splice_async(
        memory, stream,
        static_cast<GOutputStreamSpliceFlags>(G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
        G_PRIORITY_DEFAULT, nullptr, tabDropSpliced, read);
    g_object_unref(stream);
}

gboolean tabDropped(GtkDropTargetAsync*, GdkDrop* drop, double x, double y, gpointer data) {
    auto* n = static_cast<Native*>(data);
    if (ownTabDrag(drop, n)) {
        // Back on its own strip: nothing moves
        if (y > g_tabDrag.stripHeight) return FALSE;
        g_tabDrag.returned = true;
        gdk_drop_finish(drop, GDK_ACTION_MOVE);
        return TRUE;
    }
    static const char* mimes[] = {kTabMime, nullptr};
    auto* read = new TabDropRead{n, GDK_DROP(g_object_ref(drop)), x, y};
    gdk_drop_read_async(drop, mimes, G_PRIORITY_DEFAULT, nullptr, tabDropRead, read);
    return TRUE;
}

// Files dropped from a file manager: WM_DROPFILES, as for a window with
// WS_EX_ACCEPTFILES on Windows (Tinta opens them in tabs)
gboolean dropped(GtkDropTarget*, const GValue* value, double, double, gpointer data) {
    auto* n = static_cast<Native*>(data);
    if (g_modal || !G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST)) return FALSE;
    auto* drop = new HDROP__();
    GSList* files = gdk_file_list_get_files(static_cast<GdkFileList*>(g_value_get_boxed(value)));
    for (GSList* f = files; f; f = f->next) {
        if (char* path = g_file_get_path(G_FILE(f->data))) {  // local files only
            drop->files.push_back(toWide(std::string(path)));
            g_free(path);
        }
    }
    g_slist_free(files);
    if (drop->files.empty()) {
        delete drop;
        return FALSE;
    }
    postFromUi(n->hwnd, WM_DROPFILES, reinterpret_cast<WPARAM>(drop), 0);
    return TRUE;
}

gboolean scrolled(GtkEventControllerScroll* controller, double dx, double dy, gpointer data) {
    auto* n = static_cast<Native*>(data);
    if (g_modal) return FALSE;
    static double carryX = 0, carryY = 0;
    GdkModifierType state = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(controller));
    syncModifiers(state);
    // A wheel notch is 120; touchpads report pixels (about 40 to a notch)
    double perUnit = gtk_event_controller_scroll_get_unit(controller) == GDK_SCROLL_UNIT_WHEEL ? 120.0 : 3.0;
    carryY += -dy * perUnit;
    carryX += dx * perUnit;
    LPARAM point = MAKELPARAM(g_pointer.x, g_pointer.y);
    if (std::fabs(carryY) >= 1.0) {
        int delta = static_cast<int>(carryY);
        carryY -= delta;
        postFromUi(n->hwnd, WM_MOUSEWHEEL, MAKEWPARAM(modifierFlags(state), static_cast<WORD>(delta)), point);
    }
    if (std::fabs(carryX) >= 1.0) {
        int delta = static_cast<int>(carryX);
        carryX -= delta;
        postFromUi(n->hwnd, WM_MOUSEHWHEEL, MAKEWPARAM(modifierFlags(state), static_cast<WORD>(delta)), point);
    }
    return TRUE;
}

// ---- Keyboard ---------------------------------------------------------------------------------
UINT virtualKey(guint keyval) {
    if (keyval >= GDK_KEY_a && keyval <= GDK_KEY_z) return 'A' + (keyval - GDK_KEY_a);
    if (keyval >= GDK_KEY_A && keyval <= GDK_KEY_Z) return 'A' + (keyval - GDK_KEY_A);
    if (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9) return '0' + (keyval - GDK_KEY_0);
    if (keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F24) return VK_F1 + (keyval - GDK_KEY_F1);
    if (keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9) return VK_NUMPAD0 + (keyval - GDK_KEY_KP_0);
    switch (keyval) {
        case GDK_KEY_Return: case GDK_KEY_KP_Enter: return VK_RETURN;
        case GDK_KEY_BackSpace: return VK_BACK;
        case GDK_KEY_Tab: case GDK_KEY_ISO_Left_Tab: return VK_TAB;
        case GDK_KEY_Escape: return VK_ESCAPE;
        case GDK_KEY_space: return VK_SPACE;
        case GDK_KEY_Delete: case GDK_KEY_KP_Delete: return VK_DELETE;
        case GDK_KEY_Insert: case GDK_KEY_KP_Insert: return VK_INSERT;
        case GDK_KEY_Home: case GDK_KEY_KP_Home: return VK_HOME;
        case GDK_KEY_End: case GDK_KEY_KP_End: return VK_END;
        case GDK_KEY_Page_Up: case GDK_KEY_KP_Page_Up: return VK_PRIOR;
        case GDK_KEY_Page_Down: case GDK_KEY_KP_Page_Down: return VK_NEXT;
        case GDK_KEY_Left: case GDK_KEY_KP_Left: return VK_LEFT;
        case GDK_KEY_Right: case GDK_KEY_KP_Right: return VK_RIGHT;
        case GDK_KEY_Up: case GDK_KEY_KP_Up: return VK_UP;
        case GDK_KEY_Down: case GDK_KEY_KP_Down: return VK_DOWN;
        case GDK_KEY_Shift_L: case GDK_KEY_Shift_R: return VK_SHIFT;
        case GDK_KEY_Control_L: case GDK_KEY_Control_R: return VK_CONTROL;
        case GDK_KEY_Alt_L: case GDK_KEY_Alt_R: case GDK_KEY_Meta_L: case GDK_KEY_Meta_R: return VK_MENU;
        case GDK_KEY_Super_L: case GDK_KEY_Super_R: return VK_LWIN;
        case GDK_KEY_Menu: return VK_APPS;
        case GDK_KEY_comma: return VK_OEM_COMMA;
        case GDK_KEY_period: return VK_OEM_PERIOD;
        case GDK_KEY_minus: return VK_OEM_MINUS;
        case GDK_KEY_equal: return VK_OEM_PLUS;
        case GDK_KEY_slash: return VK_OEM_2;
        case GDK_KEY_semicolon: return VK_OEM_1;
        case GDK_KEY_apostrophe: return VK_OEM_7;
        case GDK_KEY_bracketleft: return VK_OEM_4;
        case GDK_KEY_bracketright: return VK_OEM_6;
        case GDK_KEY_backslash: return VK_OEM_5;
        case GDK_KEY_grave: return VK_OEM_3;
        case GDK_KEY_KP_Add: return VK_ADD;
        case GDK_KEY_KP_Subtract: return VK_SUBTRACT;
        case GDK_KEY_KP_Multiply: return VK_MULTIPLY;
        case GDK_KEY_KP_Divide: return VK_DIVIDE;
        default: return 0;
    }
}

// The key's unshifted symbol on the active layout: shortcuts name keys, not
// the characters Shift gives them
guint baseKeyval(GtkEventControllerKey* controller, guint keycode, guint keyval) {
    GdkEvent* event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
    guint base = 0;
    if (event && gdk_display_translate_key(gdk_event_get_display(event), keycode, static_cast<GdkModifierType>(0),
                                           static_cast<int>(gdk_key_event_get_layout(event)), &base, nullptr,
                                           nullptr, nullptr)) {
        if (virtualKey(base)) return base;
    }
    return keyval;
}

// Text as WM_CHAR, one UTF-16 unit each (a surrogate pair is two, as on
// Windows)
void postText(Native* n, const std::string& utf8) {
    std::wstring text = toWide(utf8);
    for (wchar_t c : text) postFromUi(n->hwnd, WM_CHAR, static_cast<WPARAM>(c), 1);
}

void imCommitted(GtkIMContext*, const gchar* text, gpointer data) {
    if (g_imFiltering) {
        g_imCommitted += text;  // keyPressed decides
    } else {
        postText(static_cast<Native*>(data), text);  // IBus/Fcitx, Wayland text-input
    }
}

// Tinta positions the candidate window on WM_IME_STARTCOMPOSITION
// (ImmSetCompositionWindow, below)
void imPreeditStarted(GtkIMContext*, gpointer data) {
    postFromUi(static_cast<Native*>(data)->hwnd, WM_IME_STARTCOMPOSITION, 0, 0);
}

gboolean keyPressed(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state,
                    gpointer data) {
    auto* n = static_cast<Native*>(data);
    if (g_modal) return FALSE;
    // Esc during a tab drag keeps the tab (and must not reach Tinta, where
    // Esc in the reader closes the window)
    if (g_tabDrag.drag && keyval == GDK_KEY_Escape) {
        g_tabDrag.escaped = true;
        return TRUE;
    }
    syncModifiers(state);
    UINT vk = virtualKey(baseKeyval(controller, keycode, keyval));
    if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) tintaSetKeyState(static_cast<int>(vk), true);
    if (!vk) vk = 0xE5;  // VK_PROCESSKEY: a key with text but no virtual key
    // TranslateMessage gives Tinta the character only if it declines the key
    UINT32 seq = ++g_keySeq;
    bool ctrl = (state & GDK_CONTROL_MASK) != 0, alt = (state & GDK_ALT_MASK) != 0;
    // The input method sees plain keys first (shortcuts never). Committing
    // just the key's own character changes nothing; composed text (a dead
    // key's accent, a CJK phrase) arrives as WM_CHAR; a key it keeps for a
    // composition sends nothing, as an IME does on Windows
    if (n->im && !ctrl && !alt) {
        GdkEvent* event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
        g_imFiltering = true;
        g_imCommitted.clear();
        bool consumed = event && gtk_im_context_filter_keypress(n->im, event);
        g_imFiltering = false;
        if (consumed) {
            gunichar own = gdk_keyval_to_unicode(keyval);
            char ownUtf8[8] = {};
            if (own) g_unichar_to_utf8(own, ownUtf8);
            if (g_imCommitted.empty()) return TRUE;  // composing
            if (g_imCommitted != ownUtf8) {
                postText(n, g_imCommitted);
                return TRUE;
            }
        }
    }
    gunichar ch = gdk_keyval_to_unicode(keyval);
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) ch = '\r';
    else if (keyval == GDK_KEY_BackSpace) ch = 8;
    else if (keyval == GDK_KEY_Tab || keyval == GDK_KEY_ISO_Left_Tab) ch = '\t';
    else if (keyval == GDK_KEY_Escape) ch = 27;
    else if (keyval == GDK_KEY_Delete || keyval == GDK_KEY_KP_Delete) ch = 0;
    if (ch && ch < 0x10000 && !ctrl && !alt) g_keyChars[seq] = static_cast<wchar_t>(ch);
    bool system = alt || vk == VK_MENU || vk == VK_F10;
    // Bit 29: Alt held; bit 30: autorepeat; above bit 32: the WM_CHAR sequence
    LPARAM lParam = 1 | (static_cast<LPARAM>(seq) << 32);
    if (alt) lParam |= 1 << 29;
    if (!g_keysDown.insert(keycode).second) lParam |= 1 << 30;
    postFromUi(n->hwnd, system ? WM_SYSKEYDOWN : WM_KEYDOWN, vk, lParam);
    return TRUE;
}

void keyReleased(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state,
                 gpointer data) {
    auto* n = static_cast<Native*>(data);
    g_keysDown.erase(keycode);
    if (g_modal) return;
    if (n->im) {
        if (GdkEvent* event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller))) {
            gtk_im_context_filter_keypress(n->im, event);
        }
    }
    UINT vk = virtualKey(baseKeyval(controller, keycode, keyval));
    syncModifiers(state);
    if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) tintaSetKeyState(static_cast<int>(vk), false);
    if (vk) postFromUi(n->hwnd, (state & GDK_ALT_MASK) || vk == VK_MENU ? WM_SYSKEYUP : WM_KEYUP, vk, 0xC0000001);
}

// ---- Window ------------------------------------------------------------------------------------
gboolean closeRequested(GtkWindow*, gpointer data) {
    auto* n = static_cast<Native*>(data);
    postFromUi(n->hwnd, WM_CLOSE, 0, 0);  // Tinta decides (unsaved changes)
    return TRUE;
}

void activeChanged(GObject* window, GParamSpec*, gpointer data) {
    auto* n = static_cast<Native*>(data);
    bool active = gtk_window_is_active(GTK_WINDOW(window));
    postFromUi(n->hwnd, active ? WM_SETFOCUS : WM_KILLFOCUS, 0, 0);
    if (n->im) {
        if (active) gtk_im_context_focus_in(n->im);
        else gtk_im_context_focus_out(n->im);
    }
    if (!active) {
        n->buttons = 0;
        g_keysDown.clear();
        syncModifiers(static_cast<GdkModifierType>(0));
    }
}

double monitorScale() {
    GListModel* monitors = gdk_display_get_monitors(gdk_display_get_default());
    if (!monitors || g_list_model_get_n_items(monitors) == 0) return 1.0;
    auto* monitor = static_cast<GdkMonitor*>(g_list_model_get_item(monitors, 0));
    double scale = gdk_monitor_get_scale(monitor);
    g_object_unref(monitor);
    return scale > 0 ? scale : 1.0;
}

// Runs the main loop until `done`
void runUntil(const bool& done) {
    g_modal++;
    while (!done) g_main_context_iteration(nullptr, TRUE);
    g_modal--;
}

GtkWindow* parentWindow(HWND hwnd) {
    Native* n = nativeOf(hwnd);
    if (!n && !g_natives.empty()) n = g_natives.front();
    return n ? n->window : nullptr;
}

GdkClipboard* clipboard() { return gdk_display_get_clipboard(gdk_display_get_default()); }

// The desktop switched light/dark: Windows announces that as
// WM_SETTINGCHANGE("ImmersiveColorSet"), and Tinta re-reads the scheme
// (RegGetValueW AppsUseLightTheme, from the same portal)
void colorSchemeChanged(GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*,
                        GVariant* parameters, gpointer) {
    const gchar* ns = nullptr;
    const gchar* key = nullptr;
    g_variant_get(parameters, "(&s&sv)", &ns, &key, nullptr);
    if (g_strcmp0(ns, "org.freedesktop.appearance") != 0 || g_strcmp0(key, "color-scheme") != 0) return;
    static const wchar_t kImmersiveColorSet[] = L"ImmersiveColorSet";
    for (Native* n : g_natives) {
        postFromUi(n->hwnd, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(kImmersiveColorSet));
    }
}

// ---- Single instance: WM_COPYDATA across processes ------------------------------------
const char kInstanceName[] = "org.tinta.Instance";
const char kInstancePath[] = "/org/tinta/Instance";
const char kInstanceXml[] =
    "<node><interface name='org.tinta.Instance'><method name='CopyData'>"
    "<arg type='t' name='kind' direction='in'/><arg type='ay' name='data' direction='in'/>"
    "<arg type='s' name='activation' direction='in'/><arg type='b' name='handled' direction='out'/>"
    "</method></interface></node>";

GDBusConnection* g_bus = nullptr;
std::string g_startupId;  // the sender's activation token, for present()

struct RemoteCopy {
    guint64 kind;
    std::vector<unsigned char> data;
    std::string activation;
};
std::vector<RemoteCopy> g_pendingCopies;

HWND primaryWindow() { return g_natives.empty() ? nullptr : g_natives.front()->hwnd; }

LRESULT deliverCopy(const RemoteCopy& copy) {
    HWND hwnd = primaryWindow();
    if (!hwnd) return FALSE;
    COPYDATASTRUCT data{};
    data.dwData = static_cast<ULONG_PTR>(copy.kind);
    data.cbData = static_cast<DWORD>(copy.data.size());
    data.lpData = const_cast<unsigned char*>(copy.data.data());
    g_startupId = copy.activation;
    LRESULT handled = SendMessageW(hwnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data));
    g_startupId.clear();
    return handled;
}

// Copies that arrived inside a dialog's nested loop or before the window
gboolean drainCopies(gpointer) {
    if (g_modal || !primaryWindow()) return G_SOURCE_CONTINUE;
    std::vector<RemoteCopy> pending;
    pending.swap(g_pendingCopies);
    for (const RemoteCopy& copy : pending) deliverCopy(copy);
    return G_SOURCE_REMOVE;
}

void instanceCall(GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* parameters,
                  GDBusMethodInvocation* invocation, gpointer) {
    guint64 kind = 0;
    GVariant* bytes = nullptr;
    const gchar* activation = nullptr;
    g_variant_get(parameters, "(t@ay&s)", &kind, &bytes, &activation);
    gsize size = 0;
    const auto* raw = static_cast<const unsigned char*>(g_variant_get_fixed_array(bytes, &size, 1));
    RemoteCopy copy{kind, std::vector<unsigned char>(raw, raw + size), activation ? activation : ""};
    g_variant_unref(bytes);
    gboolean handled = TRUE;
    if (g_modal || !primaryWindow()) {  // app code is on the stack, or no window yet
        if (g_pendingCopies.empty()) g_timeout_add(100, drainCopies, nullptr);
        g_pendingCopies.push_back(std::move(copy));
    } else {
        handled = deliverCopy(copy) != 0;
    }
    g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", handled));
}

// Every interactive Tinta queues for the name: the first owns it, and when
// it exits the next one takes over
void ownInstanceName() {
    g_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!g_bus) return;
    GDBusNodeInfo* info = g_dbus_node_info_new_for_xml(kInstanceXml, nullptr);
    static const GDBusInterfaceVTable vtable = {instanceCall, nullptr, nullptr, {}};
    g_dbus_connection_register_object(g_bus, kInstancePath, info->interfaces[0], &vtable, nullptr, nullptr, nullptr);
    g_dbus_node_info_unref(info);
    g_bus_own_name_on_connection(g_bus, kInstanceName, G_BUS_NAME_OWNER_FLAGS_NONE, nullptr, nullptr, nullptr,
                                 nullptr);
}

void watchColorScheme() {
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!bus) return;  // kept for the process's lifetime with the subscription
    g_dbus_connection_signal_subscribe(bus, "org.freedesktop.portal.Desktop", "org.freedesktop.portal.Settings",
                                       "SettingChanged", "/org/freedesktop/portal/desktop", nullptr,
                                       G_DBUS_SIGNAL_FLAGS_NONE, colorSchemeChanged, nullptr, nullptr);
}

}  // namespace

namespace {

void tracePrint(const char* what, const GError* error) {
    if (g_getenv("TINTA_TRACE_GTK")) {
        std::fprintf(stderr, "tinta-gtk: print %s%s%s\n", what, error ? ": " : "", error ? error->message : "");
    }
}

// Outside a sandbox: GTK's own dialog and a print job over CUPS. KDE's print
// portal fails for an unsandboxed app ("Operation failed"), and GtkPrintDialog
// takes the portal whenever one runs.
void printWithUnixDialog(Native* n, const std::string& path) {
    GtkWidget* dialog = gtk_print_unix_dialog_new("Print", n ? n->window : nullptr);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog* d, int response, gpointer data) {
        std::unique_ptr<std::string> file(static_cast<std::string*>(data));
        auto* chooser = GTK_PRINT_UNIX_DIALOG(d);
        GtkPrinter* printer = response == GTK_RESPONSE_OK ? gtk_print_unix_dialog_get_selected_printer(chooser) : nullptr;
        if (printer && gtk_printer_accepts_pdf(printer)) {
            GtkPrintSettings* settings = gtk_print_unix_dialog_get_settings(chooser);
            GtkPrintJob* job = gtk_print_job_new("Tinta", printer, settings, gtk_print_unix_dialog_get_page_setup(chooser));
            g_object_unref(settings);
            GError* error = nullptr;
            if (gtk_print_job_set_source_file(job, file->c_str(), &error)) {
                gtk_print_job_send(
                    job,
                    +[](GtkPrintJob*, gpointer done, const GError* sendError) {
                        std::unique_ptr<std::string> sent(static_cast<std::string*>(done));
                        tracePrint(sendError ? "not sent" : "sent", sendError);
                        g_remove(sent->c_str());
                    },
                    file.release(), nullptr);
            } else {
                tracePrint("not sent", error);
                g_error_free(error);
            }
            g_object_unref(job);
        } else {
            tracePrint(printer ? "not sent: the printer takes no PDF" : "not sent: cancelled", nullptr);
        }
        if (file) g_remove(file->c_str());
        gtk_window_destroy(GTK_WINDOW(d));
    }), new std::string(path));
    gtk_window_present(GTK_WINDOW(dialog));
}

}  // namespace

bool printPdf(HWND owner, const std::string& path) {
    Native* n = nativeOf(owner);
    // In a sandbox (Flatpak): the print portal, the only way to a printer.
    // Set up first (PreparePrint shows the portal's dialog), then print with
    // that setup: given no setup, GTK calls the portal's Print directly,
    // which KDE's portal refuses at once ("Operation failed")
    if (g_file_test("/.flatpak-info", G_FILE_TEST_EXISTS)) {
#if GTK_CHECK_VERSION(4, 14, 0)
        struct Job {
            std::string path;
            GtkWindow* parent;
        };
        GtkPrintDialog* dialog = gtk_print_dialog_new();
        gtk_print_dialog_set_title(dialog, "Print");
        gtk_print_dialog_setup(
            dialog, n ? n->window : nullptr, nullptr,
            [](GObject* source, GAsyncResult* result, gpointer data) {
                auto* job = static_cast<Job*>(data);
                auto* d = GTK_PRINT_DIALOG(source);
                GError* error = nullptr;
                GtkPrintSetup* setup = gtk_print_dialog_setup_finish(d, result, &error);
                if (!setup) {
                    tracePrint("not sent", error);
                    if (error) g_error_free(error);
                    g_remove(job->path.c_str());
                    delete job;
                    g_object_unref(d);
                    return;
                }
                GFile* file = g_file_new_for_path(job->path.c_str());
                gtk_print_dialog_print_file(
                    d, job->parent, setup, file, nullptr,
                    [](GObject* src, GAsyncResult* res, gpointer data2) {
                        std::unique_ptr<Job> printed(static_cast<Job*>(data2));
                        GError* printError = nullptr;
                        bool ok = gtk_print_dialog_print_file_finish(GTK_PRINT_DIALOG(src), res, &printError);
                        tracePrint(ok ? "sent" : "not sent", printError);
                        if (printError) g_error_free(printError);
                        g_remove(printed->path.c_str());
                        g_object_unref(src);
                    },
                    job);
                g_object_unref(file);
                gtk_print_setup_unref(setup);
            },
            new Job{path, n ? n->window : nullptr});
        return true;
#endif
    }
    printWithUnixDialog(n, path);
    return true;
}

bool beginTabDrag(HWND hwnd, const std::string& path, const TintaTabDragCard& card, int hotX, int hotY,
                  UINT doneMessage) {
    Native* n = nativeOf(hwnd);
    if (!n || g_tabDrag.drag || g_modal) return false;
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(n->window));
    GdkSeat* seat = gdk_display_get_default_seat(gtk_widget_get_display(n->area));
    GdkDevice* pointer = seat ? gdk_seat_get_pointer(seat) : nullptr;
    if (!surface || !pointer) return false;
    static int drags = 0;
    std::string receipt = std::string(g_get_user_runtime_dir()) + "/tinta-tab-" + std::to_string(getpid()) + "-" +
                          std::to_string(++drags);
    g_remove(receipt.c_str());
    std::string payload = path + "\n" + receipt;
    GBytes* bytes = g_bytes_new(payload.data(), payload.size());
    GdkContentProvider* content = gdk_content_provider_new_for_bytes(kTabMime, bytes);
    g_bytes_unref(bytes);
    GdkDrag* drag = gdk_drag_begin(surface, pointer, content, GDK_ACTION_MOVE, 0, 0);
    g_object_unref(content);
    if (!drag) return false;
    // The press now belongs to the drag: no release comes back to us
    n->buttons = 0;
    g_tabDrag = OutgoingTabDrag{drag, n, doneMessage, card.stripHeight, false, false, false, receipt};
    GdkTexture* texture = tabCardTexture(card, surfaceScale(n));
    GtkWidget* picture = gtk_picture_new_for_paintable(GDK_PAINTABLE(texture));
    g_object_unref(texture);
    gtk_widget_set_size_request(picture, card.width, card.height);
    gtk_drag_icon_set_child(GTK_DRAG_ICON(gtk_drag_icon_get_for_drag(drag)), picture);
    gdk_drag_set_hotspot(drag, hotX, hotY);
    g_signal_connect(drag, "drop-performed", G_CALLBACK(tabDragPerformed), nullptr);
    g_signal_connect(drag, "dnd-finished", G_CALLBACK(tabDragDone), nullptr);
    g_signal_connect(drag, "cancel", G_CALLBACK(tabDragCancelled), nullptr);
    return true;
}

bool active() { return g_active; }

bool start() {
    g_set_prgname("tinta");  // Wayland app id / WM_CLASS: matches tinta.desktop
    g_active = gtk_init_check();
    // TINTA_TRACE_GTK=1: which display server (GdkWaylandDisplay or
    // GdkX11Display) and, per resize, the scale and size in pixels
    if (g_active && g_getenv("TINTA_TRACE_GTK")) {
        std::fprintf(stderr, "tinta-gtk: display %s (%s)\n", G_OBJECT_TYPE_NAME(gdk_display_get_default()),
                     gdk_display_get_name(gdk_display_get_default()));
    }
    if (g_active) {
        watchColorScheme();
        ownInstanceName();
    }
    return g_active;
}

bool remoteInstanceExists() {
    if (!g_bus) return false;
    GVariant* reply = g_dbus_connection_call_sync(
        g_bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetNameOwner",
        g_variant_new("(s)", kInstanceName), G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, nullptr);
    if (!reply) return false;  // nobody owns it yet
    const gchar* owner = nullptr;
    g_variant_get(reply, "(&s)", &owner);
    bool remote = g_strcmp0(owner, g_dbus_connection_get_unique_name(g_bus)) != 0;
    g_variant_unref(reply);
    return remote;
}

// The launcher's activation token lets the receiving window take focus
// (xdg-activation on Wayland, startup notification on X11)
LRESULT sendRemoteCopyData(const COPYDATASTRUCT& data) {
    if (!g_bus) return FALSE;
    const char* token = g_getenv("XDG_ACTIVATION_TOKEN");
    if (!token) token = g_getenv("DESKTOP_STARTUP_ID");
    GVariant* bytes = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, data.lpData, data.lpData ? data.cbData : 0, 1);
    GVariant* reply = g_dbus_connection_call_sync(
        g_bus, kInstanceName, kInstancePath, "org.tinta.Instance", "CopyData",
        g_variant_new("(t@ays)", static_cast<guint64>(data.dwData), bytes, token ? token : ""), G_VARIANT_TYPE("(b)"),
        G_DBUS_CALL_FLAGS_NO_AUTO_START, 15000, nullptr, nullptr);
    if (!reply) return FALSE;
    gboolean handled = FALSE;
    g_variant_get(reply, "(b)", &handled);
    g_variant_unref(reply);
    return handled;
}

void present(HWND hwnd) {
    Native* n = nativeOf(hwnd);
    if (!n) return;
    if (!g_startupId.empty()) gtk_window_set_startup_id(n->window, g_startupId.c_str());
    gtk_window_present(n->window);
}

void createWindow(HWND hwnd) {
    auto* n = new Native();
    n->hwnd = hwnd;
    n->scale = monitorScale();
    hwnd->native = n;
    n->window = GTK_WINDOW(gtk_window_new());
    g_object_ref_sink(n->window);
    gtk_window_set_decorated(n->window, FALSE);  // Tinta draws its own caption
    gtk_window_set_icon_name(n->window, "tinta");
    gtk_window_set_title(n->window, utf8(hwnd->title.c_str()).c_str());
    int width = static_cast<int>((hwnd->rect.right - hwnd->rect.left) / n->scale);
    int height = static_cast<int>((hwnd->rect.bottom - hwnd->rect.top) / n->scale);
    gtk_window_set_default_size(n->window, std::max(400, width), std::max(300, height));
    hwnd->rect = {0, 0, hwnd->rect.right - hwnd->rect.left, hwnd->rect.bottom - hwnd->rect.top};

    n->area = gtk_drawing_area_new();
    gtk_widget_set_focusable(n->area, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(n->area), draw, n, nullptr);
    g_signal_connect(n->area, "resize", G_CALLBACK(resized), n);
    gtk_window_set_child(n->window, n->area);

    GtkEventController* pointer = gtk_event_controller_legacy_new();
    g_signal_connect(pointer, "event", G_CALLBACK(pointerEvent), n);
    gtk_widget_add_controller(n->area, pointer);

    GtkEventController* motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "leave", G_CALLBACK(left), n);
    gtk_widget_add_controller(n->area, motion);

    GtkEventController* scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll, "scroll", G_CALLBACK(scrolled), n);
    gtk_widget_add_controller(n->area, scroll);

    if (hwnd->exStyle & WS_EX_ACCEPTFILES) {
        GtkDropTarget* drop = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
        g_signal_connect(drop, "drop", G_CALLBACK(dropped), n);
        gtk_widget_add_controller(n->area, GTK_EVENT_CONTROLLER(drop));
    }
    if (hwnd->className == L"Tinta") {
        GdkContentFormats* formats = gdk_content_formats_new((const char*[]){kTabMime}, 1);
        GtkDropTargetAsync* tabs = gtk_drop_target_async_new(formats, GDK_ACTION_MOVE);
        g_signal_connect(tabs, "accept", G_CALLBACK(tabDropAccept), n);
        g_signal_connect(tabs, "drag-enter", G_CALLBACK(tabDropMotion), n);
        g_signal_connect(tabs, "drag-motion", G_CALLBACK(tabDropMotion), n);
        g_signal_connect(tabs, "drop", G_CALLBACK(tabDropped), n);
        gtk_widget_add_controller(n->area, GTK_EVENT_CONTROLLER(tabs));
    }

    // Capture phase: ahead of GtkWindow's own keys (Tab moves focus)
    GtkEventController* keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    g_signal_connect(keys, "key-pressed", G_CALLBACK(keyPressed), n);
    g_signal_connect(keys, "key-released", G_CALLBACK(keyReleased), n);
    gtk_widget_add_controller(GTK_WIDGET(n->window), keys);

    g_signal_connect(n->window, "close-request", G_CALLBACK(closeRequested), n);
    g_signal_connect(n->window, "notify::is-active", G_CALLBACK(activeChanged), n);
    n->im = gtk_im_multicontext_new();
    gtk_im_context_set_client_widget(n->im, n->area);
    g_signal_connect(n->im, "commit", G_CALLBACK(imCommitted), n);
    g_signal_connect(n->im, "preedit-start", G_CALLBACK(imPreeditStarted), n);
    g_natives.push_back(n);
}

void showWindow(HWND hwnd, int cmd) {
    Native* n = nativeOf(hwnd);
    if (!n) return;
    switch (cmd) {
        case SW_HIDE: gtk_widget_set_visible(GTK_WIDGET(n->window), FALSE); return;
        case SW_MINIMIZE:
        case SW_SHOWMINIMIZED: gtk_window_minimize(n->window); return;
        case SW_SHOWMAXIMIZED: gtk_window_maximize(n->window); break;
        case SW_RESTORE: gtk_window_unmaximize(n->window); gtk_window_unminimize(n->window); break;
        default: break;
    }
    gtk_window_present(n->window);
    gtk_widget_grab_focus(n->area);
}

void destroyWindow(HWND hwnd) {
    Native* n = nativeOf(hwnd);
    if (!n) return;
    hwnd->native = nullptr;
    g_natives.erase(std::remove(g_natives.begin(), g_natives.end(), n), g_natives.end());
    if (n->im) {
        gtk_im_context_set_client_widget(n->im, nullptr);
        g_object_unref(n->im);
    }
    gtk_window_destroy(n->window);
    g_object_unref(n->window);
    delete n;
}

void invalidate(HWND hwnd) {
    if (g_inDraw) return;
    if (Native* n = nativeOf(hwnd)) gtk_widget_queue_draw(n->area);
}

void setTitle(HWND hwnd) {
    if (Native* n = nativeOf(hwnd)) gtk_window_set_title(n->window, utf8(hwnd->title.c_str()).c_str());
}

// Win32 cursor ids to CSS cursor names; the cursor is process-wide, as there
void setCursor(LPCWSTR id) {
    const char* name = "default";
    switch (reinterpret_cast<ULONG_PTR>(id)) {
        case 32513: name = "text"; break;
        case 32649: name = "pointer"; break;
        case 32644: name = "ew-resize"; break;
        case 32645: name = "ns-resize"; break;
        case 32642: name = "nwse-resize"; break;
        case 32643: name = "nesw-resize"; break;
        case 32646: name = "move"; break;
        case 32648: name = "not-allowed"; break;
        case 32514: name = "wait"; break;
        case 32650: name = "progress"; break;
        case 32515: name = "crosshair"; break;
        default: break;
    }
    for (Native* n : g_natives) gtk_widget_set_cursor_from_name(n->area, name);
}

bool isMaximized(HWND hwnd) {
    Native* n = nativeOf(hwnd);
    return n && gtk_window_is_maximized(n->window);
}

bool isMinimized(HWND hwnd) {
    Native* n = nativeOf(hwnd);
    if (!n) return false;
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(n->window));
    return surface && GDK_IS_TOPLEVEL(surface) &&
           (gdk_toplevel_get_state(GDK_TOPLEVEL(surface)) & GDK_TOPLEVEL_STATE_MINIMIZED);
}

double scale(HWND hwnd) {
    Native* n = nativeOf(hwnd);
    return n ? n->scale : 1.0;
}

void beginMove(HWND hwnd) {
    Native* n = nativeOf(hwnd);
    if (!n || !n->device) return;
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(n->window));
    if (surface && GDK_IS_TOPLEVEL(surface)) {
        gdk_toplevel_begin_move(GDK_TOPLEVEL(surface), n->device, n->button, n->pressX, n->pressY, n->pressTime);
    }
    n->nonClientPress = false;
}

void beginResize(HWND hwnd, int hit) {
    Native* n = nativeOf(hwnd);
    if (!n || !n->device) return;
    GdkSurfaceEdge edge;
    switch (hit) {
        case HTLEFT: edge = GDK_SURFACE_EDGE_WEST; break;
        case HTRIGHT: edge = GDK_SURFACE_EDGE_EAST; break;
        case HTTOP: edge = GDK_SURFACE_EDGE_NORTH; break;
        case HTBOTTOM: edge = GDK_SURFACE_EDGE_SOUTH; break;
        case HTTOPLEFT: edge = GDK_SURFACE_EDGE_NORTH_WEST; break;
        case HTTOPRIGHT: edge = GDK_SURFACE_EDGE_NORTH_EAST; break;
        case HTBOTTOMLEFT: edge = GDK_SURFACE_EDGE_SOUTH_WEST; break;
        default: edge = GDK_SURFACE_EDGE_SOUTH_EAST; break;
    }
    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(n->window));
    if (surface && GDK_IS_TOPLEVEL(surface)) {
        gdk_toplevel_begin_resize(GDK_TOPLEVEL(surface), edge, n->device, n->button, n->pressX, n->pressY,
                                  n->pressTime);
    }
    n->nonClientPress = false;
}

void minimize(HWND hwnd) {
    if (Native* n = nativeOf(hwnd)) gtk_window_minimize(n->window);
}

void toggleMaximize(HWND hwnd) {
    Native* n = nativeOf(hwnd);
    if (!n) return;
    if (gtk_window_is_maximized(n->window)) gtk_window_unmaximize(n->window);
    else gtk_window_maximize(n->window);
}

POINT cursorPos() { return g_pointer; }

// ImmSetCompositionWindow: the caret (client pixels) the input method's
// candidate window follows
void setImeCaret(HWND hwnd, POINT caret) {
    Native* n = nativeOf(hwnd);
    if (!n || !n->im) return;
    GdkRectangle area{static_cast<int>(caret.x / n->scale), static_cast<int>(caret.y / n->scale), 1,
                      static_cast<int>(20 / n->scale) + 1};
    gtk_im_context_set_cursor_location(n->im, &area);
}

void wake() { g_main_context_wakeup(nullptr); }

void waitForEvents(int timeoutMs) {
    guint id = 0;
    if (timeoutMs >= 0) {
        // The source only ends the wait; iteration returns once it dispatches
        id = g_timeout_add(static_cast<guint>(timeoutMs), [](gpointer) -> gboolean { return G_SOURCE_REMOVE; },
                           nullptr);
    }
    g_main_context_iteration(nullptr, TRUE);
    if (id) {
        if (GSource* source = g_main_context_find_source_by_id(nullptr, id)) g_source_destroy(source);
    }
}

bool charForKey(const MSG& msg, std::vector<wchar_t>& out) {
    if (msg.message != WM_KEYDOWN) return false;
    auto seq = static_cast<UINT32>(static_cast<ULONG_PTR>(msg.lParam) >> 32);
    auto it = g_keyChars.find(seq);
    if (it == g_keyChars.end()) return false;
    out.push_back(it->second);
    g_keyChars.erase(it);
    return true;
}

// ---- Clipboard -------------------------------------------------------------------------------
void publishClipboardText(const std::wstring& text) { gdk_clipboard_set_text(clipboard(), toUtf8(text).c_str()); }

// As a texture: GDK offers it as image/png and the other image types
void publishClipboardImage(const std::string& png) {
    GBytes* bytes = g_bytes_new(png.data(), png.size());
    GdkTexture* texture = gdk_texture_new_from_bytes(bytes, nullptr);
    g_bytes_unref(bytes);
    if (!texture) return;
    gdk_clipboard_set_texture(clipboard(), texture);
    g_object_unref(texture);
}

bool clipboardIsLocal() { return gdk_clipboard_is_local(clipboard()); }

bool clipboardHasText() {
    GdkContentFormats* formats = gdk_clipboard_get_formats(clipboard());
    return formats && (gdk_content_formats_contain_gtype(formats, G_TYPE_STRING) ||
                       gdk_content_formats_contain_mime_type(formats, "text/plain;charset=utf-8") ||
                       gdk_content_formats_contain_mime_type(formats, "text/plain"));
}

bool clipboardHasImage() {
    GdkContentFormats* formats = gdk_clipboard_get_formats(clipboard());
    return formats && (gdk_content_formats_contain_gtype(formats, GDK_TYPE_TEXTURE) ||
                       gdk_content_formats_contain_mime_type(formats, "image/png"));
}

bool readClipboardImagePng(std::string& png) {
    struct Read {
        bool done = false;
        GdkTexture* texture = nullptr;
    } read;
    gdk_clipboard_read_texture_async(
        clipboard(), nullptr,
        [](GObject* source, GAsyncResult* result, gpointer data) {
            auto* r = static_cast<Read*>(data);
            r->texture = gdk_clipboard_read_texture_finish(GDK_CLIPBOARD(source), result, nullptr);
            r->done = true;
        },
        &read);
    runUntil(read.done);
    if (!read.texture) return false;
    GBytes* bytes = gdk_texture_save_to_png_bytes(read.texture);
    g_object_unref(read.texture);
    if (!bytes) return false;
    gsize size = 0;
    const void* data = g_bytes_get_data(bytes, &size);
    png.assign(static_cast<const char*>(data), size);
    g_bytes_unref(bytes);
    return !png.empty();
}

bool readClipboardText(std::wstring& text) {
    struct Read {
        bool done = false;
        char* text = nullptr;
    } read;
    gdk_clipboard_read_text_async(
        clipboard(), nullptr,
        [](GObject* source, GAsyncResult* result, gpointer data) {
            auto* r = static_cast<Read*>(data);
            r->text = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(source), result, nullptr);
            r->done = true;
        },
        &read);
    runUntil(read.done);
    if (!read.text) return false;
    text = toWide(std::string(read.text));
    g_free(read.text);
    return true;
}

// ---- Dialogs -----------------------------------------------------------------------------------
bool fileDialog(OPENFILENAMEW* ofn, bool save) {
    if (!ofn || !ofn->lpstrFile || ofn->nMaxFile == 0) return false;
    GtkFileDialog* dialog = gtk_file_dialog_new();
    if (ofn->lpstrTitle) gtk_file_dialog_set_title(dialog, utf8(ofn->lpstrTitle).c_str());
    // "Name\0*.md;*.markdown\0Name\0*.*\0\0"
    if (const wchar_t* f = ofn->lpstrFilter) {
        GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
        while (*f) {
            std::wstring name = f;
            f += name.size() + 1;
            if (!*f) break;
            std::wstring patterns = f;
            f += patterns.size() + 1;
            GtkFileFilter* filter = gtk_file_filter_new();
            gtk_file_filter_set_name(filter, utf8(name.c_str()).c_str());
            size_t start = 0;
            while (start <= patterns.size()) {
                size_t end = patterns.find(L';', start);
                std::wstring pattern =
                    patterns.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
                if (pattern == L"*.*" || pattern == L"*") gtk_file_filter_add_pattern(filter, "*");
                else if (pattern.rfind(L"*.", 0) == 0) gtk_file_filter_add_suffix(filter, utf8(pattern.c_str() + 2).c_str());
                if (end == std::wstring::npos) break;
                start = end + 1;
            }
            g_list_store_append(filters, filter);
            g_object_unref(filter);
        }
        gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
        g_object_unref(filters);
    }
    if (ofn->lpstrInitialDir && *ofn->lpstrInitialDir) {
        GFile* folder = g_file_new_for_path(utf8(ofn->lpstrInitialDir).c_str());
        gtk_file_dialog_set_initial_folder(dialog, folder);
        g_object_unref(folder);
    }
    std::string current = utf8(ofn->lpstrFile);
    if (save && !current.empty()) {
        gtk_file_dialog_set_initial_name(dialog, current.substr(current.find_last_of('/') + 1).c_str());
    }
    struct Pick {
        bool done = false;
        bool save = false;
        GFile* file = nullptr;
    } pick;
    pick.save = save;
    auto finished = [](GObject* source, GAsyncResult* result, gpointer data) {
        auto* p = static_cast<Pick*>(data);
        p->file = p->save ? gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, nullptr)
                          : gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, nullptr);
        p->done = true;
    };
    if (save) gtk_file_dialog_save(dialog, parentWindow(ofn->hwndOwner), nullptr, finished, &pick);
    else gtk_file_dialog_open(dialog, parentWindow(ofn->hwndOwner), nullptr, finished, &pick);
    runUntil(pick.done);
    g_object_unref(dialog);
    if (!pick.file) return false;
    char* path = g_file_get_path(pick.file);
    g_object_unref(pick.file);
    if (!path) return false;
    std::wstring chosen = toWide(std::string(path));
    g_free(path);
    // Save dialogs add the default extension when the name has none
    size_t slash = chosen.find_last_of(L'/');
    size_t nameStart = slash == std::wstring::npos ? 0 : slash + 1;
    if (save && ofn->lpstrDefExt && *ofn->lpstrDefExt && chosen.find(L'.', nameStart) == std::wstring::npos) {
        chosen += L'.';
        chosen += ofn->lpstrDefExt;
    }
    if (chosen.size() + 1 > ofn->nMaxFile) return false;
    wmemcpy(ofn->lpstrFile, chosen.c_str(), chosen.size() + 1);
    size_t dot = chosen.find_last_of(L'.');
    ofn->nFileOffset = static_cast<WORD>(nameStart);
    ofn->nFileExtension = static_cast<WORD>(dot == std::wstring::npos || dot < nameStart ? 0 : dot + 1);
    return true;
}

int messageBox(HWND owner, LPCWSTR text, LPCWSTR caption, UINT type) {
    std::vector<const char*> labels;
    std::vector<int> results;
    switch (type & 0xF) {
        case MB_OKCANCEL: labels = {"OK", "Cancel"}; results = {IDOK, IDCANCEL}; break;
        case MB_YESNOCANCEL: labels = {"Yes", "No", "Cancel"}; results = {IDYES, IDNO, IDCANCEL}; break;
        case MB_YESNO: labels = {"Yes", "No"}; results = {IDYES, IDNO}; break;
        default: labels = {"OK"}; results = {IDOK}; break;
    }
    labels.push_back(nullptr);
    GtkAlertDialog* dialog = gtk_alert_dialog_new("%s", utf8(caption ? caption : L"Tinta").c_str());
    gtk_alert_dialog_set_detail(dialog, utf8(text).c_str());
    gtk_alert_dialog_set_buttons(dialog, labels.data());
    gtk_alert_dialog_set_default_button(dialog, 0);
    gtk_alert_dialog_set_cancel_button(dialog, static_cast<int>(results.size()) - 1);
    struct Choice {
        bool done = false;
        int index = -1;
    } choice;
    gtk_alert_dialog_choose(
        dialog, parentWindow(owner), nullptr,
        [](GObject* source, GAsyncResult* result, gpointer data) {
            auto* c = static_cast<Choice*>(data);
            c->index = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), result, nullptr);
            c->done = true;
        },
        &choice);
    runUntil(choice.done);
    g_object_unref(dialog);
    if (choice.index < 0 || choice.index >= static_cast<int>(results.size())) return results.back();
    return results[static_cast<size_t>(choice.index)];
}

}  // namespace gtk
}  // namespace tinta
