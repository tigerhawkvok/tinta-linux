// Text conversion, character classes and time for the Linux <windows.h>.
#include <windows.h>

#include <glib.h>

#include <ctime>

namespace {

constexpr char32_t kReplacement = 0xFFFD;

// Decodes one UTF-8 sequence starting at s[i]; advances i. Overlong forms,
// surrogates and values past U+10FFFF are invalid, as in the Windows decoder.
bool decodeUtf8(const unsigned char* s, size_t n, size_t& i, char32_t& out) {
    unsigned char c = s[i];
    if (c < 0x80) { out = c; i++; return true; }
    int extra;
    char32_t value;
    char32_t minimum;
    if ((c & 0xE0) == 0xC0) { extra = 1; value = c & 0x1F; minimum = 0x80; }
    else if ((c & 0xF0) == 0xE0) { extra = 2; value = c & 0x0F; minimum = 0x800; }
    else if ((c & 0xF8) == 0xF0) { extra = 3; value = c & 0x07; minimum = 0x10000; }
    else { i++; return false; }
    size_t j = i + 1;
    for (int k = 0; k < extra; k++, j++) {
        if (j >= n || (s[j] & 0xC0) != 0x80) { i = j; return false; }
        value = (value << 6) | (s[j] & 0x3F);
    }
    i = j;
    if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        return false;
    }
    out = value;
    return true;
}

int encodeUtf8(char32_t c, char* out) {
    if (c < 0x80) { out[0] = static_cast<char>(c); return 1; }
    if (c < 0x800) {
        out[0] = static_cast<char>(0xC0 | (c >> 6));
        out[1] = static_cast<char>(0x80 | (c & 0x3F));
        return 2;
    }
    if (c < 0x10000) {
        out[0] = static_cast<char>(0xE0 | (c >> 12));
        out[1] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out[2] = static_cast<char>(0x80 | (c & 0x3F));
        return 3;
    }
    out[0] = static_cast<char>(0xF0 | (c >> 18));
    out[1] = static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    out[2] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out[3] = static_cast<char>(0x80 | (c & 0x3F));
    return 4;
}

constexpr ULONGLONG kTicksPerSecond = 10000000ULL;
// Seconds between 1601-01-01 and 1970-01-01
constexpr ULONGLONG kEpochDelta = 11644473600ULL;

ULONGLONG toTicks(const FILETIME& ft) {
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

void fromTicks(ULONGLONG ticks, FILETIME& ft) {
    ft.dwLowDateTime = static_cast<DWORD>(ticks & 0xFFFFFFFFULL);
    ft.dwHighDateTime = static_cast<DWORD>(ticks >> 32);
}

void fillSystemTime(const struct tm& t, long milliseconds, SYSTEMTIME* st) {
    st->wYear = static_cast<WORD>(t.tm_year + 1900);
    st->wMonth = static_cast<WORD>(t.tm_mon + 1);
    st->wDayOfWeek = static_cast<WORD>(t.tm_wday);
    st->wDay = static_cast<WORD>(t.tm_mday);
    st->wHour = static_cast<WORD>(t.tm_hour);
    st->wMinute = static_cast<WORD>(t.tm_min);
    st->wSecond = static_cast<WORD>(t.tm_sec);
    st->wMilliseconds = static_cast<WORD>(milliseconds);
}

bool isLeap(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

}  // namespace

int MultiByteToWideChar(UINT codePage, DWORD flags, const char* source,
                        int sourceLength, wchar_t* dest, int destLength) {
    if ((codePage != CP_UTF8 && codePage != CP_ACP) || !source || destLength < 0) return 0;
    size_t n = sourceLength < 0 ? strlen(source) + 1 : static_cast<size_t>(sourceLength);
    const auto* s = reinterpret_cast<const unsigned char*>(source);
    int written = 0;
    for (size_t i = 0; i < n;) {
        char32_t c;
        if (!decodeUtf8(s, n, i, c)) {
            if (flags & MB_ERR_INVALID_CHARS) return 0;
            c = kReplacement;
        }
        if (destLength) {
            if (written >= destLength) return 0;
            dest[written] = static_cast<wchar_t>(c);
        }
        written++;
    }
    return written;
}

int WideCharToMultiByte(UINT codePage, DWORD flags, const wchar_t* source,
                        int sourceLength, char* dest, int destLength,
                        const char*, BOOL* usedDefaultChar) {
    if ((codePage != CP_UTF8 && codePage != CP_ACP) || !source || destLength < 0) return 0;
    if (usedDefaultChar) *usedDefaultChar = FALSE;
    size_t n = sourceLength < 0 ? wcslen(source) + 1 : static_cast<size_t>(sourceLength);
    int written = 0;
    char buffer[4];
    for (size_t i = 0; i < n; i++) {
        auto c = static_cast<char32_t>(source[i]);
        if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) {
            if (flags & WC_ERR_INVALID_CHARS) return 0;
            c = kReplacement;
        }
        int length = encodeUtf8(c, buffer);
        if (destLength) {
            if (written + length > destLength) return 0;
            memcpy(dest + written, buffer, length);
        }
        written += length;
    }
    return written;
}

BOOL GetStringTypeW(DWORD infoType, const wchar_t* source, int length, WORD* charTypes) {
    if (infoType != CT_CTYPE1 || !source || !charTypes) return FALSE;
    size_t n = length < 0 ? wcslen(source) + 1 : static_cast<size_t>(length);
    for (size_t i = 0; i < n; i++) {
        auto c = static_cast<gunichar>(source[i]);
        WORD type = 0;
        if (g_unichar_isupper(c)) type |= C1_UPPER;
        if (g_unichar_islower(c)) type |= C1_LOWER;
        if (g_unichar_isdigit(c)) type |= C1_DIGIT;
        if (g_unichar_isspace(c)) type |= C1_SPACE;
        if (g_unichar_ispunct(c)) type |= C1_PUNCT;
        if (g_unichar_iscntrl(c)) type |= C1_CNTRL;
        if (c == ' ' || c == '\t' || g_unichar_type(c) == G_UNICODE_SPACE_SEPARATOR) type |= C1_BLANK;
        if (g_unichar_isxdigit(c) && c < 0x80) type |= C1_XDIGIT;
        if (g_unichar_isalpha(c)) type |= C1_ALPHA;
        charTypes[i] = type;
    }
    return TRUE;
}

BOOL SystemTimeToFileTime(const SYSTEMTIME* st, FILETIME* ft) {
    static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (!st || !ft) return FALSE;
    if (st->wYear < 1601 || st->wYear > 30827 || st->wMonth < 1 || st->wMonth > 12 ||
        st->wDay < 1 || st->wHour > 23 || st->wMinute > 59 || st->wSecond > 59 ||
        st->wMilliseconds > 999) {
        return FALSE;
    }
    int monthDays = kDays[st->wMonth - 1] + (st->wMonth == 2 && isLeap(st->wYear));
    if (st->wDay > monthDays) return FALSE;
    // Days since 1601-01-01 (proleptic Gregorian, as Windows counts them)
    long long y = st->wYear - 1601;
    long long days = y * 365 + y / 4 - y / 100 + y / 400;
    for (int m = 1; m < st->wMonth; m++) days += kDays[m - 1] + (m == 2 && isLeap(st->wYear));
    days += st->wDay - 1;
    ULONGLONG seconds = static_cast<ULONGLONG>(days) * 86400ULL + st->wHour * 3600ULL +
                        st->wMinute * 60ULL + st->wSecond;
    fromTicks(seconds * kTicksPerSecond + st->wMilliseconds * 10000ULL, *ft);
    return TRUE;
}

BOOL FileTimeToSystemTime(const FILETIME* ft, SYSTEMTIME* st) {
    if (!ft || !st) return FALSE;
    ULONGLONG ticks = toTicks(*ft);
    if (ticks >> 63) return FALSE;
    auto seconds = static_cast<long long>(ticks / kTicksPerSecond) -
                   static_cast<long long>(kEpochDelta);
    auto milliseconds = static_cast<long>((ticks % kTicksPerSecond) / 10000ULL);
    time_t t = static_cast<time_t>(seconds);
    struct tm parts {};
    if (!gmtime_r(&t, &parts)) return FALSE;
    fillSystemTime(parts, milliseconds, st);
    return TRUE;
}

void GetSystemTime(SYSTEMTIME* st) {
    struct timespec now {};
    clock_gettime(CLOCK_REALTIME, &now);
    struct tm parts {};
    gmtime_r(&now.tv_sec, &parts);
    fillSystemTime(parts, now.tv_nsec / 1000000L, st);
}

void GetLocalTime(SYSTEMTIME* st) {
    struct timespec now {};
    clock_gettime(CLOCK_REALTIME, &now);
    struct tm parts {};
    localtime_r(&now.tv_sec, &parts);
    fillSystemTime(parts, now.tv_nsec / 1000000L, st);
}

void GetSystemTimeAsFileTime(FILETIME* ft) {
    struct timespec now {};
    clock_gettime(CLOCK_REALTIME, &now);
    ULONGLONG ticks = (static_cast<ULONGLONG>(now.tv_sec) + kEpochDelta) * kTicksPerSecond +
                      static_cast<ULONGLONG>(now.tv_nsec) / 100ULL;
    fromTicks(ticks, *ft);
}

ULONGLONG GetTickCount64() {
    struct timespec now {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<ULONGLONG>(now.tv_sec) * 1000ULL +
           static_cast<ULONGLONG>(now.tv_nsec) / 1000000ULL;
}
