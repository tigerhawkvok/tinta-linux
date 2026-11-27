// MSVC C runtime spellings used by shared code without including any Windows
// header (they come from <string.h>/<stdio.h>/<wchar.h> there). Force-included
// on Linux through tinta_prelude.h.
#ifndef TINTA_LINUX_MSVC_CRT_H
#define TINTA_LINUX_MSVC_CRT_H

#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <strings.h>

#ifndef _countof
#define _countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

inline int _stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
inline int _strnicmp(const char* a, const char* b, size_t n) { return strncasecmp(a, b, n); }
inline int _wcsicmp(const wchar_t* a, const wchar_t* b) { return wcscasecmp(a, b); }
// std::filesystem::path::c_str() is wide on Windows and UTF-8 here
inline int _wcsicmp(const char* a, const char* b) { return strcasecmp(a, b); }
inline int _wcsnicmp(const wchar_t* a, const wchar_t* b, size_t n) { return wcsncasecmp(a, b, n); }
inline int _wtoi(const wchar_t* s) { return static_cast<int>(wcstol(s, nullptr, 10)); }

// Shared code only passes numeric conversions to sscanf_s, so the extra
// size arguments that %s/%c take under MSVC never arise.
#define sscanf_s sscanf

template <size_t N>
inline int sprintf_s(char (&buffer)[N], const char* format, ...) {
    va_list args;
    va_start(args, format);
    int written = vsnprintf(buffer, N, format, args);
    va_end(args);
    return written;
}

inline int sprintf_s(char* buffer, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int written = vsnprintf(buffer, size, format, args);
    va_end(args);
    return written;
}

namespace tinta_crt {

// A wide format as MSVC reads it, rewritten for glibc. MSVC's wide printf
// takes %s and %c as wide, and %hs/%hc or %S/%C as narrow; glibc takes %s
// and %c as narrow and wants %ls/%lc for wide. Shared code is written for
// MSVC, and so are languages.ini strings ("%s words" printed only the
// first digit of the count, and Pandoc's temp file landed in "/").
inline std::wstring msvcWideFormat(const wchar_t* format) {
    std::wstring out;
    const wchar_t* p = format;
    auto digits = [&] {
        while (*p == L'*' || (*p >= L'0' && *p <= L'9')) out += *p++;
    };
    while (*p) {
        if (*p != L'%') {
            out += *p++;
            continue;
        }
        out += *p++;
        if (*p == L'%') {
            out += *p++;
            continue;
        }
        while (*p && std::wcschr(L"-+ #0'", *p)) out += *p++;
        digits();
        if (*p == L'.') {
            out += *p++;
            digits();
        }
        std::wstring length;
        while (*p && std::wcschr(L"hlLjzt", *p)) length += *p++;
        const wchar_t conversion = *p;
        if (!conversion) break;
        p++;
        const bool text = conversion == L's' || conversion == L'c';
        if (text && length.empty()) {
            out += L'l';  // wide, as MSVC means it
        } else if (text && length == L"h") {
            // narrow: glibc's plain %s / %c
        } else if ((conversion == L'S' || conversion == L'C') && length.empty()) {
            out += conversion == L'S' ? L's' : L'c';  // MSVC's narrow spelling
            continue;
        } else {
            out += length;
        }
        out += conversion;
    }
    return out;
}

}  // namespace tinta_crt

template <size_t N>
inline int swprintf_s(wchar_t (&buffer)[N], const wchar_t* format, ...) {
    const std::wstring glibc = tinta_crt::msvcWideFormat(format);
    va_list args;
    va_start(args, format);
    int written = vswprintf(buffer, N, glibc.c_str(), args);
    va_end(args);
    return written;
}

inline int swprintf_s(wchar_t* buffer, size_t count, const wchar_t* format, ...) {
    const std::wstring glibc = tinta_crt::msvcWideFormat(format);
    va_list args;
    va_start(args, format);
    int written = vswprintf(buffer, count, glibc.c_str(), args);
    va_end(args);
    return written;
}

#define _TRUNCATE ((size_t)-1)

inline int wcscpy_s(wchar_t* dest, size_t count, const wchar_t* src) {
    if (!dest || !count) return EINVAL;
    if (wcslen(src) >= count) { dest[0] = L'\0'; return ERANGE; }
    wcscpy(dest, src);
    return 0;
}

template <size_t N>
inline int wcscpy_s(wchar_t (&dest)[N], const wchar_t* src) {
    return wcscpy_s(dest, N, src);
}

inline int wcsncpy_s(wchar_t* dest, size_t count, const wchar_t* src, size_t max) {
    if (!dest || !count) return EINVAL;
    size_t length = wcsnlen(src, max == _TRUNCATE ? count - 1 : max);
    if (length >= count) { dest[0] = L'\0'; return ERANGE; }
    wmemcpy(dest, src, length);
    dest[length] = L'\0';
    return 0;
}

template <size_t N>
inline int wcsncpy_s(wchar_t (&dest)[N], const wchar_t* src, size_t max) {
    return wcsncpy_s(dest, N, src, max);
}

#endif  // TINTA_LINUX_MSVC_CRT_H
