// COM essentials for the Linux compat layer: GUIDs, IUnknown, refcounting,
// IStream. Interfaces keep their Windows names and method signatures so
// shared code compiles unchanged; calling conventions vanish.
#ifndef TINTA_LINUX_COM_H
#define TINTA_LINUX_COM_H

#include <atomic>
#include <cstdint>
#include <cstring>

#include "windows.h"

#define STDMETHODCALLTYPE
#define STDMETHODIMP HRESULT
#define STDMETHODIMP_(type) type
#define COM_DECLSPEC_NOTHROW
#define DECLSPEC_UUID(x)
#define DECLSPEC_NOVTABLE
#define MIDL_INTERFACE(x) struct
#define interface struct

struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
};
typedef GUID IID;
typedef GUID CLSID;
typedef const GUID& REFGUID;
typedef const IID& REFIID;
typedef const CLSID& REFCLSID;

inline bool operator==(const GUID& a, const GUID& b) { return std::memcmp(&a, &b, sizeof(GUID)) == 0; }
inline bool operator!=(const GUID& a, const GUID& b) { return !(a == b); }
inline bool IsEqualGUID(REFGUID a, REFGUID b) { return a == b; }
#define IsEqualIID(a, b) IsEqualGUID(a, b)

// __uuidof(Type): one GUID per interface or class, declared next to it with
// TINTA_UUID. Only identity matters on Linux: nothing crosses a process.
template <class T>
const GUID& tinta_uuidof();

#define TINTA_UUID(type, id)                                                      \
    struct type;                                                                  \
    template <>                                                                   \
    inline const GUID& tinta_uuidof<type>() {                                     \
        static const GUID guid = {id, 0x7173, 0x7461, {'t', 'i', 'n', 't', 'a', 0, 0, 0}}; \
        return guid;                                                              \
    }

#define __uuidof(type) tinta_uuidof<type>()

template <class T>
void** IID_PPV_ARGS_Helper(T** pp) {
    return reinterpret_cast<void**>(pp);
}
template <class T>
const GUID& tinta_uuidof_target(T**) {
    return tinta_uuidof<T>();
}
#define IID_PPV_ARGS(pp) tinta_uuidof_target(pp), IID_PPV_ARGS_Helper(pp)

#define E_NOINTERFACE ((HRESULT)0x80004002)
#define E_NOTIMPL ((HRESULT)0x80004001)
#define E_POINTER ((HRESULT)0x80004003)
#define E_UNEXPECTED ((HRESULT)0x8000FFFF)
#define E_ACCESSDENIED ((HRESULT)0x80070005)
#define E_HANDLE ((HRESULT)0x80070006)
#define E_ABORT ((HRESULT)0x80004004)
#define HRESULT_FROM_WIN32(x) \
    ((HRESULT)(x) <= 0 ? (HRESULT)(x) : (HRESULT)(((x) & 0x0000FFFF) | 0x80070000))

TINTA_UUID(IUnknown, 0x00000000)
struct IUnknown {
    virtual HRESULT QueryInterface(REFIID riid, void** object) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;

    template <class Q>
    HRESULT QueryInterface(Q** object) {
        return QueryInterface(__uuidof(Q), reinterpret_cast<void**>(object));
    }

protected:
    virtual ~IUnknown() = default;
};

// ---- Streams ----------------------------------------------------------------
typedef struct tagSTATSTG STATSTG;
enum STREAM_SEEK { STREAM_SEEK_SET = 0, STREAM_SEEK_CUR = 1, STREAM_SEEK_END = 2 };
union LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };
    LONGLONG QuadPart;
};
union ULARGE_INTEGER {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    };
    ULONGLONG QuadPart;
};
#define STGC_DEFAULT 0
#define STGM_READ 0x00000000L
#define STGM_WRITE 0x00000001L
#define STGM_READWRITE 0x00000002L
#define STGM_SHARE_EXCLUSIVE 0x00000010L
#define STGM_SHARE_DENY_WRITE 0x00000020L
#define STGM_CREATE 0x00001000L

TINTA_UUID(ISequentialStream, 0x00000001)
struct ISequentialStream : IUnknown {
    virtual HRESULT Read(void* buffer, ULONG size, ULONG* read) = 0;
    virtual HRESULT Write(const void* buffer, ULONG size, ULONG* written) = 0;
};

TINTA_UUID(IStream, 0x00000002)
struct IStream : ISequentialStream {
    virtual HRESULT Seek(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* position) = 0;
    virtual HRESULT SetSize(ULARGE_INTEGER size) = 0;
    virtual HRESULT Commit(DWORD flags) = 0;
};

// ---- Implementation helper ----------------------------------------------------
// Reference counting for compat objects; each class answers QueryInterface
// itself through tinta_query.
template <class Base>
struct TintaComObject : Base {
    ULONG AddRef() override { return ++refs_; }
    ULONG Release() override {
        ULONG left = --refs_;
        if (left == 0) delete this;
        return left;
    }

protected:
    std::atomic<ULONG> refs_{1};
};

// Answers QueryInterface for `self` when `riid` names one of `Interfaces`
template <class... Interfaces, class Self>
HRESULT tinta_query(Self* self, REFIID riid, void** object) {
    if (!object) return E_POINTER;
    *object = nullptr;
    if (riid == __uuidof(IUnknown)) {
        *object = static_cast<IUnknown*>(static_cast<typename Self::Primary*>(self));
    } else {
        bool found = ((riid == __uuidof(Interfaces) &&
                       (*object = static_cast<Interfaces*>(self), true)) ||
                      ...);
        if (!found) return E_NOINTERFACE;
    }
    self->AddRef();
    return S_OK;
}

HRESULT CoInitializeEx(void* reserved, DWORD coInit);
void CoUninitialize();
#define COINIT_APARTMENTTHREADED 0x2
#define COINIT_MULTITHREADED 0x0
#define COINIT_DISABLE_OLE1DDE 0x4
#define CLSCTX_INPROC_SERVER 0x1
// Creates compat objects by class id (the WIC imaging factory)
HRESULT CoCreateInstance(REFCLSID clsid, IUnknown* outer, DWORD context, REFIID riid, void** object);

#endif  // TINTA_LINUX_COM_H
