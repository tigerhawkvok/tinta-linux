// The default code face: built-in themes use kDefaultCodeFont, and every code
// format lands on an installed monospace face (kDefaultCodeFont where it is
// installed, which the Linux build guarantees by shipping it; else Consolas).
#include "d2d_init.h"
#include "render.h"

#include <iostream>
#include <memory>
#include <string>

namespace {
int failures = 0;
void check(bool value, const std::string& message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool installed(App& app, const wchar_t* family) {
    IDWriteFontCollection* fonts = nullptr;
    app.dwriteFactory->GetSystemFontCollection(&fonts, FALSE);
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (fonts) {
        fonts->FindFamilyName(family, &index, &exists);
        fonts->Release();
    }
    return exists != FALSE;
}

std::wstring familyOf(IDWriteTextFormat* format) {
    std::wstring name(format->GetFontFamilyNameLength() + 1, L'\0');
    format->GetFontFamilyName(name.data(), static_cast<UINT32>(name.size()));
    name.resize(name.size() - 1);
    return name;
}

float widthOf(App& app, IDWriteTextFormat* format, const wchar_t* text) {
    IDWriteTextLayout* layout = nullptr;
    app.dwriteFactory->CreateTextLayout(text, static_cast<UINT32>(wcslen(text)), format, 10000, 100, &layout);
    DWRITE_TEXT_METRICS metrics{};
    if (layout) {
        layout->GetMetrics(&metrics);
        layout->Release();
    }
    return metrics.width;
}

std::string utf8(const std::wstring& text) { return std::string(text.begin(), text.end()); }
}  // namespace

int main() {
    {
        auto state = std::make_unique<App>();
        App& app = *state;
        if (!initD2D(app)) return 2;
        app.width = 1050;
        app.height = 900;
        const bool defaultInstalled = installed(app, kDefaultCodeFont);
#ifndef _WIN32
        check(defaultInstalled, "the Linux build ships the default code face");
#endif
        const std::wstring expected = defaultInstalled ? kDefaultCodeFont : L"Consolas";

        for (int theme : {0, 1, 3, 4, 6, 7, 8}) {  // the built-ins without a Cascadia face
            applyTheme(app, theme);
            check(app.theme.codeFontFamily == expected,
                  "built-in theme " + std::to_string(theme) + " codes in " + utf8(expected) + ", not " +
                      utf8(app.theme.codeFontFamily));
            for (IDWriteTextFormat* format : {app.codeFormat, app.codeBoldFormat, app.codeItalicFormat,
                                              app.codeBoldItalicFormat, app.editorGutterFormat}) {
                check(format && familyOf(format) == expected, "code and gutter formats use " + utf8(expected));
            }
            check(std::abs(widthOf(app, app.codeFormat, L"iiii") - widthOf(app, app.codeFormat, L"WWWW")) < 0.5f,
                  "the code face is monospaced");
        }
        check(app.uiCodeFont == expected, "interface mono text uses " + utf8(expected));

        // A theme naming a face that is not installed codes in the default
        // instead of a proportional fallback
        check(installedCodeFont(app, L"No Such Mono 262") == expected, "a missing code face resolves to the default");
        check(installedCodeFont(app, nullptr) == expected, "no code face resolves to the default");
    }
    CoUninitialize();
    std::cout << "Code font: " << failures << " failures\n";
    return failures ? 1 : 0;
}
