// swprintf_s reads its format as MSVC does (msvc_crt.h): %s and %c are
// wide, %hs/%hc and %S/%C narrow. Shared code and languages.ini strings are
// written that way.
#include <cstdio>
#include <string>

namespace {

int failures = 0;

void expect(const wchar_t* got, const wchar_t* want, const char* what) {
    if (std::wstring(got) != want) {
        std::fprintf(stderr, "FAIL: %s: got [%ls] want [%ls]\n", what, got, want);
        failures++;
    }
}

}  // namespace

int main() {
    wchar_t text[128];
    std::wstring number = L"1,234";

    swprintf_s(text, L"%s words", number.c_str());
    expect(text, L"1,234 words", "%s is a wide string");
    swprintf_s(text, _countof(text), L"%stinta-pandoc-%lu%s", L"/tmp/", 42ul, L".md");
    expect(text, L"/tmp/tinta-pandoc-42.md", "the counted overload, as pandoc.cpp builds its temp file");
    swprintf_s(text, L"%ls is undefined", L"C-x z");
    expect(text, L"C-x z is undefined", "%ls stays wide");
    swprintf_s(text, L"%hs and %S", "narrow", "also narrow");
    expect(text, L"narrow and also narrow", "%hs and %S are narrow");
    swprintf_s(text, L"[%c%hc%C]", L'\u00e9', 'x', 'y');
    expect(text, L"[\u00e9xy]", "%c is wide, %hc and %C narrow");
    swprintf_s(text, L"[%-6s|%6s|%.2s]", L"ab", L"cd", L"efgh");
    expect(text, L"[ab    |    cd|ef]", "flags, width and precision carry over");
    swprintf_s(text, L"%*s|%-*.*s|", 4, L"a", 5, 2, L"bcd");
    expect(text, L"   a|bc   |", "star widths and precisions");
    swprintf_s(text, L"100%% of %d, %5.1f, %x, %zu, %lld", 7, 2.5, 255, (size_t)9, -3ll);
    expect(text, L"100% of 7,   2.5, ff, 9, -3", "numbers and %% untouched");
    swprintf_s(text, L"%%s stays literal");
    expect(text, L"%s stays literal", "an escaped percent before s");

    std::printf("crt: %d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
