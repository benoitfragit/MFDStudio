#pragma once
#include "StreamProtocol.h"
#include <string>
#if defined(_WIN32)
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace mfd::stream::detail
{
using Microsoft::WRL::ComPtr;
/** @brief Owns one native handle; legacy DXGI share handles are deliberately not stored here. */
class NativeHandle
{
public:
    NativeHandle() = default;
    explicit NativeHandle(HANDLE value) noexcept : value_(value) {}
    ~NativeHandle() { Reset(); }
    NativeHandle(const NativeHandle&) = delete;
    NativeHandle& operator=(const NativeHandle&) = delete;
    HANDLE Get() const noexcept { return value_; }
    void Reset(HANDLE value = nullptr) noexcept
    { if (value_) CloseHandle(value_); value_ = value; }
private:
    HANDLE value_ = nullptr;
};
/** @brief Zero-timeout CPU metadata lock. Abandonment is a protocol failure, never trusted state. */
class TryLock
{
public:
    explicit TryLock(HANDLE mutex) noexcept;
    ~TryLock();
    TryLock(const TryLock&) = delete;
    TryLock& operator=(const TryLock&) = delete;
    bool Acquired() const noexcept { return result_ == WAIT_OBJECT_0; }
    bool Abandoned() const noexcept { return result_ == WAIT_ABANDONED; }
private:
    HANDLE mutex_ = nullptr;
    DWORD result_ = WAIT_FAILED;
};
/** @brief Owns the fixed-size control mapping; access requires TryLock. */
class Endpoint
{
public:
    ~Endpoint();
    void Open(const std::string& instance, const std::string& stream, bool create);
    void Close() noexcept;
    WireHeader& Header() noexcept { return *header_; }
    HANDLE Mutex() const noexcept { return mutex_.Get(); }
    HANDLE ProducerLease() const noexcept { return lease_.Get(); }
    bool Ready() const noexcept { return header_ != nullptr; }
private:
    NativeHandle mapping_;
    NativeHandle mutex_;
    NativeHandle lease_;
    WireHeader* header_ = nullptr;
};
void CheckHr(HRESULT result, const char* operation);
void CreateDevice(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& context,
                  const LUID* adapter = nullptr);
LUID AdapterOf(ID3D11Device* device);
bool ProcessAlive(std::uint32_t pid) noexcept;
DXGI_FORMAT NativeFormat(std::uint32_t format) noexcept;
void CreateCompletionQuery(ID3D11Device* device, ComPtr<ID3D11Query>& query);
} // namespace mfd::stream::detail
#endif
