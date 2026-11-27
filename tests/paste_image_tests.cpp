// Pasting an image into the editor (editor.cpp): the choice card, the
// assets folder, inline, off, the remembered choice and its setting, and
// the reader showing each link form in tests/fixtures/paste-image.md
#include "d2d_init.h"
#include "editor.h"
#include "export.h"
#include "image_loader.h"
#include "input.h"
#include "overlays.h"
#include "render.h"
#include "settings.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace {

int failures = 0;

void check(bool ok, const std::string& message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << "\n";
        failures++;
    }
}

std::string read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

std::string narrow(const std::wstring& text) {
    std::string out;
    for (wchar_t c : text) out += c < 128 ? (char)c : '?';
    return out;
}

// A 4 x 3 RGB PNG, and its base64
const unsigned char kPng[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x08, 0x02, 0x00, 0x00, 0x00, 0x3b, 0x96, 0x39, 0x91, 0x00, 0x00, 0x00,
    0x1f, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0xb8, 0x60, 0x60, 0x60, 0xb0, 0xc0, 0xc1, 0x20, 0xe0, 0xc2,
    0xff, 0xff, 0xff, 0x19, 0xe0, 0x2c, 0xa0, 0x28, 0x03, 0x9c, 0x05, 0x14, 0x05, 0x00, 0x77, 0x84, 0x13, 0xa8,
    0x2a, 0xbb, 0xb1, 0x9e, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
const std::string kPngBytes(reinterpret_cast<const char*>(kPng), sizeof kPng);
const std::wstring kPngBase64 =
    L"iVBORw0KGgoAAAANSUhEUgAAAAQAAAADCAIAAAA7ljmRAAAAH0lEQVR42mO4YGBgsMDBIODC////GeAsoCgDnAUUBQB3hBOoKruxngAAAABJRU5ErkJggg==";

// The clipboard as a screenshot tool leaves it: the "PNG" format, and
// optionally text beside it
void setClipboard(HWND hwnd, const std::string& png, const std::wstring& text = L"") {
    if (!OpenClipboard(hwnd)) return;
    EmptyClipboard();
    if (!png.empty()) {
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, png.size());
        std::memcpy(GlobalLock(memory), png.data(), png.size());
        GlobalUnlock(memory);
        SetClipboardData(RegisterClipboardFormatW(L"PNG"), memory);
    }
    if (!text.empty()) {
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
        std::memcpy(GlobalLock(memory), text.c_str(), (text.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(memory);
        SetClipboardData(CF_UNICODETEXT, memory);
    }
    CloseClipboard();
}

void withKeys(bool ctrl, const std::function<void()>& action) {
    BYTE original[256], pressed[256];
    GetKeyboardState(original);
    std::memcpy(pressed, original, sizeof pressed);
    pressed[VK_CONTROL] = ctrl ? 0x80 : 0;
    pressed[VK_SHIFT] = 0;
    pressed[VK_MENU] = 0;
    SetKeyboardState(pressed);
    action();
    SetKeyboardState(original);
}

void key(App& app, unsigned vk, bool ctrl = false) {
    withKeys(ctrl, [&] { handleKeyDown(app, app.hwnd, vk); });
}

void paste(App& app) { key(app, 'V', true); }

void click(App& app, float x, float y) {
    LPARAM point = MAKELPARAM((int)x, (int)y);
    handleMouseDown(app, app.hwnd, MK_LBUTTON, point);
    handleMouseUp(app, app.hwnd, 0, point);
}

void paint(App& app, const std::function<void()>& draw) {
    app.renderTarget->BeginDraw();
    draw();
    check(SUCCEEDED(app.renderTarget->EndDraw()), "the frame renders");
}

// The paste card's hits by choice, after painting it
std::set<int> cardChoices(App& app) {
    paint(app, [&] { renderPasteImagePrompt(app); });
    std::set<int> choices;
    for (const auto& hit : app.pasteImageHits) {
        choices.insert(hit.second);
        check(hit.first.left >= 0 && hit.first.top >= 0 && hit.first.right <= app.width &&
                  hit.first.bottom <= app.height,
              "a card hit stays inside the window at width " + std::to_string(app.width));
        check(hit.first.left >= app.promptChipRect.left - 0.5f && hit.first.right <= app.promptChipRect.right + 0.5f,
              "a card hit stays inside the card at width " + std::to_string(app.width));
    }
    return choices;
}

D2D1_RECT_F hitFor(const App& app, int choice) {
    for (const auto& hit : app.pasteImageHits) {
        if (hit.second == choice) return hit.first;
    }
    return D2D1::RectF(-1, -1, -1, -1);
}

void clickHit(App& app, const D2D1_RECT_F& r) {
    click(app, (r.left + r.right) * 0.5f, (r.top + r.bottom) * 0.5f);
}

void setText(App& app, const std::wstring& text) {
    app.editorText = text;
    rebuildLineStarts(app);
    app.undoStack.clear();
    app.redoStack.clear();
    app.editorCursorPos = app.editorSelStart = app.editorSelEnd = text.size();
    app.editorHasSelection = false;
}

bool endsWith(const std::wstring& text, const std::wstring& tail) {
    return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
}

void collectImages(const ElementPtr& element, std::vector<std::string>& urls) {
    if (!element) return;
    if (element->type == ElementType::Image) urls.push_back(element->url);
    for (const auto& child : element->children) collectImages(child, urls);
}

// Every image in `markdown`, laid out as the document at `file`: its url,
// and whether the reader loaded it
std::vector<std::pair<std::string, bool>> loadedImages(App& app, const std::string& file,
                                                       const std::string& markdown) {
    app.currentFile = file;
    app.root = app.parser.parse(markdown).root;
    app.imageCache.clear();
    layoutDocument(app);
    std::vector<std::string> urls;
    collectImages(app.root, urls);
    std::vector<std::pair<std::string, bool>> out;
    for (const auto& url : urls) {
        auto it = app.imageCache.find(url);
        out.push_back({url, it != app.imageCache.end() && !it->second.failed && it->second.bitmap});
    }
    return out;
}

void dataUris() {
    std::string bytes;
    check(imageDataUriBytes("data:image/png;base64," + narrow(kPngBase64), bytes) && bytes == kPngBytes,
          "a base64 data URI decodes to its bytes");
    check(!imageDataUriBytes("data:text/plain;base64,aGk=", bytes), "only images decode");
    check(!imageDataUriBytes("data:image/png,%89PNG", bytes), "only base64 decodes");
    check(!imageDataUriBytes("data:image/png;base64,iVBOR*w0", bytes) && bytes.empty(),
          "a stray character fails cleanly");
    check(!imageDataUriBytes("paste-image.assets/a.png", bytes), "a path is no data URI");
}

}  // namespace

int main() {
    // Settings and pasted files go to a scratch folder, never the user's
    fs::path scratch = fs::temp_directory_path() / ("tinta-paste-tests-" + std::to_string(GetCurrentProcessId()));
    fs::remove_all(scratch);
    fs::create_directories(scratch / "config");
    setenv("XDG_CONFIG_HOME", (scratch / "config").string().c_str(), 1);
    check(tintaConfigDir().find(L"tinta-paste-tests-") != std::wstring::npos,
          "the checker: settings land in the scratch folder");

    auto state = std::make_unique<App>();
    App& app = *state;
    app.hwnd = CreateWindowExW(0, L"STATIC", L"Paste image tests", WS_POPUP, 0, 0, 900, 600, nullptr, nullptr,
                               nullptr, nullptr);
    if (!app.hwnd || !initD2D(app) || !createRenderTarget(app)) return 2;
    app.width = 900;
    app.height = 600;
    app.contentScale = 1.0f;
    updateTextFormats(app);
    dataUris();

    // The reader shows all three link forms the fixture holds
    {
        const std::string fixture = read(TINTA_PASTE_FIXTURE);
        auto images = loadedImages(app, TINTA_PASTE_FIXTURE, fixture);
        check(images.size() == 3, "the fixture holds three images, got " + std::to_string(images.size()));
        for (const auto& image : images) {
            check(image.second, "the reader loads " + image.first.substr(0, 40));
        }
        check(images.size() == 3 && images[2].first == "paste-image.assets/screen shot (2).png",
              "an angle-bracket destination keeps its spaces and parentheses");

        // The exports keep all three as pictures: HTML inlines them, and
        // Word embeds the inline one's bytes, not its address
        fs::path html = scratch / "fixture.html", docx = scratch / "fixture.docx";
        check(exportHtmlFile(app, html.wstring()), "the HTML export succeeds");
        const std::string page = read(html);
        int inlined = 0;
        for (size_t at = page.find("<img src=\"data:image/png;base64,"); at != std::string::npos;
             at = page.find("<img src=\"data:image/png;base64,", at + 1)) {
            inlined++;
        }
        check(inlined == 3, "the HTML export inlines all three images, got " + std::to_string(inlined));
        check(exportDocxFile(app, docx.wstring()), "the Word export succeeds");
        const std::string word = read(docx);
        int drawings = 0;
        for (size_t at = word.find("<w:drawing>"); at != std::string::npos; at = word.find("<w:drawing>", at + 1)) {
            drawings++;
        }
        std::string inlineBytes;
        check(images.size() == 3 && imageDataUriBytes(images[1].first, inlineBytes) &&
                  word.find(inlineBytes) != std::string::npos,
              "the Word export embeds the inline image's PNG");
        check(drawings == 3, "the Word export draws all three images, got " + std::to_string(drawings));
        // (the fixture's prose and code say "base64"; its payload is the test)
        check(images.size() == 3 && word.find(images[1].first.substr(22, 48)) == std::string::npos,
              "the Word export carries no data: address");
    }

    // A saved document whose name holds a space
    fs::path doc = scratch / "paste notes.md";
    { std::ofstream(doc, std::ios::binary) << "# Notes\n\n"; }
    app.currentFile = doc.string();
    enterEditMode(app);
    check(app.editMode, "the editor opens");
    setText(app, L"# Notes\n\n");
    fs::path assets = scratch / "paste notes.assets";
    const std::wstring firstLink = L"![image](<paste notes.assets/paste notes-img-01.png>)";

    // Ask: the card, and nothing typed yet
    check(app.pasteImageMode == "ask", "pasting asks by default");
    setClipboard(app.hwnd, kPngBytes);
    paste(app);
    check(app.pasteImagePending && app.editorText == L"# Notes\n\n", "an image paste opens the card");
    for (int width : {900, 420}) {
        app.width = width;
        std::set<int> choices = cardChoices(app);
        check(choices == std::set<int>({kPasteCancel, kPasteAssets, kPasteInline, kPasteSaveAs, 9}),
              "the card offers the assets folder, inline, save as, cancel and remember at width " +
                  std::to_string(width));
    }
    app.width = 900;
    cardChoices(app);

    // A: the assets folder, linked relative to the document
    key(app, 'A');
    check(!app.pasteImagePending, "A closes the card");
    check(read(assets / "paste notes-img-01.png") == kPngBytes, "A saves the PNG to the assets folder");
    check(endsWith(app.editorText, firstLink), "A links it relative: " + narrow(app.editorText));
    check(app.swallowCharsUntil > std::chrono::steady_clock::now(), "the key that chose types nothing");
    {
        std::wstring text = app.editorText;
        auto images = loadedImages(app, doc.string(), narrow(text));
        check(images.size() == 1 && images[0].first == "paste notes.assets/paste notes-img-01.png" &&
                  images[0].second,
              "the reader shows the pasted image");
        app.currentFile = doc.string();
    }
    key(app, 'Z', true);
    check(app.editorText == L"# Notes\n\n", "Ctrl+Z takes the link back");

    // Esc, and a click outside the card, cancel
    paste(app);
    key(app, VK_ESCAPE);
    check(!app.pasteImagePending && app.editorText == L"# Notes\n\n", "Esc cancels");
    paste(app);
    cardChoices(app);
    click(app, app.promptChipRect.left + 4, app.promptChipRect.bottom - 4);
    check(app.pasteImagePending, "a click on the card's blank space keeps it");
    click(app, 20, 20);
    check(!app.pasteImagePending && app.editorText == L"# Notes\n\n", "a click outside the card cancels");
    // ...and the rest of that click does nothing (the tab strip and the
    // menu button sit up there)
    check(!app.showContextMenu && !app.appMenuPressed, "the cancelling click opens nothing");

    // Remember (clicked), then the assets button (clicked): the setting
    // becomes "assets", and the next paste goes there without asking
    paste(app);
    cardChoices(app);
    clickHit(app, hitFor(app, 9));
    check(app.pasteImagePending && app.pasteImageRemember, "clicking Remember ticks it and keeps the card");
    cardChoices(app);
    clickHit(app, hitFor(app, kPasteAssets));
    check(fs::exists(assets / "paste notes-img-02.png"), "the next name is img-02");
    check(app.pasteImageMode == "assets" && loadSettings().pasteImage == "assets",
          "Remember makes the assets folder the setting");
    setText(app, L"");
    paste(app);
    check(!app.pasteImagePending && fs::exists(assets / "paste notes-img-03.png") &&
              app.editorText == L"![image](<paste notes.assets/paste notes-img-03.png>)",
          "a remembered choice pastes without the card");

    // Settings > Editor > Pasted images: four segments in the panel
    auto settingsClick = [&](int action, const std::string& what) {
        app.showSettings = true;
        app.settingsSection = 2;
        app.settingsAnimation = 1.0f;
        paint(app, [&] { renderSettingsOverlay(app); });
        D2D1_RECT_F panel = settingsPanelRect(app);
        bool found = false;
        for (const auto& hit : app.settingsHits) {
            if (hit.second < SET_PASTE_ASK || hit.second > SET_PASTE_OFF) continue;
            check(hit.first.left >= panel.left && hit.first.right <= panel.right && hit.first.top >= panel.top &&
                      hit.first.bottom <= panel.bottom,
                  "a pasted-images segment sits inside the settings panel at width " + std::to_string(app.width));
            if (hit.second == action) {
                found = true;
                click(app, (hit.first.left + hit.first.right) * 0.5f, (hit.first.top + hit.first.bottom) * 0.5f);
            }
        }
        check(found, "settings offers " + what);
        app.showSettings = false;
        app.settingsAnimation = 0.0f;
    };
    app.width = 640;
    settingsClick(SET_PASTE_ASK, "ask");
    app.width = 900;
    check(app.pasteImageMode == "ask" && loadSettings().pasteImage == "ask", "the Ask segment sets ask");
    settingsClick(SET_PASTE_INLINE, "inline");
    check(app.pasteImageMode == "inline" && loadSettings().pasteImage == "inline", "the Inline segment sets inline");

    // Inline: the bytes in the link, and the reader shows them
    setText(app, L"");
    paste(app);
    check(app.editorText == L"![image](data:image/png;base64," + kPngBase64 + L")",
          "inline embeds the PNG: " + narrow(app.editorText.substr(0, 60)));
    {
        auto images = loadedImages(app, doc.string(), narrow(app.editorText));
        check(images.size() == 1 && images[0].second, "the reader shows an inline image");
        app.currentFile = doc.string();
    }

    // Off: nothing
    settingsClick(SET_PASTE_OFF, "off");
    check(loadSettings().pasteImage == "off", "the Off segment persists");
    setText(app, L"x");
    paste(app);
    check(!app.pasteImagePending && app.editorText == L"x", "off pastes no image");

    // Text on the clipboard wins over the image beside it
    settingsClick(SET_PASTE_ASK, "ask");
    setClipboard(app.hwnd, kPngBytes, L"cell text");
    paste(app);
    check(!app.pasteImagePending && app.editorText == L"xcell text", "text beside an image pastes as text");
    setClipboard(app.hwnd, kPngBytes);

    // An untitled note has no assets folder: the card leaves it out, A does
    // nothing, and Enter takes Inline; a remembered "assets" still asks
    app.currentFile.clear();
    app.pasteImageMode = "assets";
    setText(app, L"");
    paste(app);
    check(app.pasteImagePending, "an untitled note asks even when the setting is the assets folder");
    std::set<int> choices = cardChoices(app);
    check(!choices.count(kPasteAssets) && choices.count(kPasteInline) && choices.count(kPasteSaveAs),
          "an untitled note's card offers inline and save as only");
    key(app, 'A');
    check(app.pasteImagePending && app.editorText.empty(), "A does nothing without a folder");
    key(app, VK_RETURN);
    check(!app.pasteImagePending && endsWith(app.editorText, kPngBase64 + L")"), "Enter takes Inline there");

    // The clipboard emptied while the card was up
    app.currentFile = doc.string();
    app.pasteImageMode = "ask";
    setText(app, L"");
    paste(app);
    setClipboard(app.hwnd, "");
    key(app, 'I');
    check(!app.pasteImagePending && app.editorText.empty(), "an image gone from the clipboard pastes nothing");

    DestroyWindow(app.hwnd);
    fs::remove_all(scratch);
    std::cout << "paste image: " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
