// COM activation, IStream over files and memory, and HGLOBAL memory.
#include "compat_internal.h"

#include <shlobj.h>
#include <wincodec.h>

#include <cstdio>
#include <cstring>
#include <mutex>

// HGLOBAL: a growable block; GlobalLock hands out its bytes
struct TintaGlobal {
    std::vector<unsigned char> bytes;
    int locks = 0;
};

HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes) {
    auto* block = new TintaGlobal();
    block->bytes.assign(bytes, 0);
    (void)flags;
    return block;
}

void* GlobalLock(HGLOBAL memory) {
    auto* block = static_cast<TintaGlobal*>(memory);
    if (!block) return nullptr;
    block->locks++;
    return block->bytes.empty() ? nullptr : block->bytes.data();
}

BOOL GlobalUnlock(HGLOBAL memory) {
    auto* block = static_cast<TintaGlobal*>(memory);
    if (!block || block->locks == 0) return FALSE;
    return --block->locks > 0;
}

HGLOBAL GlobalFree(HGLOBAL memory) {
    delete static_cast<TintaGlobal*>(memory);
    return nullptr;
}

SIZE_T GlobalSize(HGLOBAL memory) {
    auto* block = static_cast<TintaGlobal*>(memory);
    return block ? block->bytes.size() : 0;
}

HLOCAL LocalFree(HLOCAL memory) {
    std::free(memory);
    return nullptr;
}

namespace tinta {
namespace {

class FileStream final : public TintaComObject<IStream> {
public:
    using Primary = IStream;
    explicit FileStream(FILE* file) : file_(file) {}
    ~FileStream() override {
        if (file_) std::fclose(file_);
    }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IStream, ISequentialStream>(this, riid, object);
    }
    HRESULT Read(void* buffer, ULONG size, ULONG* read) override {
        size_t n = std::fread(buffer, 1, size, file_);
        if (read) *read = static_cast<ULONG>(n);
        return n == size ? S_OK : S_FALSE;
    }
    HRESULT Write(const void* buffer, ULONG size, ULONG* written) override {
        size_t n = std::fwrite(buffer, 1, size, file_);
        if (written) *written = static_cast<ULONG>(n);
        return n == size ? S_OK : E_FAIL;
    }
    HRESULT Seek(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* position) override {
        int whence = origin == STREAM_SEEK_CUR ? SEEK_CUR : origin == STREAM_SEEK_END ? SEEK_END : SEEK_SET;
        if (fseeko(file_, static_cast<off_t>(move.QuadPart), whence) != 0) return E_FAIL;
        if (position) position->QuadPart = static_cast<ULONGLONG>(ftello(file_));
        return S_OK;
    }
    HRESULT SetSize(ULARGE_INTEGER) override { return S_OK; }
    HRESULT Commit(DWORD) override { return std::fflush(file_) == 0 ? S_OK : E_FAIL; }

private:
    FILE* file_;
};

class MemoryStream final : public TintaComObject<IStream> {
public:
    using Primary = IStream;
    MemoryStream(TintaGlobal* block, bool owned) : block_(block), owned_(owned) {}
    ~MemoryStream() override {
        if (owned_) delete block_;
    }
    HRESULT QueryInterface(REFIID riid, void** object) override {
        return tinta_query<IStream, ISequentialStream>(this, riid, object);
    }
    HRESULT Read(void* buffer, ULONG size, ULONG* read) override {
        size_t available = block_->bytes.size() > position_ ? block_->bytes.size() - position_ : 0;
        size_t n = std::min<size_t>(size, available);
        if (n) std::memcpy(buffer, block_->bytes.data() + position_, n);
        position_ += n;
        if (read) *read = static_cast<ULONG>(n);
        return n == size ? S_OK : S_FALSE;
    }
    HRESULT Write(const void* buffer, ULONG size, ULONG* written) override {
        if (position_ + size > block_->bytes.size()) block_->bytes.resize(position_ + size);
        std::memcpy(block_->bytes.data() + position_, buffer, size);
        position_ += size;
        if (written) *written = size;
        return S_OK;
    }
    HRESULT Seek(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* position) override {
        long long base = origin == STREAM_SEEK_CUR ? static_cast<long long>(position_)
                         : origin == STREAM_SEEK_END ? static_cast<long long>(block_->bytes.size())
                                                     : 0;
        long long target = base + move.QuadPart;
        if (target < 0) return E_INVALIDARG;
        position_ = static_cast<size_t>(target);
        if (position) position->QuadPart = position_;
        return S_OK;
    }
    HRESULT SetSize(ULARGE_INTEGER size) override {
        block_->bytes.resize(static_cast<size_t>(size.QuadPart));
        return S_OK;
    }
    HRESULT Commit(DWORD) override { return S_OK; }
    TintaGlobal* block() const { return block_; }

private:
    TintaGlobal* block_;
    bool owned_;
    size_t position_ = 0;
};

}  // namespace

IStream* createFileStream(const std::string& path, bool write, bool create) {
    FILE* file = std::fopen(path.c_str(), write ? (create ? "wb" : "r+b") : "rb");
    return file ? new FileStream(file) : nullptr;
}

IStream* createMemoryStream(const void* data, size_t size) {
    auto* block = new TintaGlobal();
    if (data && size) block->bytes.assign(static_cast<const unsigned char*>(data), static_cast<const unsigned char*>(data) + size);
    return new MemoryStream(block, true);
}

bool memoryStreamBytes(IStream* stream, std::vector<unsigned char>& out) {
    auto* memory = dynamic_cast<MemoryStream*>(stream);
    if (!memory) return false;
    out = memory->block()->bytes;
    return true;
}

}  // namespace tinta

HRESULT SHCreateStreamOnFileEx(LPCWSTR file, DWORD mode, DWORD, BOOL create, IStream*, IStream** stream) {
    if (!file || !stream) return E_INVALIDARG;
    bool write = (mode & (STGM_WRITE | STGM_READWRITE)) != 0;
    *stream = tinta::createFileStream(tinta::nativePath(file), write, create || (mode & STGM_CREATE));
    return *stream ? S_OK : HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
}

IStream* SHCreateMemStream(const BYTE* init, UINT size) { return tinta::createMemoryStream(init, size); }

HRESULT CreateStreamOnHGlobal(HGLOBAL global, BOOL deleteOnRelease, IStream** stream) {
    if (!stream) return E_POINTER;
    auto* block = static_cast<TintaGlobal*>(global);
    bool owned = deleteOnRelease || !block;
    if (!block) block = new TintaGlobal();
    *stream = new tinta::MemoryStream(block, owned);
    return S_OK;
}

HRESULT GetHGlobalFromStream(IStream* stream, HGLOBAL* global) {
    auto* memory = dynamic_cast<tinta::MemoryStream*>(stream);
    if (!memory || !global) return E_INVALIDARG;
    *global = memory->block();
    return S_OK;
}

HRESULT CoInitializeEx(void*, DWORD) { return S_OK; }
void CoUninitialize() {}

HRESULT CoCreateInstance(REFCLSID clsid, IUnknown*, DWORD, REFIID riid, void** object) {
    if (!object) return E_POINTER;
    *object = nullptr;
    if (clsid == CLSID_WICImagingFactory) return tinta::createWicFactory(riid, object);
    if (clsid == __uuidof(PrintDocumentPackageTargetFactory)) return tinta::createPrintTargetFactory(riid, object);
    return static_cast<HRESULT>(0x80040154);  // REGDB_E_CLASSNOTREG
}
