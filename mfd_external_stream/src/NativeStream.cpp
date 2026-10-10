#include "NativeStream.h"
#include <cstring>
#include <stdexcept>
#if defined(_WIN32)
namespace mfd::stream::detail
{
TryLock::TryLock(HANDLE mutex) noexcept : mutex_(mutex), result_(WaitForSingleObject(mutex, 0)) {}
TryLock::~TryLock()
{ if (result_ == WAIT_OBJECT_0 || result_ == WAIT_ABANDONED) ReleaseMutex(mutex_); }
Endpoint::~Endpoint() { Close(); }
void Endpoint::Close() noexcept
{
    if (header_) UnmapViewOfFile(header_);
    header_ = nullptr;
    mapping_.Reset();
    mutex_.Reset();
    lease_.Reset();
}
void Endpoint::Open(const std::string& instance, const std::string& stream, const bool create)
{
    Close();
    if (!IsValidStreamId(instance) || !IsValidStreamId(stream))
        throw std::runtime_error("Invalid external stream instance/id");
    const std::string base = "Local\\MFDStudio.Stream.v1." + instance + "." + stream;
    lease_.Reset(CreateMutexA(nullptr, FALSE, (base + ".producer").c_str()));
    bool existing = false;
    if (create)
    {
        mapping_.Reset(CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                         sizeof(WireHeader), base.c_str()));
        existing = GetLastError() == ERROR_ALREADY_EXISTS;
        mutex_.Reset(CreateMutexA(nullptr, FALSE, (base + ".control").c_str()));
    }
    else
    {
        mapping_.Reset(OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, base.c_str()));
        mutex_.Reset(OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, (base + ".control").c_str()));
    }
    if (!mapping_.Get() || !mutex_.Get() || !lease_.Get())
    { Close(); throw std::runtime_error("External stream endpoint is unavailable"); }
    // MapViewOfFile supplies suitably aligned storage for the fixed, ABI-checked wire layout.
    header_ = static_cast<WireHeader*>(MapViewOfFile(mapping_.Get(), FILE_MAP_ALL_ACCESS, 0, 0,
                                                   sizeof(WireHeader)));
    if (!header_) { Close(); throw std::runtime_error("Cannot map stream control block"); }
    if (create)
    {
        TryLock lock(Mutex());
        if (!lock.Acquired()) throw std::runtime_error("Cannot initialize stream endpoint");
        if (existing && (!ValidHeader(*header_) || ProcessAlive(header_->hostPid)))
            throw std::runtime_error("Duplicate external stream consumer");
        WireHeader initial;
        initial.bytes = sizeof(WireHeader);
        initial.hostPid = GetCurrentProcessId();
        LARGE_INTEGER counter {};
        QueryPerformanceCounter(&counter);
        initial.session = static_cast<std::uint64_t>(counter.QuadPart) | 1ULL;
        initial.hostHeartbeat = GetTickCount64();
        std::memcpy(header_, &initial, sizeof(initial));
    }
}
void CheckHr(const HRESULT result, const char* operation)
{
    if (FAILED(result)) throw std::runtime_error(std::string(operation) + " HRESULT=" +
                                               std::to_string(static_cast<unsigned long>(result)));
}
void CreateDevice(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& context, const LUID* adapter)
{
    ComPtr<IDXGIAdapter> selected;
    if (adapter)
    {
        ComPtr<IDXGIFactory1> factory;
        CheckHr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1");
        for (UINT index = 0; index < 32; ++index)
        {
            ComPtr<IDXGIAdapter> candidate;
            if (factory->EnumAdapters(index, &candidate) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC desc {};
            if (candidate && SUCCEEDED(candidate->GetDesc(&desc)) &&
                desc.AdapterLuid.LowPart == adapter->LowPart && desc.AdapterLuid.HighPart == adapter->HighPart)
            { selected = candidate; break; }
        }
        if (!selected) throw std::runtime_error("Consumer GPU adapter is not available");
    }
    CheckHr(D3D11CreateDevice(selected.Get(), selected ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                             nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                             &device, nullptr, &context), "D3D11CreateDevice");
}
LUID AdapterOf(ID3D11Device* device)
{
    ComPtr<IDXGIDevice> dxgi;
    CheckHr(device->QueryInterface(IID_PPV_ARGS(&dxgi)), "DXGI device");
    ComPtr<IDXGIAdapter> adapter;
    CheckHr(dxgi->GetAdapter(&adapter), "DXGI adapter");
    DXGI_ADAPTER_DESC desc {};
    CheckHr(adapter->GetDesc(&desc), "DXGI adapter description");
    return desc.AdapterLuid;
}
bool ProcessAlive(const std::uint32_t pid) noexcept
{
    if (pid == 0) return false;
    NativeHandle process(OpenProcess(SYNCHRONIZE, FALSE, pid));
    return process.Get() && WaitForSingleObject(process.Get(), 0) == WAIT_TIMEOUT;
}
DXGI_FORMAT NativeFormat(const std::uint32_t format) noexcept
{
    return format == static_cast<std::uint32_t>(StreamPixelFormat::Bgra32) ?
        DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
}
void CreateCompletionQuery(ID3D11Device* device, ComPtr<ID3D11Query>& query)
{
    D3D11_QUERY_DESC desc {};
    desc.Query = D3D11_QUERY_EVENT;
    CheckHr(device->CreateQuery(&desc, &query), "Create GPU completion query");
}
} // namespace mfd::stream::detail
#endif
