// Win32 files, processes, threads, environment, locale, shell, registry and
// downloads for the Linux compat layer.
//
// Paths: shared code builds Windows-syntax wide paths; every entry point
// converts them with nativePath() ('\' becomes '/'). Paths handed back are
// native (UTF-8 decoded to wide, '/' separators).
#include "compat_internal.h"

#include <shellapi.h>
#include <shlobj.h>
#include <dbghelp.h>
#include <d3d11.h>
#include <winhttp.h>

#include <gio/gio.h>

#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <langinfo.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

#ifdef TINTA_HAVE_CURL
#include <curl/curl.h>
#endif

extern char** environ;

namespace {

thread_local DWORD lastError = ERROR_SUCCESS;

DWORD errorFromErrno(int error) {
    switch (error) {
        case 0: return ERROR_SUCCESS;
        case ENOENT: return ERROR_FILE_NOT_FOUND;
        case ENOTDIR: return ERROR_PATH_NOT_FOUND;
        case EACCES:
        case EPERM:
        case EROFS: return ERROR_ACCESS_DENIED;
        case EEXIST: return ERROR_ALREADY_EXISTS;
        default: return 31;  // ERROR_GEN_FAILURE
    }
}

void failWithErrno() { lastError = errorFromErrno(errno); }

// Writes `text` into a Win32 buffer: the length without the terminator on
// success, or the size needed with it when the buffer is too small
DWORD copyOut(const std::wstring& text, LPWSTR buffer, DWORD size) {
    if (!buffer || size <= text.size()) return static_cast<DWORD>(text.size() + 1);
    wmemcpy(buffer, text.c_str(), text.size() + 1);
    return static_cast<DWORD>(text.size());
}

std::wstring wide(const std::string& s) { return tinta::toWide(s); }
std::string native(LPCWSTR path) { return path ? tinta::nativePath(path) : std::string(); }

// ---- Handles ---------------------------------------------------------------------------
struct TintaHandle {
    virtual ~TintaHandle() = default;
};

struct FileHandle final : TintaHandle {
    explicit FileHandle(int fd) : fd(fd) {}
    ~FileHandle() override { ::close(fd); }
    int fd;
};

struct FindHandle final : TintaHandle {
    std::vector<WIN32_FIND_DATAW> entries;
    size_t next = 0;
};

struct ProcessHandle final : TintaHandle {
    ProcessHandle(pid_t pid, bool child) : pid(pid), child(child) {}
    // Waits up to `ms` (INFINITE: forever) for the process to exit: a child
    // is reaped with its exit code; another process (OpenProcess) counts as
    // exited once it is gone or a zombie, its exit code unknown (0)
    bool wait(DWORD ms) {
        if (exited) return true;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (true) {
            if (child) {
                int status = 0;
                pid_t r = waitpid(pid, &status, WNOHANG);
                if (r == pid) {
                    exited = true;
                    exitCode = WIFEXITED(status) ? static_cast<DWORD>(WEXITSTATUS(status)) : 1;
                    return true;
                }
                if (r < 0 && errno != EINTR) return false;
            } else if (gone()) {
                exited = true;
                exitCode = 0;
                return true;
            }
            if (ms != INFINITE && std::chrono::steady_clock::now() >= deadline) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    // Another process's end: no such process, or a zombie its parent has
    // not reaped yet
    bool gone() const {
        if (::kill(pid, 0) != 0) return errno == ESRCH;
        char path[64];
        std::snprintf(path, sizeof path, "/proc/%d/stat", static_cast<int>(pid));
        FILE* f = std::fopen(path, "r");
        if (!f) return true;
        char stat[512];
        size_t n = std::fread(stat, 1, sizeof stat - 1, f);
        std::fclose(f);
        stat[n] = 0;
        const char* name = std::strrchr(stat, ')');  // the state follows the name
        return name && name[1] == ' ' && name[2] == 'Z';
    }
    pid_t pid;
    bool child;
    bool exited = false;
    DWORD exitCode = 0;
};

struct ThreadState {
    std::mutex mutex;
    std::condition_variable done;
    bool finished = false;
    DWORD exitCode = 0;
};

struct ThreadHandle final : TintaHandle {
    std::shared_ptr<ThreadState> state;
};

struct EventHandle final : TintaHandle {
    std::mutex mutex;
    std::condition_variable changed;
    bool manualReset = false;
    bool signaled = false;
    // Takes the signal if it is set (an auto-reset event lets one waiter in)
    bool tryTake() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!signaled) return false;
        if (!manualReset) signaled = false;
        return true;
    }
};

// A job's process groups; with KILL_ON_JOB_CLOSE, closing the job ends
// whatever is still running in them
struct JobHandle final : TintaHandle {
    ~JobHandle() override {
        if (killOnClose) killAll();
    }
    void killAll() {
        for (pid_t group : groups) ::kill(-group, SIGKILL);
    }
    std::vector<pid_t> groups;
    bool killOnClose = false;
};

TintaHandle* handleOf(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE) return nullptr;
    return static_cast<TintaHandle*>(h);
}

// ---- Files --------------------------------------------------------------------------------
void fileTimeFrom(const struct timespec& ts, FILETIME& ft) {
    constexpr ULONGLONG kEpochDelta = 11644473600ULL;
    ULONGLONG ticks = (static_cast<ULONGLONG>(ts.tv_sec) + kEpochDelta) * 10000000ULL + ts.tv_nsec / 100;
    ft.dwLowDateTime = static_cast<DWORD>(ticks & 0xFFFFFFFFULL);
    ft.dwHighDateTime = static_cast<DWORD>(ticks >> 32);
}

DWORD attributesOf(const std::string& path, const struct stat& st) {
    DWORD attrs = 0;
    if (S_ISDIR(st.st_mode)) attrs |= FILE_ATTRIBUTE_DIRECTORY;
    std::string name = path.substr(path.find_last_of('/') == std::string::npos ? 0 : path.find_last_of('/') + 1);
    if (name.size() > 1 && name[0] == '.' && name != "..") attrs |= FILE_ATTRIBUTE_HIDDEN;
    if (access(path.c_str(), W_OK) != 0 && errno == EACCES) attrs |= FILE_ATTRIBUTE_READONLY;
    return attrs ? attrs : FILE_ATTRIBUTE_NORMAL;
}

void fillFindData(const std::string& dir, const std::string& name, WIN32_FIND_DATAW& data) {
    std::memset(&data, 0, sizeof data);
    std::string full = dir + "/" + name;
    struct stat st {};
    if (stat(full.c_str(), &st) == 0) {
        data.dwFileAttributes = attributesOf(full, st);
        fileTimeFrom(st.st_ctim, data.ftCreationTime);
        fileTimeFrom(st.st_atim, data.ftLastAccessTime);
        fileTimeFrom(st.st_mtim, data.ftLastWriteTime);
        auto size = static_cast<unsigned long long>(st.st_size);
        data.nFileSizeHigh = static_cast<DWORD>(size >> 32);
        data.nFileSizeLow = static_cast<DWORD>(size & 0xFFFFFFFFULL);
    } else {
        data.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    }
    std::wstring w = wide(name);
    size_t n = std::min<size_t>(w.size(), MAX_PATH - 1);
    wmemcpy(data.cFileName, w.c_str(), n);
    data.cFileName[n] = 0;
}

}  // namespace

BOOL GetFileAttributesExW(LPCWSTR path, GET_FILEEX_INFO_LEVELS level, void* info) {
    if (!path || !info || level != GetFileExInfoStandard) {
        lastError = 87;
        return FALSE;
    }
    std::string full = native(path);
    struct stat st {};
    if (stat(full.c_str(), &st) != 0) {
        lastError = errno == ENOENT ? ERROR_FILE_NOT_FOUND : 5;
        return FALSE;
    }
    auto* data = static_cast<WIN32_FILE_ATTRIBUTE_DATA*>(info);
    data->dwFileAttributes = attributesOf(full, st);
    fileTimeFrom(st.st_ctim, data->ftCreationTime);
    fileTimeFrom(st.st_atim, data->ftLastAccessTime);
    fileTimeFrom(st.st_mtim, data->ftLastWriteTime);
    auto size = static_cast<unsigned long long>(st.st_size);
    data->nFileSizeHigh = static_cast<DWORD>(size >> 32);
    data->nFileSizeLow = static_cast<DWORD>(size & 0xFFFFFFFFULL);
    return TRUE;
}

namespace {

// ---- Command lines (CommandLineToArgvW rules) ------------------------------------------------
std::vector<std::wstring> splitCommandLine(const std::wstring& line) {
    std::vector<std::wstring> args;
    size_t i = 0, n = line.size();
    while (i < n) {
        while (i < n && (line[i] == L' ' || line[i] == L'\t')) i++;
        if (i >= n) break;
        std::wstring arg;
        bool quoted = false;
        while (i < n && (quoted || (line[i] != L' ' && line[i] != L'\t'))) {
            if (line[i] == L'\\') {
                size_t slashes = 0;
                while (i < n && line[i] == L'\\') {
                    slashes++;
                    i++;
                }
                if (i < n && line[i] == L'"') {
                    arg.append(slashes / 2, L'\\');
                    if (slashes % 2) {
                        arg += L'"';
                        i++;
                    }
                } else {
                    arg.append(slashes, L'\\');
                }
            } else if (line[i] == L'"') {
                if (quoted && i + 1 < n && line[i + 1] == L'"') {
                    arg += L'"';
                    i += 2;
                } else {
                    quoted = !quoted;
                    i++;
                }
            } else {
                arg += line[i++];
            }
        }
        args.push_back(arg);
    }
    return args;
}

std::wstring quoteArgument(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos) return arg;
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            slashes++;
            continue;
        }
        if (c == L'"') out.append(slashes * 2 + 1, L'\\');
        else out.append(slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    return out + L"\"";
}

std::string selfExe() {
    char buffer[4096];
    ssize_t n = readlink("/proc/self/exe", buffer, sizeof buffer - 1);
    return n > 0 ? std::string(buffer, static_cast<size_t>(n)) : std::string();
}

bool sameFile(const std::string& a, const std::string& b) {
    struct stat sa {}, sb {};
    return stat(a.c_str(), &sa) == 0 && stat(b.c_str(), &sb) == 0 && sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

// Starts `argv` detached (GLib double-forks, so no zombie is left behind)
bool spawnDetached(const std::vector<std::string>& argv, const std::string& directory) {
    std::vector<char*> raw;
    for (const std::string& a : argv) raw.push_back(const_cast<char*>(a.c_str()));
    raw.push_back(nullptr);
    GError* error = nullptr;
    gboolean ok = g_spawn_async(directory.empty() ? nullptr : directory.c_str(), raw.data(), nullptr,
                                G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, &error);
    if (error) g_error_free(error);
    return ok;
}

// ---- Locale ------------------------------------------------------------------------------------
// "de_DE.UTF-8@euro" -> "de-DE"; "" or "C"/"POSIX" -> ""
std::wstring localeTag(const char* value) {
    if (!value || !*value) return L"";
    std::string s = value;
    s = s.substr(0, s.find_first_of(".@"));
    if (s == "C" || s == "POSIX") return L"";
    std::replace(s.begin(), s.end(), '_', '-');
    return wide(s);
}

std::wstring messagesLocale() {
    for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        std::wstring tag = localeTag(std::getenv(var));
        if (!tag.empty()) return tag;
    }
    return L"en-US";
}

std::wstring measurementLocale() {
    for (const char* var : {"LC_ALL", "LC_MEASUREMENT", "LANG"}) {
        std::wstring tag = localeTag(std::getenv(var));
        if (!tag.empty()) return tag;
    }
    return L"en-US";
}

std::wstring homeDir() {
    const char* home = std::getenv("HOME");
    return wide(home && *home ? home : "/");
}

std::wstring xdgDir(const char* var, const wchar_t* fallback) {
    const char* value = std::getenv(var);
    if (value && *value == '/') return wide(value);
    return homeDir() + fallback;
}

}  // namespace

// ---- Errors ------------------------------------------------------------------------------------
DWORD GetLastError() { return lastError; }
void SetLastError(DWORD error) { lastError = error; }

BOOL CloseHandle(HANDLE handle) {
    TintaHandle* h = handleOf(handle);
    if (!h) return FALSE;
    delete h;
    return TRUE;
}

// ---- Files ---------------------------------------------------------------------------------------
HANDLE CreateFileW(LPCWSTR path, DWORD access, DWORD share, SECURITY_ATTRIBUTES* security, DWORD disposition,
                   DWORD flags, HANDLE templateFile) {
    return CreateFileW(native(path).c_str(), access, share, security, disposition, flags, templateFile);
}

// Share modes are not enforced: Linux has no mandatory locks
HANDLE CreateFileW(const char* path, DWORD access, DWORD, SECURITY_ATTRIBUTES*, DWORD disposition, DWORD, HANDLE) {
    std::string p = path ? path : "";
    int flags = O_CLOEXEC;
    bool read = (access & GENERIC_READ) != 0, write = (access & GENERIC_WRITE) != 0;
    flags |= write ? (read ? O_RDWR : O_WRONLY) : O_RDONLY;
    switch (disposition) {
        case CREATE_NEW: flags |= O_CREAT | O_EXCL; break;
        case CREATE_ALWAYS: flags |= O_CREAT | O_TRUNC; break;
        case OPEN_ALWAYS: flags |= O_CREAT; break;
        case 5: flags |= O_TRUNC; break;  // TRUNCATE_EXISTING
        default: break;
    }
    int fd = ::open(p.c_str(), flags, 0644);
    if (fd < 0) {
        failWithErrno();
        return INVALID_HANDLE_VALUE;
    }
    lastError = ERROR_SUCCESS;
    return static_cast<TintaHandle*>(new FileHandle(fd));
}

BOOL ReadFile(HANDLE file, void* buffer, DWORD toRead, DWORD* read, void*) {
    auto* h = dynamic_cast<FileHandle*>(handleOf(file));
    if (!h) return FALSE;
    ssize_t n = ::read(h->fd, buffer, toRead);
    if (n < 0) {
        failWithErrno();
        return FALSE;
    }
    if (read) *read = static_cast<DWORD>(n);
    return TRUE;
}

BOOL WriteFile(HANDLE file, const void* buffer, DWORD toWrite, DWORD* written, void*) {
    auto* h = dynamic_cast<FileHandle*>(handleOf(file));
    if (!h) return FALSE;
    ssize_t n = ::write(h->fd, buffer, toWrite);
    if (n < 0) {
        failWithErrno();
        return FALSE;
    }
    if (written) *written = static_cast<DWORD>(n);
    return TRUE;
}

BOOL GetFileTime(HANDLE file, FILETIME* creation, FILETIME* access, FILETIME* write) {
    auto* h = dynamic_cast<FileHandle*>(handleOf(file));
    struct stat st {};
    if (!h || fstat(h->fd, &st) != 0) return FALSE;
    if (creation) fileTimeFrom(st.st_ctim, *creation);
    if (access) fileTimeFrom(st.st_atim, *access);
    if (write) fileTimeFrom(st.st_mtim, *write);
    return TRUE;
}

DWORD GetFinalPathNameByHandleW(HANDLE file, LPWSTR path, DWORD size, DWORD) {
    auto* h = dynamic_cast<FileHandle*>(handleOf(file));
    if (!h) return 0;
    char link[64], target[4096];
    std::snprintf(link, sizeof link, "/proc/self/fd/%d", h->fd);
    ssize_t n = readlink(link, target, sizeof target - 1);
    if (n <= 0) return 0;
    return copyOut(wide(std::string(target, static_cast<size_t>(n))), path, size);
}

DWORD GetFileAttributesW(LPCWSTR path) {
    std::string p = native(path);
    struct stat st {};
    if (stat(p.c_str(), &st) != 0) {
        failWithErrno();
        return INVALID_FILE_ATTRIBUTES;
    }
    return attributesOf(p, st);
}

BOOL CreateDirectoryW(LPCWSTR path, SECURITY_ATTRIBUTES*) {
    if (mkdir(native(path).c_str(), 0755) != 0) {
        failWithErrno();
        return FALSE;
    }
    return TRUE;
}

BOOL DeleteFileW(LPCWSTR path) {
    if (unlink(native(path).c_str()) != 0) {
        failWithErrno();
        return FALSE;
    }
    return TRUE;
}

BOOL CopyFileW(LPCWSTR from, LPCWSTR to, BOOL failIfExists) {
    std::error_code ec;
    auto options = failIfExists ? std::filesystem::copy_options::none
                                : std::filesystem::copy_options::overwrite_existing;
    if (!std::filesystem::copy_file(native(from), native(to), options, ec)) {
        lastError = ec ? errorFromErrno(ec.value()) : ERROR_ALREADY_EXISTS;
        return FALSE;
    }
    return TRUE;
}

BOOL MoveFileExW(LPCWSTR from, LPCWSTR to, DWORD flags) {
    std::string a = native(from), b = native(to);
    if (!(flags & MOVEFILE_REPLACE_EXISTING) && access(b.c_str(), F_OK) == 0) {
        lastError = ERROR_ALREADY_EXISTS;
        return FALSE;
    }
    if (rename(a.c_str(), b.c_str()) == 0) return TRUE;
    if (errno != EXDEV) {
        failWithErrno();
        return FALSE;
    }
    std::error_code ec;
    std::filesystem::copy_file(a, b, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec || unlink(a.c_str()) != 0) {
        lastError = ERROR_ACCESS_DENIED;
        return FALSE;
    }
    return TRUE;
}

// `pattern` is "<dir>\<glob>"; matching ignores case, as on Windows
HANDLE FindFirstFileW(LPCWSTR pattern, WIN32_FIND_DATAW* data) {
    std::string p = native(pattern);
    size_t slash = p.find_last_of('/');
    std::string dir = slash == std::string::npos ? "." : (slash == 0 ? "/" : p.substr(0, slash));
    std::string glob = slash == std::string::npos ? p : p.substr(slash + 1);
    if (glob == "*.*") glob = "*";
    DIR* d = opendir(dir.c_str());
    if (!d) {
        failWithErrno();
        return INVALID_HANDLE_VALUE;
    }
    auto* find = new FindHandle();
    while (dirent* entry = readdir(d)) {
        if (fnmatch(glob.c_str(), entry->d_name, FNM_CASEFOLD) != 0) continue;
        find->entries.emplace_back();
        fillFindData(dir, entry->d_name, find->entries.back());
    }
    closedir(d);
    if (find->entries.empty()) {
        delete find;
        lastError = ERROR_FILE_NOT_FOUND;
        return INVALID_HANDLE_VALUE;
    }
    *data = find->entries[find->next++];
    return static_cast<TintaHandle*>(find);
}

BOOL FindNextFileW(HANDLE handle, WIN32_FIND_DATAW* data) {
    auto* find = dynamic_cast<FindHandle*>(handleOf(handle));
    if (!find || find->next >= find->entries.size()) {
        lastError = 18;  // ERROR_NO_MORE_FILES
        return FALSE;
    }
    *data = find->entries[find->next++];
    return TRUE;
}

BOOL FindClose(HANDLE handle) { return CloseHandle(handle); }

BOOL RemoveDirectoryW(LPCWSTR path) {
    if (rmdir(native(path).c_str()) != 0) {
        failWithErrno();
        return FALSE;
    }
    return TRUE;
}

BOOL MoveFileW(LPCWSTR from, LPCWSTR to) { return MoveFileExW(from, to, 0); }

BOOL SetFileTime(HANDLE file, const FILETIME*, const FILETIME* access, const FILETIME* write) {
    auto* h = dynamic_cast<FileHandle*>(handleOf(file));
    if (!h) return FALSE;
    auto toTimespec = [](const FILETIME* ft) {
        struct timespec ts {};
        if (!ft) {
            ts.tv_nsec = UTIME_OMIT;
            return ts;
        }
        constexpr ULONGLONG kEpochDelta = 11644473600ULL;
        ULONGLONG ticks = (static_cast<ULONGLONG>(ft->dwHighDateTime) << 32) | ft->dwLowDateTime;
        ts.tv_sec = static_cast<time_t>(ticks / 10000000ULL - kEpochDelta);
        ts.tv_nsec = static_cast<long>((ticks % 10000000ULL) * 100);
        return ts;
    };
    struct timespec times[2] = {toTimespec(access), toTimespec(write)};
    if (futimens(h->fd, times) != 0) {
        failWithErrno();
        return FALSE;
    }
    return TRUE;
}

// Crash dialogs do not exist here
UINT SetErrorMode(UINT) { return 0; }

DWORD GetFullPathNameW(LPCWSTR path, DWORD size, LPWSTR buffer, LPWSTR* filePart) {
    if (!path) return 0;
    std::filesystem::path p = native(path);
    if (p.is_relative()) {
        std::error_code ec;
        p = std::filesystem::current_path(ec) / p;
    }
    std::wstring full = wide(p.lexically_normal().string());
    DWORD result = copyOut(full, buffer, size);
    if (filePart && buffer && result < size) {
        size_t slash = full.find_last_of(L'/');
        *filePart = slash == std::wstring::npos || slash + 1 >= full.size() ? nullptr : buffer + slash + 1;
    }
    return result;
}

LONG CompareFileTime(const FILETIME* a, const FILETIME* b) {
    ULONGLONG x = (static_cast<ULONGLONG>(a->dwHighDateTime) << 32) | a->dwLowDateTime;
    ULONGLONG y = (static_cast<ULONGLONG>(b->dwHighDateTime) << 32) | b->dwLowDateTime;
    return x < y ? -1 : x > y ? 1 : 0;
}

BOOL FileTimeToLocalFileTime(const FILETIME* utc, FILETIME* local) {
    if (!utc || !local) return FALSE;
    time_t now = time(nullptr);
    struct tm tm {};
    localtime_r(&now, &tm);
    ULONGLONG ticks = (static_cast<ULONGLONG>(utc->dwHighDateTime) << 32) | utc->dwLowDateTime;
    ticks += static_cast<long long>(tm.tm_gmtoff) * 10000000LL;
    local->dwLowDateTime = static_cast<DWORD>(ticks & 0xFFFFFFFFULL);
    local->dwHighDateTime = static_cast<DWORD>(ticks >> 32);
    return TRUE;
}

// ---- Module, directories, environment ----------------------------------------------------------
DWORD GetModuleFileNameW(HMODULE, LPWSTR path, DWORD size) {
    std::wstring exe = wide(selfExe());
    if (!path || size == 0) return 0;
    if (exe.size() >= size) {
        wmemcpy(path, exe.c_str(), size - 1);
        path[size - 1] = 0;
        lastError = 122;  // ERROR_INSUFFICIENT_BUFFER
        return size;
    }
    wmemcpy(path, exe.c_str(), exe.size() + 1);
    return static_cast<DWORD>(exe.size());
}

HMODULE GetModuleHandleW(LPCWSTR) {
    static char self;
    return reinterpret_cast<HMODULE>(&self);
}

DWORD GetCurrentDirectoryW(DWORD size, LPWSTR buffer) {
    char cwd[4096];
    if (!getcwd(cwd, sizeof cwd)) return 0;
    return copyOut(wide(cwd), buffer, size);
}

BOOL SetCurrentDirectoryW(LPCWSTR path) {
    if (chdir(native(path).c_str()) != 0) {
        failWithErrno();
        return FALSE;
    }
    return TRUE;
}

DWORD GetTempPathW(DWORD size, LPWSTR buffer) {
    const char* tmp = std::getenv("TMPDIR");
    std::wstring dir = wide(tmp && *tmp == '/' ? tmp : "/tmp");
    if (dir.back() != L'/') dir += L'/';
    return copyOut(dir, buffer, size);
}

// Where Tinta parks its working directory so it never holds a folder busy
UINT GetSystemDirectoryW(LPWSTR buffer, UINT size) { return copyOut(L"/", buffer, size); }

DWORD GetEnvironmentVariableW(LPCWSTR name, LPWSTR buffer, DWORD size) {
    if (!name) return 0;
    const char* value = std::getenv(tinta::toUtf8(name, wcslen(name)).c_str());
    if (!value) {
        lastError = 203;  // ERROR_ENVVAR_NOT_FOUND
        return 0;
    }
    return copyOut(wide(value), buffer, size);
}

// A null value removes the variable
BOOL SetEnvironmentVariableW(LPCWSTR name, LPCWSTR value) {
    if (!name || !*name) {
        lastError = 87;
        return FALSE;
    }
    std::string key = tinta::toUtf8(name, wcslen(name));
    int result = value ? setenv(key.c_str(), tinta::toUtf8(value, wcslen(value)).c_str(), 1) : unsetenv(key.c_str());
    if (result != 0) {
        failWithErrno();
        return FALSE;
    }
    return TRUE;
}

// %VAR% from the environment (unknown names stay as written), plus a
// leading ~ for the home folder, which Linux users type in path boxes
DWORD ExpandEnvironmentStringsW(LPCWSTR source, LPWSTR dest, DWORD size) {
    if (!source) return 0;
    std::wstring in = source, out;
    if (in == L"~" || in.rfind(L"~/", 0) == 0) in = homeDir() + in.substr(1);
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == L'%') {
            size_t end = in.find(L'%', i + 1);
            if (end != std::wstring::npos && end > i + 1) {
                std::string name = tinta::toUtf8(in.substr(i + 1, end - i - 1));
                if (const char* value = std::getenv(name.c_str())) {
                    out += wide(value);
                    i = end;
                    continue;
                }
            }
        }
        out += in[i];
    }
    // Returns the size with the terminator, success or not
    DWORD needed = static_cast<DWORD>(out.size() + 1);
    if (dest && size >= needed) wmemcpy(dest, out.c_str(), needed);
    return needed;
}

// PATH lookup; "tool.exe" also finds "tool", so shared code's Windows
// executable names resolve to their Linux binaries
DWORD SearchPathW(LPCWSTR path, LPCWSTR fileName, LPCWSTR extension, DWORD size, LPWSTR buffer, LPWSTR* filePart) {
    if (!fileName) return 0;
    std::string name = native(fileName);
    std::vector<std::string> candidates{name};
    if (extension && name.find('.') == std::string::npos) candidates.push_back(name + native(extension));
    if (name.size() > 4 && strcasecmp(name.c_str() + name.size() - 4, ".exe") == 0) {
        candidates.push_back(name.substr(0, name.size() - 4));
    }
    std::vector<std::string> dirs;
    std::string list = path ? native(path) : (std::getenv("PATH") ? std::getenv("PATH") : "/usr/bin:/bin");
    for (size_t start = 0; start <= list.size();) {
        size_t end = list.find_first_of(path ? ";:" : ":", start);
        if (end == std::string::npos) end = list.size();
        if (end > start) dirs.push_back(list.substr(start, end - start));
        start = end + 1;
    }
    for (const std::string& dir : dirs) {
        for (const std::string& candidate : candidates) {
            std::string full = candidate.find('/') == 0 ? candidate : dir + "/" + candidate;
            struct stat st {};
            if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                std::wstring found = wide(full);
                DWORD result = copyOut(found, buffer, size);
                if (filePart && buffer && result < size) *filePart = buffer + found.find_last_of(L'/') + 1;
                return result;
            }
        }
    }
    lastError = ERROR_FILE_NOT_FOUND;
    return 0;
}

// ---- Processes and threads ---------------------------------------------------------------------
DWORD GetCurrentProcessId() { return static_cast<DWORD>(getpid()); }

HANDLE OpenProcess(DWORD, BOOL, DWORD processId) {
    if (kill(static_cast<pid_t>(processId), 0) != 0 && errno != EPERM) {
        lastError = 87;  // ERROR_INVALID_PARAMETER, as for a gone pid
        return nullptr;
    }
    return static_cast<TintaHandle*>(new ProcessHandle(static_cast<pid_t>(processId), false));
}

BOOL QueryFullProcessImageNameW(HANDLE process, DWORD, LPWSTR name, DWORD* size) {
    auto* h = dynamic_cast<ProcessHandle*>(handleOf(process));
    if (!h || !name || !size) return FALSE;
    char link[64], target[4096];
    std::snprintf(link, sizeof link, "/proc/%d/exe", static_cast<int>(h->pid));
    ssize_t n = readlink(link, target, sizeof target - 1);
    if (n <= 0) {
        failWithErrno();
        return FALSE;
    }
    std::wstring image = wide(std::string(target, static_cast<size_t>(n)));
    if (image.size() >= *size) {
        lastError = 122;
        return FALSE;
    }
    wmemcpy(name, image.c_str(), image.size() + 1);
    *size = static_cast<DWORD>(image.size());
    return TRUE;
}

BOOL CreateProcessW(LPCWSTR application, LPWSTR commandLine, SECURITY_ATTRIBUTES*, SECURITY_ATTRIBUTES*, BOOL,
                    DWORD flags, void* environment, LPCWSTR currentDirectory, STARTUPINFOW*,
                    PROCESS_INFORMATION* info) {
    std::vector<std::wstring> args = splitCommandLine(commandLine ? commandLine : L"");
    if (application) {
        if (args.empty()) args.push_back(application);
        else args[0] = application;
    }
    if (args.empty()) {
        lastError = 87;
        return FALSE;
    }
    std::vector<std::string> utf8;
    for (const std::wstring& a : args) utf8.push_back(tinta::toUtf8(a));
    utf8[0] = tinta::nativePath(args[0].c_str());
    std::vector<char*> argv;
    for (std::string& a : utf8) argv.push_back(a.data());
    argv.push_back(nullptr);

    // An environment block: NAME=VALUE strings, each ended by a NUL, the
    // block by a second one (UTF-16 with CREATE_UNICODE_ENVIRONMENT)
    std::vector<std::string> envStrings;
    std::vector<char*> envp;
    if (environment) {
        if (flags & CREATE_UNICODE_ENVIRONMENT) {
            for (auto* p = static_cast<const wchar_t*>(environment); *p; p += wcslen(p) + 1) {
                envStrings.push_back(tinta::toUtf8(p, wcslen(p)));
            }
        } else {
            for (auto* p = static_cast<const char*>(environment); *p; p += strlen(p) + 1) envStrings.push_back(p);
        }
        for (std::string& e : envStrings) envp.push_back(e.data());
        envp.push_back(nullptr);
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    std::string dir = currentDirectory ? native(currentDirectory) : std::string();
    if (!dir.empty()) posix_spawn_file_actions_addchdir_np(&actions, dir.c_str());
    // The job pattern (create suspended, assign to a job, resume): the child
    // starts now, in a process group of its own that the job can end whole
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    if (flags & CREATE_SUSPENDED) {
        posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
        posix_spawnattr_setpgroup(&attributes, 0);
    }
    pid_t pid = 0;
    int error = posix_spawnp(&pid, argv[0], &actions, &attributes, argv.data(),
                             environment ? envp.data() : environ);
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    if (error != 0) {
        lastError = errorFromErrno(error);
        return FALSE;
    }
    if (info) {
        info->hProcess = static_cast<TintaHandle*>(new ProcessHandle(pid, true));
        info->hThread = static_cast<TintaHandle*>(new ThreadHandle());
        info->dwProcessId = static_cast<DWORD>(pid);
        info->dwThreadId = static_cast<DWORD>(pid);
    }
    return TRUE;
}

BOOL GetExitCodeProcess(HANDLE process, DWORD* exitCode) {
    auto* h = dynamic_cast<ProcessHandle*>(handleOf(process));
    if (!h || !exitCode) return FALSE;
    *exitCode = h->wait(0) ? h->exitCode : 259;  // STILL_ACTIVE
    return TRUE;
}

BOOL TerminateProcess(HANDLE process, UINT exitCode) {
    auto* h = dynamic_cast<ProcessHandle*>(handleOf(process));
    if (!h || kill(h->pid, SIGKILL) != 0) return FALSE;
    if (h->child) {
        h->wait(INFINITE);
        h->exitCode = exitCode;
    }
    return TRUE;
}

// The child already runs (CreateProcessW cannot start one suspended here)
DWORD ResumeThread(HANDLE) { return 1; }

HANDLE CreateJobObjectW(SECURITY_ATTRIBUTES*, LPCWSTR) { return static_cast<TintaHandle*>(new JobHandle()); }

BOOL SetInformationJobObject(HANDLE job, JOBOBJECTINFOCLASS infoClass, void* info, DWORD size) {
    auto* h = dynamic_cast<JobHandle*>(handleOf(job));
    if (!h || !info) return FALSE;
    if (infoClass == JobObjectExtendedLimitInformation && size >= sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION)) {
        const auto* limits = static_cast<const JOBOBJECT_EXTENDED_LIMIT_INFORMATION*>(info);
        h->killOnClose = (limits->BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE) != 0;
        return TRUE;
    }
    lastError = 87;
    return FALSE;
}

// Only a process that leads its own group (CREATE_SUSPENDED) can join: a
// job must never take its parent's group, Tinta's own, with it
BOOL AssignProcessToJobObject(HANDLE job, HANDLE process) {
    auto* h = dynamic_cast<JobHandle*>(handleOf(job));
    auto* p = dynamic_cast<ProcessHandle*>(handleOf(process));
    if (!h || !p || getpgid(p->pid) != p->pid) {
        lastError = 5;  // ERROR_ACCESS_DENIED
        return FALSE;
    }
    h->groups.push_back(p->pid);
    return TRUE;
}

BOOL TerminateJobObject(HANDLE job, UINT) {
    auto* h = dynamic_cast<JobHandle*>(handleOf(job));
    if (!h) return FALSE;
    h->killAll();
    return TRUE;
}

LPWSTR GetEnvironmentStringsW() {
    std::wstring block;
    for (char** e = environ; e && *e; ++e) {
        block += wide(*e);
        block += L'\0';
    }
    block += L'\0';
    auto* out = new wchar_t[block.size()];
    wmemcpy(out, block.data(), block.size());
    return out;
}

BOOL FreeEnvironmentStringsW(LPWSTR block) {
    delete[] block;
    return TRUE;
}

HANDLE CreateEventW(SECURITY_ATTRIBUTES*, BOOL manualReset, BOOL initialState, LPCWSTR) {
    auto* h = new EventHandle();
    h->manualReset = manualReset != FALSE;
    h->signaled = initialState != FALSE;
    return static_cast<TintaHandle*>(h);
}

BOOL SetEvent(HANDLE event) {
    auto* h = dynamic_cast<EventHandle*>(handleOf(event));
    if (!h) return FALSE;
    {
        std::lock_guard<std::mutex> lock(h->mutex);
        h->signaled = true;
    }
    h->changed.notify_all();
    return TRUE;
}

BOOL ResetEvent(HANDLE event) {
    auto* h = dynamic_cast<EventHandle*>(handleOf(event));
    if (!h) return FALSE;
    std::lock_guard<std::mutex> lock(h->mutex);
    h->signaled = false;
    return TRUE;
}

HANDLE CreateThread(SECURITY_ATTRIBUTES*, SIZE_T, LPTHREAD_START_ROUTINE start, void* parameter, DWORD,
                    DWORD* threadId) {
    auto* handle = new ThreadHandle();
    handle->state = std::make_shared<ThreadState>();
    std::shared_ptr<ThreadState> state = handle->state;
    std::thread worker([state, start, parameter] {
        DWORD code = start(parameter);
        std::lock_guard<std::mutex> lock(state->mutex);
        state->finished = true;
        state->exitCode = code;
        state->done.notify_all();
    });
    if (threadId) *threadId = static_cast<DWORD>(std::hash<std::thread::id>{}(worker.get_id()));
    worker.detach();
    return static_cast<TintaHandle*>(handle);
}

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds) {
    TintaHandle* h = handleOf(handle);
    if (auto* process = dynamic_cast<ProcessHandle*>(h)) {
        return process->wait(milliseconds) ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
    }
    if (auto* event = dynamic_cast<EventHandle*>(h)) {
        std::unique_lock<std::mutex> lock(event->mutex);
        auto set = [&] { return event->signaled; };
        if (milliseconds == INFINITE) event->changed.wait(lock, set);
        else if (!event->changed.wait_for(lock, std::chrono::milliseconds(milliseconds), set)) return WAIT_TIMEOUT;
        if (!event->manualReset) event->signaled = false;
        return WAIT_OBJECT_0;
    }
    if (auto* thread = dynamic_cast<ThreadHandle*>(h)) {
        if (!thread->state) return WAIT_OBJECT_0;
        std::unique_lock<std::mutex> lock(thread->state->mutex);
        auto finished = [&] { return thread->state->finished; };
        if (milliseconds == INFINITE) thread->state->done.wait(lock, finished);
        else if (!thread->state->done.wait_for(lock, std::chrono::milliseconds(milliseconds), finished))
            return WAIT_TIMEOUT;
        return WAIT_OBJECT_0;
    }
    return 0xFFFFFFFF;  // WAIT_FAILED
}

// Processes, threads and events, polled every 5 ms (a process has no
// descriptor to block on together with an event)
DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL waitAll, DWORD milliseconds) {
    if (count == 0 || !handles) return WAIT_FAILED;
    auto signaled = [](HANDLE handle) {
        TintaHandle* h = handleOf(handle);
        if (auto* process = dynamic_cast<ProcessHandle*>(h)) return process->wait(0);
        if (auto* event = dynamic_cast<EventHandle*>(h)) {
            std::lock_guard<std::mutex> lock(event->mutex);
            return event->signaled;
        }
        if (dynamic_cast<ThreadHandle*>(h)) return WaitForSingleObject(handle, 0) == WAIT_OBJECT_0;
        return false;
    };
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (true) {
        if (waitAll) {
            bool all = true;
            for (DWORD i = 0; i < count && all; ++i) all = signaled(handles[i]);
            if (all) {
                for (DWORD i = 0; i < count; ++i)
                    if (auto* event = dynamic_cast<EventHandle*>(handleOf(handles[i]))) event->tryTake();
                return WAIT_OBJECT_0;
            }
        } else {
            for (DWORD i = 0; i < count; ++i) {
                if (auto* event = dynamic_cast<EventHandle*>(handleOf(handles[i]))) {
                    if (event->tryTake()) return WAIT_OBJECT_0 + i;
                } else if (signaled(handles[i])) {
                    return WAIT_OBJECT_0 + i;
                }
            }
        }
        if (milliseconds != INFINITE && std::chrono::steady_clock::now() >= deadline) return WAIT_TIMEOUT;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void Sleep(DWORD milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

void OutputDebugStringW(LPCWSTR text) {
    static const bool enabled = std::getenv("TINTA_DEBUG_OUTPUT") != nullptr;
    if (enabled && text) std::fprintf(stderr, "%s", tinta::toUtf8(text, wcslen(text)).c_str());
}

LPWSTR GetCommandLineW() {
    static std::wstring line = [] {
        std::wstring out;
        FILE* f = std::fopen("/proc/self/cmdline", "rb");
        if (!f) return out;
        std::string arg;
        int c;
        while ((c = std::fgetc(f)) != EOF) {
            if (c == 0) {
                if (!out.empty()) out += L' ';
                out += quoteArgument(wide(arg));
                arg.clear();
            } else {
                arg += static_cast<char>(c);
            }
        }
        std::fclose(f);
        return out;
    }();
    return line.data();
}

// One allocation holding the pointer table and the strings; LocalFree frees it
LPWSTR* CommandLineToArgvW(LPCWSTR commandLine, int* count) {
    std::vector<std::wstring> args = splitCommandLine(commandLine ? commandLine : L"");
    size_t chars = 0;
    for (const auto& a : args) chars += a.size() + 1;
    size_t table = (args.size() + 1) * sizeof(LPWSTR);
    auto* block = static_cast<unsigned char*>(std::malloc(table + chars * sizeof(wchar_t)));
    if (!block) return nullptr;
    auto** argv = reinterpret_cast<LPWSTR*>(block);
    auto* text = reinterpret_cast<wchar_t*>(block + table);
    for (size_t i = 0; i < args.size(); i++) {
        argv[i] = text;
        wmemcpy(text, args[i].c_str(), args[i].size() + 1);
        text += args[i].size() + 1;
    }
    argv[args.size()] = nullptr;
    if (count) *count = static_cast<int>(args.size());
    return argv;
}

// ---- Locale --------------------------------------------------------------------------------------
LANGID GetUserDefaultUILanguage() {
    std::wstring tag = messagesLocale();
    std::wstring lang = tag.substr(0, tag.find(L'-'));
    if (lang == L"ja") return 0x0411;
    if (lang == L"ko") return 0x0412;
    if (lang == L"zh") return tag.find(L"TW") != std::wstring::npos || tag.find(L"HK") != std::wstring::npos
                                  ? 0x0404
                                  : 0x0804;
    return 0x0409;
}

// LANGUAGE (a colon list, GNU gettext order) first, then the messages locale
BOOL GetUserPreferredUILanguages(DWORD, ULONG* count, LPWSTR buffer, ULONG* size) {
    std::vector<std::wstring> tags;
    if (const char* list = std::getenv("LANGUAGE")) {
        std::string s = list;
        for (size_t start = 0; start <= s.size();) {
            size_t end = s.find(':', start);
            if (end == std::string::npos) end = s.size();
            std::wstring tag = localeTag(s.substr(start, end - start).c_str());
            if (!tag.empty() && std::find(tags.begin(), tags.end(), tag) == tags.end()) tags.push_back(tag);
            start = end + 1;
        }
    }
    std::wstring primary = messagesLocale();
    if (std::find(tags.begin(), tags.end(), primary) == tags.end()) tags.push_back(primary);
    std::wstring multi;
    for (const auto& t : tags) multi += t + L'\0';
    multi += L'\0';
    if (!size) return FALSE;
    if (count) *count = static_cast<ULONG>(tags.size());
    if (buffer) {
        if (*size < multi.size()) {
            *size = static_cast<ULONG>(multi.size());
            lastError = 122;
            return FALSE;
        }
        wmemcpy(buffer, multi.data(), multi.size());
    }
    *size = static_cast<ULONG>(multi.size());
    return TRUE;
}

int GetUserDefaultLocaleName(LPWSTR name, int size) {
    std::wstring tag = messagesLocale();
    if (!name || size <= static_cast<int>(tag.size())) return 0;
    wmemcpy(name, tag.c_str(), tag.size() + 1);
    return static_cast<int>(tag.size() + 1);
}

// Only LOCALE_IMEASURE is asked for: "1" for US customary (Letter, inches)
int GetLocaleInfoEx(LPCWSTR, DWORD type, LPWSTR data, int size) {
    if ((type & 0xFFFF) != LOCALE_IMEASURE) return 0;
    std::wstring tag = measurementLocale();
    std::wstring region = tag.find(L'-') == std::wstring::npos ? L"" : tag.substr(tag.find(L'-') + 1);
    bool us = region == L"US" || region == L"LR" || region == L"MM";
    if (!data || size < 2) return 2;
    data[0] = us ? L'1' : L'0';
    data[1] = 0;
    return 2;
}

// Date pictures d, dd, ddd, dddd, M, MM, MMM, MMMM, y, yy, yyyy and 'quoted'
// text; names come from the user's LC_TIME locale
int GetDateFormatEx(LPCWSTR, DWORD, const SYSTEMTIME* date, LPCWSTR format, LPWSTR buffer, int size, LPCWSTR) {
    if (!date) return 0;
    static locale_t timeLocale = newlocale(LC_TIME_MASK, "", nullptr);
    locale_t loc = timeLocale ? timeLocale : newlocale(LC_TIME_MASK, "C", nullptr);
    auto name = [&](nl_item item) { return wide(nl_langinfo_l(item, loc)); };
    std::wstring f = format ? format : L"yyyy-MM-dd", out;
    for (size_t i = 0; i < f.size();) {
        wchar_t c = f[i];
        if (c == L'\'') {
            size_t end = f.find(L'\'', i + 1);
            out += f.substr(i + 1, (end == std::wstring::npos ? f.size() : end) - i - 1);
            i = end == std::wstring::npos ? f.size() : end + 1;
            continue;
        }
        size_t run = 1;
        while (i + run < f.size() && f[i + run] == c) run++;
        wchar_t num[16];
        if (c == L'd') {
            if (run == 1) swprintf(num, 16, L"%d", date->wDay), out += num;
            else if (run == 2) swprintf(num, 16, L"%02d", date->wDay), out += num;
            else out += name((run == 3 ? ABDAY_1 : DAY_1) + date->wDayOfWeek % 7);
        } else if (c == L'M') {
            if (run == 1) swprintf(num, 16, L"%d", date->wMonth), out += num;
            else if (run == 2) swprintf(num, 16, L"%02d", date->wMonth), out += num;
            else out += name((run == 3 ? ABMON_1 : MON_1) + (date->wMonth + 11) % 12);
        } else if (c == L'y') {
            if (run <= 2) swprintf(num, 16, run == 1 ? L"%d" : L"%02d", date->wYear % 100), out += num;
            else swprintf(num, 16, L"%d", date->wYear), out += num;
        } else {
            out.append(run, c);
        }
        i += run;
    }
    int needed = static_cast<int>(out.size() + 1);
    if (!buffer || size == 0) return needed;
    if (size < needed) {
        lastError = 122;
        return 0;
    }
    wmemcpy(buffer, out.c_str(), out.size() + 1);
    return needed;
}

// ---- Shell ------------------------------------------------------------------------------------------
HRESULT SHGetFolderPathW(HWND, int csidl, HANDLE, DWORD, LPWSTR path) {
    std::wstring dir;
    switch (csidl & 0xFF) {
        case CSIDL_APPDATA: dir = xdgDir("XDG_CONFIG_HOME", L"/.config"); break;
        case CSIDL_LOCAL_APPDATA: dir = xdgDir("XDG_DATA_HOME", L"/.local/share"); break;
        default: return E_INVALIDARG;
    }
    if (!path || dir.size() >= MAX_PATH) return E_FAIL;
    wmemcpy(path, dir.c_str(), dir.size() + 1);
    return S_OK;
}

namespace {

// Opens `target` (a URL or a path) with the desktop's handler; 0 on
// success, else a Win32 error
DWORD openWithDefaultHandler(const std::string& target) {
    std::string uri;
    if (target.find("://") != std::string::npos || target.rfind("mailto:", 0) == 0) {
        uri = target;
    } else {
        if (access(target.c_str(), F_OK) != 0) return ERROR_FILE_NOT_FOUND;
        GFile* file = g_file_new_for_path(target.c_str());
        char* u = g_file_get_uri(file);
        uri = u;
        g_free(u);
        g_object_unref(file);
    }
    GError* error = nullptr;
    gboolean ok = g_app_info_launch_default_for_uri(uri.c_str(), nullptr, &error);
    if (error) g_error_free(error);
    return ok ? ERROR_SUCCESS : ERROR_NO_ASSOCIATION;
}

// Shows `path` selected in the file manager (FileManager1 over D-Bus);
// without one, opens the containing folder
DWORD revealInFileManager(const std::string& path) {
    GError* error = nullptr;
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (bus) {
        GFile* file = g_file_new_for_path(path.c_str());
        char* uri = g_file_get_uri(file);
        const char* uris[] = {uri, nullptr};
        GVariant* reply = g_dbus_connection_call_sync(
            bus, "org.freedesktop.FileManager1", "/org/freedesktop/FileManager1", "org.freedesktop.FileManager1",
            "ShowItems", g_variant_new("(^ass)", uris, ""), nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
        g_free(uri);
        g_object_unref(file);
        g_object_unref(bus);
        if (reply) {
            g_variant_unref(reply);
            return ERROR_SUCCESS;
        }
    }
    if (error) g_error_free(error);
    std::string dir = path.substr(0, path.find_last_of('/'));
    return openWithDefaultHandler(dir.empty() ? "/" : dir);
}

DWORD shellExecute(LPCWSTR operation, LPCWSTR file, LPCWSTR parameters, LPCWSTR directory) {
    if (!file) return ERROR_FILE_NOT_FOUND;
    std::wstring verb = operation ? operation : L"open";
    if (verb != L"open" && verb != L"explore") return ERROR_NO_ASSOCIATION;
    std::wstring name = file;
    std::wstring params = parameters ? parameters : L"";

    // explorer.exe /select,"<path>" reveals a file; explorer.exe <dir> opens one
    if (name == L"explorer.exe") {
        std::vector<std::wstring> args = splitCommandLine(params);
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i].rfind(L"/select,", 0) == 0) {
                std::wstring target = args[i].substr(8);
                if (target.empty() && i + 1 < args.size()) target = args[i + 1];
                return revealInFileManager(tinta::nativePath(target.c_str()));
            }
        }
        return args.empty() ? ERROR_FILE_NOT_FOUND : openWithDefaultHandler(tinta::nativePath(args[0].c_str()));
    }

    std::string path = tinta::nativePath(name.c_str());
    // Programs run only when the target is Tinta itself (new windows,
    // relaunch). Anything else opens with its handler: on NTFS and exFAT
    // mounts every file has the execute bit, and opening a document must
    // never run it.
    if (sameFile(path, selfExe())) {
        std::vector<std::string> argv{path};
        for (const std::wstring& a : splitCommandLine(params)) argv.push_back(tinta::toUtf8(a));
        return spawnDetached(argv, directory ? native(directory) : std::string()) ? ERROR_SUCCESS
                                                                                 : ERROR_FILE_NOT_FOUND;
    }
    return openWithDefaultHandler(path);
}

HINSTANCE shellResult(DWORD error) {
    if (error == ERROR_SUCCESS) return reinterpret_cast<HINSTANCE>(static_cast<intptr_t>(42));
    lastError = error;
    intptr_t code = error == ERROR_NO_ASSOCIATION ? 31 : static_cast<intptr_t>(error);  // SE_ERR_NOASSOC
    return reinterpret_cast<HINSTANCE>(code);
}

}  // namespace

HINSTANCE ShellExecuteW(HWND, LPCWSTR operation, LPCWSTR file, LPCWSTR parameters, LPCWSTR directory, int) {
    return shellResult(shellExecute(operation, file, parameters, directory));
}

HINSTANCE ShellExecuteA(HWND, LPCSTR operation, LPCSTR file, LPCSTR parameters, LPCSTR directory, int) {
    auto w = [](LPCSTR s) { return s ? tinta::toWide(s, std::strlen(s)) : std::wstring(); };
    std::wstring op = w(operation), f = w(file), p = w(parameters), d = w(directory);
    return shellResult(shellExecute(operation ? op.c_str() : nullptr, file ? f.c_str() : nullptr,
                                    parameters ? p.c_str() : nullptr, directory ? d.c_str() : nullptr));
}

BOOL ShellExecuteExW(SHELLEXECUTEINFOW* info) {
    if (!info) return FALSE;
    DWORD error = shellExecute(info->lpVerb, info->lpFile, info->lpParameters, info->lpDirectory);
    info->hInstApp = shellResult(error);
    info->hProcess = nullptr;
    return error == ERROR_SUCCESS;
}

void SHChangeNotify(LONG, UINT, const void*, const void*) {}

// ---- Registry: nothing is stored there on Linux ----------------------------------------------------
namespace tinta {

// The desktop's light/dark preference (org.freedesktop.appearance
// color-scheme, through the XDG desktop portal: KDE and GNOME both serve
// it): 0 no preference, 1 dark, 2 light; -1 without a portal
int portalColorScheme() {
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!bus) return -1;
    int scheme = -1;
    GVariant* reply = g_dbus_connection_call_sync(
        bus, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Settings",
        "ReadOne", g_variant_new("(ss)", "org.freedesktop.appearance", "color-scheme"), G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NO_AUTO_START, 1000, nullptr, nullptr);
    if (reply) {
        GVariant* value = nullptr;
        g_variant_get(reply, "(v)", &value);
        if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32)) scheme = static_cast<int>(g_variant_get_uint32(value));
        g_variant_unref(value);
        g_variant_unref(reply);
    }
    g_object_unref(bus);
    return scheme;
}

}  // namespace tinta

LONG RegOpenKeyExW(HKEY, LPCWSTR, DWORD, REGSAM, HKEY* result) {
    if (result) *result = nullptr;
    return ERROR_FILE_NOT_FOUND;
}

LONG RegCreateKeyExW(HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM, SECURITY_ATTRIBUTES*, HKEY* result, DWORD*) {
    if (result) *result = nullptr;
    return ERROR_ACCESS_DENIED;
}

LONG RegCloseKey(HKEY) { return ERROR_SUCCESS; }
LONG RegSetValueExW(HKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD) { return ERROR_ACCESS_DENIED; }
LONG RegQueryValueExW(HKEY, LPCWSTR, DWORD*, DWORD*, BYTE*, DWORD*) { return ERROR_FILE_NOT_FOUND; }
// Windows' "apps use light theme" answers from the desktop's color scheme
// (dark is 0; light and no preference are 1); nothing else is stored
LONG RegGetValueW(HKEY, LPCWSTR subKey, LPCWSTR valueName, DWORD, DWORD* type, void* data, DWORD* size) {
    if (!subKey || !valueName || wcscmp(valueName, L"AppsUseLightTheme") != 0 ||
        !wcsstr(subKey, L"Themes\\Personalize")) {
        return ERROR_FILE_NOT_FOUND;
    }
    int scheme = tinta::portalColorScheme();
    if (scheme < 0) return ERROR_FILE_NOT_FOUND;
    if (type) *type = REG_DWORD;
    if (size && *size < sizeof(DWORD)) {
        *size = sizeof(DWORD);
        return ERROR_MORE_DATA;
    }
    if (size) *size = sizeof(DWORD);
    if (data) *static_cast<DWORD*>(data) = scheme == 1 ? 0 : 1;
    return ERROR_SUCCESS;
}
LONG RegDeleteTreeW(HKEY, LPCWSTR) { return ERROR_FILE_NOT_FOUND; }

LONG GetCurrentPackageFullName(UINT32* length, LPWSTR) {
    if (length) *length = 0;
    return APPMODEL_ERROR_NO_PACKAGE;
}

// ---- Downloads ----------------------------------------------------------------------------------------
// Remote images: fetched once into $XDG_CACHE_HOME/tinta/remote, named by a
// hash of the URL
HRESULT URLDownloadToCacheFileW(IUnknown*, LPCWSTR url, LPWSTR fileName, DWORD bufferSize, DWORD, void*) {
#ifdef TINTA_HAVE_CURL
    if (!url || !fileName || bufferSize == 0) return E_INVALIDARG;
    std::string u = tinta::toUtf8(url, wcslen(url));
    std::wstring cacheRoot = xdgDir("XDG_CACHE_HOME", L"/.cache");
    std::string dir = tinta::toUtf8(cacheRoot) + "/tinta/remote";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    char hash[32];
    std::snprintf(hash, sizeof hash, "%016zx", std::hash<std::string>{}(u));
    std::string ext;
    size_t dot = u.find_last_of('.'), slash = u.find_last_of('/');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        ext = u.substr(dot, std::min<size_t>(6, u.find_first_of("?#", dot) - dot));
    }
    std::string path = dir + "/" + hash + ext;
    struct stat st {};
    if (stat(path.c_str(), &st) != 0 || st.st_size == 0) {
        std::string partial = path + ".part";
        FILE* out = std::fopen(partial.c_str(), "wb");
        if (!out) return E_FAIL;
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::fclose(out);
            return E_FAIL;
        }
        curl_easy_setopt(curl, CURLOPT_URL, u.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Tinta");
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
        CURLcode code = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        std::fclose(out);
        if (code != CURLE_OK || rename(partial.c_str(), path.c_str()) != 0) {
            unlink(partial.c_str());
            return static_cast<HRESULT>(0x800C0008);  // INET_E_DOWNLOAD_FAILURE
        }
    }
    std::wstring w = wide(path);
    if (w.size() >= bufferSize) return E_OUTOFMEMORY;
    wmemcpy(fileName, w.c_str(), w.size() + 1);
    return S_OK;
#else
    (void)url;
    (void)fileName;
    (void)bufferSize;
    return static_cast<HRESULT>(0x800C0008);  // INET_E_DOWNLOAD_FAILURE: built without libcurl
#endif
}

// ---- Entry-point services that do not exist here (tinta_main.h) --------------------------------
// No GPU warm-up: Cairo renders on the CPU
HRESULT D3D11CreateDevice(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT,
                          ID3D11Device** device, D3D_FEATURE_LEVEL*, ID3D11DeviceContext** context) {
    if (device) *device = nullptr;
    if (context) *context = nullptr;
    return E_NOTIMPL;
}

// The update check (main_d2d.cpp) asks this fork's GitHub releases, which
// carry the AppImage too. WinHTTP over libcurl: the request is one HTTPS GET
// at WinHttpSendRequest, read back from memory. Without libcurl every check
// fails, and fails quietly unless the reader asked for it.
namespace {
struct WinHttpHandle {
    std::string agent;
    std::string host;
    std::string path;
    std::string body;
    size_t readAt = 0;
    bool received = false;
};
WinHttpHandle* httpHandle(HINTERNET h) { return static_cast<WinHttpHandle*>(h); }
#ifdef TINTA_HAVE_CURL
size_t appendBody(char* data, size_t size, size_t count, void* user) {
    static_cast<std::string*>(user)->append(data, size * count);
    return size * count;
}
#endif
}  // namespace

HINTERNET WinHttpOpen(LPCWSTR agent, DWORD, LPCWSTR, LPCWSTR, DWORD) {
    auto* session = new WinHttpHandle;
    if (agent) session->agent = tinta::toUtf8(agent, wcslen(agent));
    return session;
}
HINTERNET WinHttpConnect(HINTERNET session, LPCWSTR server, INTERNET_PORT, DWORD) {
    if (!session || !server) return nullptr;
    auto* connect = new WinHttpHandle;
    connect->agent = httpHandle(session)->agent;
    connect->host = tinta::toUtf8(server, wcslen(server));
    return connect;
}
// HTTPS GET only: the one request Tinta makes
HINTERNET WinHttpOpenRequest(HINTERNET connect, LPCWSTR verb, LPCWSTR object, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD) {
    if (!connect || !object || (verb && wcscmp(verb, L"GET") != 0)) return nullptr;
    auto* request = new WinHttpHandle;
    request->agent = httpHandle(connect)->agent;
    request->host = httpHandle(connect)->host;
    request->path = tinta::toUtf8(object, wcslen(object));
    return request;
}
BOOL WinHttpSendRequest(HINTERNET request, LPCWSTR, DWORD, void*, DWORD, DWORD, DWORD_PTR) {
    if (!request) return FALSE;
#ifdef TINTA_HAVE_CURL
    WinHttpHandle* r = httpHandle(request);
    CURL* curl = curl_easy_init();
    if (!curl) return FALSE;
    std::string url = "https://" + r->host + r->path;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r->body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, r->agent.empty() ? "Tinta" : r->agent.c_str());
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    CURLcode code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    r->received = code == CURLE_OK;
    if (!r->received) r->body.clear();
    return r->received ? TRUE : FALSE;
#else
    return FALSE;
#endif
}
BOOL WinHttpReceiveResponse(HINTERNET request, void*) {
    return request && httpHandle(request)->received ? TRUE : FALSE;
}
BOOL WinHttpQueryDataAvailable(HINTERNET request, DWORD* available) {
    if (!request || !available) return FALSE;
    WinHttpHandle* r = httpHandle(request);
    *available = static_cast<DWORD>(r->body.size() - r->readAt);
    return TRUE;
}
BOOL WinHttpReadData(HINTERNET request, void* buffer, DWORD size, DWORD* read) {
    if (!request || !buffer || !read) return FALSE;
    WinHttpHandle* r = httpHandle(request);
    size_t n = std::min<size_t>(size, r->body.size() - r->readAt);
    std::memcpy(buffer, r->body.data() + r->readAt, n);
    r->readAt += n;
    *read = static_cast<DWORD>(n);
    return TRUE;
}
BOOL WinHttpCloseHandle(HINTERNET handle) {
    delete httpHandle(handle);
    return TRUE;
}

// Crash reports on Linux come from the system's handler (core dumps)
LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER) { return nullptr; }
BOOL MiniDumpWriteDump(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, MINIDUMP_EXCEPTION_INFORMATION*, void*, void*) {
    return FALSE;
}
HANDLE GetCurrentProcess() { return reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)); }
DWORD GetCurrentThreadId() { return static_cast<DWORD>(syscall(SYS_gettid)); }
