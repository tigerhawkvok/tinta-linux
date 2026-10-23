// PlantUML inside the app (PR #248 review): fit-toggle ordinals while a
// render is pending, the print preview picking up finished renders, Word
// export of dark-theme diagrams, the settings row and choosing the tool.
// Drives the real layout, preview, export and settings code with the fake
// PlantUML tool; CTest stages the binary beside its own settings.ini.
#include "d2d_init.h"
#include "document.h"
#include "export.h"
#include "overlays.h"
#include "plantuml.h"
#include "plantuml_app.h"
#include "plantuml_queue.h"
#include "print.h"
#include "render.h"
#include "utils.h"

#include <wincodec.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
std::string read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
}

struct ScopedEnv {
    std::wstring name;
    ScopedEnv(const wchar_t* key, const std::wstring& value) : name(key) {
        SetEnvironmentVariableW(key, value.c_str());
    }
    ~ScopedEnv() { SetEnvironmentVariableW(name.c_str(), nullptr); }
};

const std::wstring kFakeTool = toWide(TINTA_FAKE_PLANTUML);

void layout(App& app, const std::string& markdown) {
    auto doc = parseDocument(app.parser, markdown, "plantuml-app.md");
    check(doc.success, "the document parses");
    app.root = doc.root;
    layoutDocument(app);
    ensureLayoutComplete(app);
}

const App::CodeBlockInfo* findBlock(const App& app, bool plantumlBlock) {
    for (const auto& block : app.codeBlocks) {
        if (block.isDiagram && block.isPlantuml == plantumlBlock) return &block;
    }
    return nullptr;
}

// A diagram still rendering takes its fit ordinal, so the diagrams after
// it keep their fit state when it lands
void fitOrdinals(App& app) {
    layout(app, "```plantuml\n@startuml\nOrdinal -> Pending\n@enduml\n```\n\n"
                "```mermaid\nflowchart LR\n  A --> B\n```\n");
    const App::CodeBlockInfo* mermaidBefore = findBlock(app, false);
    check(mermaidBefore != nullptr && findBlock(app, true) == nullptr,
          "the PlantUML diagram is still rendering on the first layout");
    const unsigned before = mermaidBefore ? mermaidBefore->fitKey : 0;
    if (app.plantumlQueue) app.plantumlQueue->waitForIdle(10000);
    layout(app, "```plantuml\n@startuml\nOrdinal -> Pending\n@enduml\n```\n\n"
                "```mermaid\nflowchart LR\n  A --> B\n```\n");
    const App::CodeBlockInfo* plantumlAfter = findBlock(app, true);
    const App::CodeBlockInfo* mermaidAfter = findBlock(app, false);
    check(plantumlAfter != nullptr, "the PlantUML diagram lands");
    check(mermaidAfter != nullptr && mermaidAfter->fitKey == before,
          "the next diagram keeps its fit key when an earlier one lands");
    check(plantumlAfter && mermaidAfter && plantumlAfter->fitKey != mermaidAfter->fitKey,
          "each diagram has its own fit key");
}

// The preview lays out at print width in the print palette, so its render
// differs from the screen's; when it lands the open preview shows it
void previewPicksUpRender(App& app) {
    applyTheme(app, 5);  // Midnight: the print palette's key differs from the screen's
    updateTextFormats(app);
    auto doc = parseDocument(app.parser,
                             "Before the diagram.\n\n```plantuml\n@startuml\nPreview -> Lands\n"
                             "@enduml\n```\n\nAfter the diagram.\n",
                             "preview.md");
    app.root = doc.root;
    layoutDocument(app);
    openPrintPreview(app, app.hwnd);
    check(app.showPrintPreview, "the print preview opens");
    check(findBlock(app, true) == nullptr, "the preview's render is still pending at first");
    if (app.plantumlQueue) app.plantumlQueue->waitForIdle(10000);
    printPreviewRefresh(app);
    check(findBlock(app, true) != nullptr,
          "the open preview shows the diagram once its render lands");
    closePrintPreview(app, app.hwnd);
    applyTheme(app, 0);
    updateTextFormats(app);
}

// The embedded PNG's first pixel, decoded
bool firstPixel(App& app, const std::string& png, BYTE bgra[4]) {
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    bool ok = SUCCEEDED(app.wicFactory->CreateStream(&stream)) &&
              SUCCEEDED(stream->InitializeFromMemory(
                  reinterpret_cast<BYTE*>(const_cast<char*>(png.data())),
                  static_cast<DWORD>(png.size()))) &&
              SUCCEEDED(app.wicFactory->CreateDecoderFromStream(
                  stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) &&
              SUCCEEDED(decoder->GetFrame(0, &frame)) &&
              SUCCEEDED(app.wicFactory->CreateFormatConverter(&converter)) &&
              SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                                              WICBitmapDitherTypeNone, nullptr, 0.0,
                                              WICBitmapPaletteTypeCustom));
    if (ok) {
        WICRect one{0, 0, 1, 1};
        ok = SUCCEEDED(converter->CopyPixels(&one, 4, 4, bgra));
    }
    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    return ok;
}

// A dark theme's diagram goes into Word on the theme's own background, as
// the mermaid diagrams do, not as light labels on a transparent page
void docxDarkDiagram(App& app, const std::filesystem::path& dir) {
    const ScopedEnv fixture(L"TINTA_FAKE_PLANTUML_FIXTURE_DIR",
                            toWide(TINTA_TRANSPARENT_FIXTURE_DIR));
    applyTheme(app, 5);
    updateTextFormats(app);
    auto doc = parseDocument(app.parser,
                             "```plantuml\n@startuml\nDark -> Word\n@enduml\n```\n", "dark.md");
    app.root = doc.root;
    layoutDocument(app);
    const std::filesystem::path out = dir / L"dark-diagram.docx";
    check(exportDocxFile(app, out.wstring()), "the Word export succeeds");
    const std::string zip = read(out);
    const std::string signature = "\x89PNG\r\n\x1a\n";
    const size_t media = zip.find("word/media/");
    const size_t start = media == std::string::npos ? std::string::npos : zip.find(signature, media);
    const size_t iend = start == std::string::npos ? std::string::npos : zip.find("IEND", start);
    check(iend != std::string::npos, "the diagram is embedded as a PNG");
    if (iend != std::string::npos) {
        BYTE bgra[4] = {};
        check(firstPixel(app, zip.substr(start, iend + 8 - start), bgra),
              "the embedded PNG decodes");
        const D2D1_COLOR_F bg = app.theme.background;
        const auto matches = [](BYTE value, float channel) {
            return std::abs(static_cast<int>(value) - static_cast<int>(channel * 255.0f + 0.5f)) <= 2;
        };
        check(bgra[3] == 255 && matches(bgra[2], bg.r) && matches(bgra[1], bg.g) && matches(bgra[0], bg.b),
              "the diagram sits on the theme's opaque background");
    }
    applyTheme(app, 0);
    updateTextFormats(app);
}

// Seven rows in Editor since PlantUML joined: the last stays above the footer
void settingsRow(App& app) {
    app.showSettings = true;
    app.settingsSection = 2;
    app.settingsAnimation = 1.0f;
    app.renderTarget->BeginDraw();
    renderSettingsOverlay(app);
    check(SUCCEEDED(app.renderTarget->EndDraw()), "the Editor settings draw");
    const D2D1_RECT_F panel = settingsPanelRect(app);
    bool found = false;
    for (const auto& [rect, id] : app.settingsHits) {
        if (id != SET_LOCATE_PLANTUML) continue;
        found = true;
        // The chip sits mid-card: the card ends 17 DIPs below it, the
        // footer text starts 32 DIPs above the panel's bottom
        check(rect.bottom + dpi(app, 17.0f) <= panel.bottom - dpi(app, 32.0f) + 0.5f,
              "the PlantUML row stays clear of the settings footer");
    }
    check(found, "the PlantUML row offers Browse");
    app.showSettings = false;
}

void chooseTool(App& app) {
    app.layoutDirty = false;
    plantumlSetUserPath(app, kFakeTool);
    check(app.plantumlTool.available, "the chosen tool resolves");
    check(app.layoutDirty, "choosing the tool lays the open document out again");
}
}  // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    if (argc != 2 || std::string(argv[1]) != "--portable-test") {
        std::cerr << "Run this test through CTest's isolated portable wrapper\n";
        return 2;
    }
    auto state = std::make_unique<App>();
    App& app = *state;
    app.hwnd = CreateWindowExW(0, L"STATIC", L"PlantUML app tests", WS_POPUP, 0, 0, 1180, 900,
                               nullptr, nullptr, nullptr, nullptr);
    if (!app.hwnd || !initD2D(app) || !createRenderTarget(app)) return 2;
    app.width = 1180;
    app.height = 900;
    app.readingWidthPct = 100;
    applyTheme(app, 0);
    updateTextFormats(app);
    app.plantumlUserPath = kFakeTool;
    app.plantumlChecked = false;
    check(plantumlAvailable(app), "the fake PlantUML tool resolves");

    fitOrdinals(app);
    previewPicksUpRender(app);
    docxDarkDiagram(app, fs::current_path());
    settingsRow(app);
    chooseTool(app);

    if (app.plantumlQueue) app.plantumlQueue->shutdown();
    DestroyWindow(app.hwnd);
    app.hwnd = nullptr;
    state.reset();
    CoUninitialize();
    std::cout << "PlantUML in the app: " << failures << " failures\n";
    return failures ? 1 : 0;
}
