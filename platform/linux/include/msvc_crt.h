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

// MSVC reads %s in a wide format as a wide string; glibc reads it as a narrow
// one. Shared code must use %ls for wide strings.
template <size_t N>
inline int swprintf_s(wchar_t (&buffer)[N], const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    int written = vswprintf(buffer, N, format, args);
    va_end(args);
    return written;
}

inline int swprintf_s(wchar_t* buffer, size_t count, const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    int written = vswprintf(buffer, count, format, args);
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
