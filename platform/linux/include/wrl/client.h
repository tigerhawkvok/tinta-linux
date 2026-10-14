// Microsoft::WRL::ComPtr for the Linux compat layer: the subset Tinta uses.
#ifndef TINTA_LINUX_WRL_CLIENT_H
#define TINTA_LINUX_WRL_CLIENT_H

#include "../tinta_com.h"

#include <cstddef>
#include <utility>

namespace Microsoft {
namespace WRL {

template <class T>
class ComPtr {
public:
    typedef T InterfaceType;

    ComPtr() = default;
    ComPtr(std::nullptr_t) {}
    ComPtr(T* p) : ptr_(p) {
        if (ptr_) ptr_->AddRef();
    }
    ComPtr(const ComPtr& other) : ptr_(other.ptr_) {
        if (ptr_) ptr_->AddRef();
    }
    template <class U>
    ComPtr(const ComPtr<U>& other) : ptr_(other.Get()) {
        if (ptr_) ptr_->AddRef();
    }
    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    ~ComPtr() { InternalRelease(); }

    ComPtr& operator=(const ComPtr& other) {
        ComPtr(other).Swap(*this);
        return *this;
    }
    ComPtr& operator=(ComPtr&& other) noexcept {
        ComPtr(std::move(other)).Swap(*this);
        return *this;
    }
    ComPtr& operator=(T* p) {
        ComPtr(p).Swap(*this);
        return *this;
    }
    ComPtr& operator=(std::nullptr_t) {
        InternalRelease();
        return *this;
    }

    T* Get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }
    T* const* GetAddressOf() const { return &ptr_; }
    T** GetAddressOf() { return &ptr_; }
    T** ReleaseAndGetAddressOf() {
        InternalRelease();
        return &ptr_;
    }
    T** operator&() { return ReleaseAndGetAddressOf(); }
    T* Detach() {
        T* p = ptr_;
        ptr_ = nullptr;
        return p;
    }
    void Attach(T* p) {
        InternalRelease();
        ptr_ = p;
    }
    unsigned long Reset() {
        T* p = ptr_;
        ptr_ = nullptr;
        return p ? p->Release() : 0;
    }
    void Swap(ComPtr& other) { std::swap(ptr_, other.ptr_); }
    HRESULT CopyTo(T** out) const {
        if (ptr_) ptr_->AddRef();
        *out = ptr_;
        return S_OK;
    }
    template <class U>
    HRESULT As(ComPtr<U>* out) const {
        if (!ptr_) return E_POINTER;
        return ptr_->QueryInterface(__uuidof(U), reinterpret_cast<void**>(out->ReleaseAndGetAddressOf()));
    }
    template <class U>
    HRESULT As(U** out) const {
        if (!ptr_) return E_POINTER;
        return ptr_->QueryInterface(__uuidof(U), reinterpret_cast<void**>(out));
    }

private:
    void InternalRelease() {
        T* p = ptr_;
        ptr_ = nullptr;
        if (p) p->Release();
    }

    T* ptr_ = nullptr;
};

template <class T, class U>
bool operator==(const ComPtr<T>& a, const ComPtr<U>& b) { return a.Get() == b.Get(); }
template <class T>
bool operator==(const ComPtr<T>& a, std::nullptr_t) { return a.Get() == nullptr; }
template <class T>
bool operator!=(const ComPtr<T>& a, std::nullptr_t) { return a.Get() != nullptr; }

}  // namespace WRL
}  // namespace Microsoft

#endif  // TINTA_LINUX_WRL_CLIENT_H
