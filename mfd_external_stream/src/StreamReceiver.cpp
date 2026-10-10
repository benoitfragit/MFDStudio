#include "mfd/stream/StreamReceiver.h"
#include "NativeStream.h"
#include <array>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <utility>
#if defined(_WIN32)
#include <GL/gl.h>
#endif

namespace mfd::stream
{
namespace
{
std::atomic<std::uint32_t> instanceSequence {0};
#if defined(_WIN32)
constexpr std::uint64_t kGpuBudget = 128ULL * 1024 * 1024;
std::atomic<std::uint64_t> allocatedBytes {0};
using OpenDevice = HANDLE(WINAPI*)(void*);
using CloseDevice = BOOL(WINAPI*)(HANDLE);
using RegisterObject = HANDLE(WINAPI*)(HANDLE, void*, GLuint, GLenum, GLenum);
using UnregisterObject = BOOL(WINAPI*)(HANDLE, HANDLE);
using LockObjects = BOOL(WINAPI*)(HANDLE, GLint, HANDLE*);
using FenceSync = void*(APIENTRY*)(GLenum, GLbitfield);
using ClientWaitSync = GLenum(APIENTRY*)(void*, GLbitfield, std::uint64_t);
using DeleteSync = void(APIENTRY*)(void*);
PROC LoadGlFunction(const char* name) noexcept
{
    const PROC function = wglGetProcAddress(name);
    // WGL drivers may return these documented failure sentinels instead of nullptr.
    const auto address = reinterpret_cast<std::uintptr_t>(function);
    return address <= 3 || address == UINTPTR_MAX ? nullptr : function;
}
bool HasInteropExtension()
{
    using GetExtensions = const char*(WINAPI*)(HDC);
    const auto getExtensions = reinterpret_cast<GetExtensions>(LoadGlFunction("wglGetExtensionsStringARB"));
    const char* extensions = getExtensions ? getExtensions(wglGetCurrentDC()) : nullptr;
    if (!extensions) return false;
    constexpr char required[] = "WGL_NV_DX_interop2";
    const char* match = std::strstr(extensions, required);
    return match && (match == extensions || match[-1] == ' ') &&
           (match[sizeof(required) - 1] == ' ' || match[sizeof(required) - 1] == '\0');
}
/** @brief WGL functions loaded per owning context; native casts are required by wglGetProcAddress. */
struct GlInterop
{
    OpenDevice open = nullptr;
    CloseDevice close = nullptr;
    RegisterObject registerObject = nullptr;
    UnregisterObject unregisterObject = nullptr;
    LockObjects lock = nullptr;
    LockObjects unlock = nullptr;
    FenceSync fence = nullptr;
    ClientWaitSync wait = nullptr;
    DeleteSync deleteFence = nullptr;
    bool extensionAvailable = false;
    GlInterop()
    {
        extensionAvailable = HasInteropExtension();
        open = reinterpret_cast<OpenDevice>(LoadGlFunction("wglDXOpenDeviceNV"));
        close = reinterpret_cast<CloseDevice>(LoadGlFunction("wglDXCloseDeviceNV"));
        registerObject = reinterpret_cast<RegisterObject>(LoadGlFunction("wglDXRegisterObjectNV"));
        unregisterObject = reinterpret_cast<UnregisterObject>(LoadGlFunction("wglDXUnregisterObjectNV"));
        lock = reinterpret_cast<LockObjects>(LoadGlFunction("wglDXLockObjectsNV"));
        unlock = reinterpret_cast<LockObjects>(LoadGlFunction("wglDXUnlockObjectsNV"));
        fence = reinterpret_cast<FenceSync>(LoadGlFunction("glFenceSync"));
        wait = reinterpret_cast<ClientWaitSync>(LoadGlFunction("glClientWaitSync"));
        deleteFence = reinterpret_cast<DeleteSync>(LoadGlFunction("glDeleteSync"));
    }
    bool Available() const noexcept
    { return extensionAvailable && open && close && registerObject && unregisterObject && lock && unlock && fence && wait && deleteFence; }
};
/** @brief All three resources and their interop registrations; never copied during role changes. */
class Generation
{
public:
    Generation(const GlInterop& api, ID3D11Device* device, const std::uint32_t width,
               const std::uint32_t height, const std::uint32_t format, const std::uint64_t number)
        : api_(api), width_(width), height_(height), number_(number)
    {
        const std::uint64_t bytes = static_cast<std::uint64_t>(width) * height * 4 * detail::kSlotCount;
        if (allocatedBytes.fetch_add(bytes) + bytes > kGpuBudget)
        { allocatedBytes.fetch_sub(bytes); throw std::runtime_error("External stream GPU budget (128 MiB) exceeded"); }
        bytes_ = bytes;
        try
        {
            bridge_ = api_.open(device);
            if (!bridge_) throw std::runtime_error("D3D11/OpenGL interop unavailable on this adapter");
            D3D11_TEXTURE2D_DESC desc {};
            desc.Width = width; desc.Height = height; desc.ArraySize = 1; desc.MipLevels = 1;
            desc.Format = detail::NativeFormat(format); desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            // Legacy DXGI sharing interoperates with WGL. GPU queries + the control mutex replace keyed mutexes.
            desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
            for (std::size_t index = 0; index < detail::kSlotCount; ++index)
            {
                detail::CheckHr(device->CreateTexture2D(&desc, nullptr, &textures_[index]), "Create shared texture");
                detail::ComPtr<IDXGIResource> resource;
                detail::CheckHr(textures_[index].As(&resource), "Get shared texture resource");
                HANDLE share = nullptr;
                detail::CheckHr(resource->GetSharedHandle(&share), "Get shared texture handle");
                // GetSharedHandle returns an opaque DXGI token; it is not a closeable NT handle.
                handles_[index] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(share));
                glGenTextures(1, &names_[index]);
                objects_[index] = api_.registerObject(bridge_, textures_[index].Get(), names_[index], GL_TEXTURE_2D, 0);
                if (!objects_[index]) throw std::runtime_error("Cannot register shared D3D11 texture with OpenGL");
            }
        }
        catch (...) { Release(); throw; }
    }
    ~Generation() { Release(); }
    Generation(const Generation&) = delete;
    Generation& operator=(const Generation&) = delete;
    void Lock(const std::uint32_t index)
    {
        if (!api_.lock(bridge_, 1, &objects_[index])) throw std::runtime_error("Cannot acquire WGL texture ownership");
        locked_ = index;
        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glBindTexture(GL_TEXTURE_2D, names_[index]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    }
    void Unlock()
    {
        if (locked_ == detail::kNoSlot) return;
        if (!api_.unlock(bridge_, 1, &objects_[locked_])) throw std::runtime_error("Cannot release WGL texture ownership");
        locked_ = detail::kNoSlot;
    }
    GLuint Texture() const noexcept { return locked_ < detail::kSlotCount ? names_[locked_] : 0; }
    std::uint32_t Slot() const noexcept { return locked_; }
    std::uint32_t Width() const noexcept { return width_; }
    std::uint32_t Height() const noexcept { return height_; }
    std::uint64_t Number() const noexcept { return number_; }
    std::uint64_t Handle(const std::size_t index) const noexcept { return handles_[index]; }
private:
    void Release() noexcept
    {
        if (bridge_)
        {
            if (locked_ < detail::kSlotCount) api_.unlock(bridge_, 1, &objects_[locked_]);
            for (std::size_t index = 0; index < detail::kSlotCount; ++index)
            {
                if (objects_[index]) api_.unregisterObject(bridge_, objects_[index]);
                if (names_[index]) glDeleteTextures(1, &names_[index]);
            }
            api_.close(bridge_); bridge_ = nullptr;
        }
        for (auto& texture : textures_) texture.Reset();
        allocatedBytes.fetch_sub(bytes_); bytes_ = 0;
    }
    GlInterop api_;
    std::array<detail::ComPtr<ID3D11Texture2D>, detail::kSlotCount> textures_;
    std::array<GLuint, detail::kSlotCount> names_ {};
    std::array<HANDLE, detail::kSlotCount> objects_ {};
    std::array<std::uint64_t, detail::kSlotCount> handles_ {};
    HANDLE bridge_ = nullptr;
    std::uint64_t bytes_ = 0;
    std::uint32_t width_, height_;
    std::uint64_t number_;
    std::uint32_t locked_ = detail::kNoSlot;
};
#endif
} // namespace
std::string NewStreamInstance()
{
#if defined(_WIN32)
    return std::to_string(GetCurrentProcessId()) + "-" + std::to_string(++instanceSequence);
#else
    return "unsupported-" + std::to_string(++instanceSequence);
#endif
}
struct StreamReceiver::Impl
{
    ExternalStreamStatus status;
#if defined(_WIN32)
    detail::Endpoint endpoint;
    detail::ComPtr<ID3D11Device> device;
    detail::ComPtr<ID3D11DeviceContext> context;
    GlInterop api;
    std::unique_ptr<Generation> current;
    std::unique_ptr<Generation> retained;
    Generation* display = nullptr;
    void* fence = nullptr;
    std::uint64_t appliedRequest = 0;
    std::uint64_t stopRevision = 0;
    bool clearDisplay = false;
    bool failed = false;
    StreamPlayback previousPlayback = StreamPlayback::Play;
    ~Impl()
    {
        if (endpoint.Ready())
        {
            detail::TryLock lock(endpoint.Mutex());
            if (lock.Acquired()) endpoint.Header().hostPid = 0;
        }
        if (fence) api.deleteFence(fence);
        // WGL unlock synchronizes outstanding reads on teardown; ordinary frame updates poll fences first.
        retained.reset(); current.reset();
    }
    bool ReadCompleted()
    {
        if (!fence) return true;
        const GLenum result = api.wait(fence, 0, 0);
        if (result == 0x911D) throw std::runtime_error("OpenGL stream fence failed");
        return result == 0x911A || result == 0x911C;
    }
    void ReleaseDisplay(detail::WireHeader& header)
    {
        if (!display) return;
        const auto index = display->Slot();
        display->Unlock();
        if (display == current.get()) header.slots[index].state = static_cast<std::uint32_t>(detail::SlotState::Free);
        display = nullptr;
        retained.reset();
        if (fence) { api.deleteFence(fence); fence = nullptr; }
    }
    void Configure(detail::WireHeader& header)
    {
        if (!IsValidStreamConfiguration(header.requestedWidth, header.requestedHeight,
                                        static_cast<StreamPixelFormat>(header.requestedFormat)))
            throw std::runtime_error("Rejected stream dimensions/format (maximum 1920x1080)");
        if (!api.Available())
        { status.result = StreamResult::Unsupported; throw std::runtime_error("WGL_NV_DX_interop2 and GL sync support required"); }
        if (!device) detail::CreateDevice(device, context);
        auto next = std::make_unique<Generation>(api, device.Get(), header.requestedWidth,
                                                header.requestedHeight, header.requestedFormat, header.generation + 1);
        if (display == current.get()) retained = std::move(current);
        current = std::move(next);
        header.width = header.requestedWidth; header.height = header.requestedHeight;
        header.format = header.requestedFormat; ++header.generation;
        const LUID adapter = detail::AdapterOf(device.Get());
        header.adapterLow = adapter.LowPart; header.adapterHigh = adapter.HighPart;
        for (std::size_t index = 0; index < detail::kSlotCount; ++index)
        { header.slots[index] = {}; header.slots[index].handle = current->Handle(index); }
        appliedRequest = header.requestSerial;
        header.result = static_cast<std::uint32_t>(StreamResult::Ok);
    }
#endif
};
StreamReceiver::StreamReceiver(const std::string& instance, const std::string& streamId)
    : impl_(std::make_unique<Impl>())
{
#if defined(_WIN32)
    try { impl_->endpoint.Open(instance, streamId, true); }
    catch (const std::exception& error)
    {
        impl_->endpoint.Close();
        impl_->status.result = StreamResult::Error;
        impl_->status.diagnostic = error.what();
        impl_->failed = true;
    }
#else
    impl_->status.result = StreamResult::Unsupported;
    impl_->status.diagnostic = "External streams require Windows D3D11/WGL";
#endif
}
StreamReceiver::~StreamReceiver() = default;
void StreamReceiver::Update(const StreamPlayback playback, const std::uint64_t stopRevision, const bool visible)
{
    auto& state = *impl_;
    state.status.playback = playback;
#if defined(_WIN32)
    if (state.failed || !state.endpoint.Ready()) return;
    try
    {
        detail::TryLock lock(state.endpoint.Mutex());
        if (lock.Abandoned()) throw std::runtime_error("Abandoned stream control mutex; restart endpoint");
        if (!lock.Acquired()) { state.status.fresh = false; return; }
        auto& header = state.endpoint.Header();
        if (!detail::ValidHeader(header)) throw std::runtime_error("Invalid stream control metadata");
        header.hostHeartbeat = GetTickCount64();
        detail::TryLock producerLease(state.endpoint.ProducerLease());
        state.status.producerConnected = !producerLease.Acquired() && !producerLease.Abandoned() &&
                                         detail::ProcessAlive(header.producerPid);
        if (!state.status.producerConnected) { header.producerPid = 0; header.requestedWidth = 0; }
        state.status.fresh = state.status.producerConnected &&
                             header.hostHeartbeat - header.producerHeartbeat < 2000;
        const bool enteringStop = playback == StreamPlayback::Stop && state.previousPlayback != StreamPlayback::Stop;
        const bool restarting = playback == StreamPlayback::Play && state.previousPlayback == StreamPlayback::Stop;
        if (stopRevision != state.stopRevision || enteringStop || restarting)
        { ++header.epoch; detail::InvalidateReady(header); }
        if (stopRevision != state.stopRevision || playback == StreamPlayback::Stop)
        {
            detail::InvalidateReady(header);
            state.stopRevision = stopRevision; state.clearDisplay = true;
            state.status.hasImage = false;
        }
        state.previousPlayback = playback;
        if (state.clearDisplay && state.ReadCompleted())
        { state.ReleaseDisplay(header); state.clearDisplay = false; }
        if (header.producerPid && header.requestedWidth && header.requestSerial != state.appliedRequest)
            state.Configure(header);
        if (state.device) detail::CheckHr(state.device->GetDeviceRemovedReason(), "Stream D3D11 device lost");
        if (visible && playback == StreamPlayback::Play && !state.clearDisplay && state.current && state.ReadCompleted())
        {
            const auto latest = detail::FindLatest(header);
            if (latest < detail::kSlotCount)
            {
                state.ReleaseDisplay(header);
                state.current->Lock(latest);
                state.display = state.current.get();
                header.slots[latest].state = static_cast<std::uint32_t>(detail::SlotState::Reading);
                state.status.hasImage = true;
                state.status.consumedFrame = header.slots[latest].frame;
                state.status.generation = header.generation;
                detail::InvalidateReady(header);
            }
        }
        state.status.result = state.status.producerConnected ? StreamResult::Ok : StreamResult::Disconnected;
        state.status.diagnostic = state.status.producerConnected ?
            (state.status.fresh ? "Connected" : "Producer heartbeat stale; retained image remains valid") :
            "Waiting for producer; last valid image is retained";
    }
    catch (const std::exception& error)
    {
        state.status.diagnostic = error.what();
        if (state.status.result != StreamResult::Unsupported) state.status.result = StreamResult::Error;
        state.failed = true;
        state.status.hasImage = false;
        state.status.fresh = false;
        detail::TryLock lock(state.endpoint.Mutex());
        if (lock.Acquired()) state.endpoint.Header().result = static_cast<std::uint32_t>(state.status.result);
    }
#endif
}
void StreamReceiver::EndFrame()
{
#if defined(_WIN32)
    if (impl_->failed || !impl_->display || !impl_->status.hasImage) return;
    if (impl_->fence) impl_->api.deleteFence(impl_->fence);
    impl_->fence = impl_->api.fence(0x9117, 0);
    glFlush();
    if (!impl_->fence)
    {
        impl_->failed = true;
        impl_->status.hasImage = false;
        impl_->status.fresh = false;
        impl_->status.result = StreamResult::Error;
        impl_->status.diagnostic = "Cannot fence stream draw";
    }
#endif
}
unsigned int StreamReceiver::Texture() const noexcept
{
#if defined(_WIN32)
    return impl_->status.hasImage && impl_->display ? impl_->display->Texture() : 0;
#else
    return 0;
#endif
}
std::uint32_t StreamReceiver::Width() const noexcept
{
#if defined(_WIN32)
    return impl_->display ? impl_->display->Width() : 0;
#else
    return 0;
#endif
}
std::uint32_t StreamReceiver::Height() const noexcept
{
#if defined(_WIN32)
    return impl_->display ? impl_->display->Height() : 0;
#else
    return 0;
#endif
}
const ExternalStreamStatus& StreamReceiver::Status() const noexcept { return impl_->status; }
} // namespace mfd::stream
