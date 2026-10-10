#include "mfd/stream/StreamProducer.h"
#include "NativeStream.h"
#include <array>
#include <limits>
#include <mutex>
#include <unordered_set>
#include <stdexcept>

namespace mfd::stream
{
namespace
{
// A Win32 mutex is recursive on its owning thread. This local guard also rejects two adapters
// opened for the same endpoint on that thread, while the native lease handles other processes.
struct LocalOwners
{
    std::mutex mutex;
    std::unordered_set<std::string> keys;
};
std::shared_ptr<LocalOwners> ProcessOwners()
{
    // Each adapter retains the registry through static-object destruction in embedding DLLs.
    static const auto owners = std::make_shared<LocalOwners>();
    return owners;
}
}
struct StreamProducer::Impl
{
    std::string diagnostic;
    std::string localOwner;
    std::shared_ptr<LocalOwners> owners = ProcessOwners();
#if defined(_WIN32)
    detail::Endpoint endpoint;
    detail::ComPtr<ID3D11Device> device;
    detail::ComPtr<ID3D11DeviceContext> context;
    std::array<detail::ComPtr<ID3D11Texture2D>, detail::kSlotCount> textures;
    std::array<detail::ComPtr<ID3D11Query>, detail::kSlotCount> queries;
    std::array<bool, detail::kSlotCount> pending {};
    std::array<bool, detail::kSlotCount> discard {};
    std::uint64_t session = 0;
    std::uint64_t generation = 0;
    std::uint64_t lastFrame = 0;
    std::uint32_t writing = detail::kNoSlot;
    bool registered = false;
    bool leaseOwned = false;

    StreamResult Synchronize(detail::WireHeader& header)
    {
        if (!detail::ValidHeader(header)) throw std::runtime_error("Invalid stream protocol header");
        if (!detail::ProcessAlive(header.hostPid) || (session && session != header.session) ||
            GetTickCount64() - header.hostHeartbeat >= 2000)
            return StreamResult::Disconnected;
        if (header.producerPid != GetCurrentProcessId()) return StreamResult::Disconnected;
        header.producerHeartbeat = GetTickCount64();
        if (header.result != static_cast<std::uint32_t>(StreamResult::Ok))
            return static_cast<StreamResult>(header.result);
        if (!header.generation) return StreamResult::Unavailable;
        if (generation != header.generation)
        {
            if (!IsValidStreamConfiguration(header.width, header.height,
                                            static_cast<StreamPixelFormat>(header.format)))
                throw std::runtime_error("Invalid negotiated stream dimensions/format");
            for (auto& texture : textures) texture.Reset();
            for (auto& query : queries) query.Reset();
            context.Reset(); device.Reset();
            pending.fill(false); discard.fill(false); writing = detail::kNoSlot;
            const LUID adapter {header.adapterLow, header.adapterHigh};
            detail::CreateDevice(device, context, &adapter);
            for (std::size_t index = 0; index < detail::kSlotCount; ++index)
            {
                const auto share = header.slots[index].handle;
                if (share == 0 || share > std::numeric_limits<std::uint32_t>::max())
                    throw std::runtime_error("Invalid legacy DXGI share handle");
                // DXGI legacy shared handles are opaque, zero-extended 32-bit tokens, not pointers.
                const HANDLE handle = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(share));
                detail::CheckHr(device->OpenSharedResource(handle, IID_PPV_ARGS(&textures[index])),
                                "Open shared producer texture");
                D3D11_TEXTURE2D_DESC desc {};
                textures[index]->GetDesc(&desc);
                if (desc.Width != header.width || desc.Height != header.height ||
                    desc.Format != detail::NativeFormat(header.format) || desc.SampleDesc.Count != 1)
                    throw std::runtime_error("Shared texture does not match negotiated configuration");
                detail::CreateCompletionQuery(device.Get(), queries[index]);
            }
            session = header.session; generation = header.generation;
            return StreamResult::Reconfigured;
        }
        for (std::size_t index = 0; index < detail::kSlotCount; ++index)
        {
            if (!pending[index]) continue;
            const HRESULT result = context->GetData(queries[index].Get(), nullptr, 0,
                                                     D3D11_ASYNC_GETDATA_DONOTFLUSH);
            detail::CheckHr(result, "Poll producer GPU completion");
            if (result != S_OK) continue;
            auto& slot = header.slots[index];
            slot.state = static_cast<std::uint32_t>(discard[index] || slot.epoch != header.epoch ?
                                                    detail::SlotState::Free : detail::SlotState::Ready);
            pending[index] = false;
        }
        return StreamResult::Ok;
    }
#endif
};
StreamProducer::StreamProducer() : impl_(std::make_unique<Impl>()) {}
StreamProducer::~StreamProducer() { Close(); }
const std::string& StreamProducer::Diagnostic() const noexcept { return impl_->diagnostic; }
StreamResult StreamProducer::Open(const std::string& instance, const std::string& streamId,
                                 const std::uint32_t width, const std::uint32_t height,
                                 const StreamPixelFormat format)
{
    if (!IsValidStreamId(instance) || !IsValidStreamId(streamId) ||
        !IsValidStreamConfiguration(width, height, format)) return StreamResult::InvalidArgument;
#if defined(_WIN32)
    try
    {
        if (impl_->localOwner.empty())
        {
            const std::string key = instance + "." + streamId;
            std::lock_guard<std::mutex> ownerLock(impl_->owners->mutex);
            if (!impl_->owners->keys.insert(key).second) return StreamResult::Unavailable;
            impl_->localOwner = key;
        }
        if (!impl_->endpoint.Ready()) impl_->endpoint.Open(instance, streamId, false);
        if (!impl_->leaseOwned)
        {
            const DWORD lease = WaitForSingleObject(impl_->endpoint.ProducerLease(), 0);
            if (lease != WAIT_OBJECT_0 && lease != WAIT_ABANDONED) return StreamResult::Unavailable;
            impl_->leaseOwned = true;
        }
        detail::TryLock lock(impl_->endpoint.Mutex());
        if (lock.Abandoned()) throw std::runtime_error("Abandoned control mutex; restart consumer");
        if (!lock.Acquired()) return StreamResult::Unavailable;
        auto& header = impl_->endpoint.Header();
        if (!detail::ValidHeader(header)) return StreamResult::Unavailable;
        if (!impl_->registered)
        {
            header.producerPid = GetCurrentProcessId();
            header.producerHeartbeat = GetTickCount64();
            header.requestedWidth = width; header.requestedHeight = height;
            header.requestedFormat = static_cast<std::uint32_t>(format);
            ++header.requestSerial;
            header.result = static_cast<std::uint32_t>(StreamResult::Unavailable);
            impl_->registered = true;
        }
        return impl_->Synchronize(header);
    }
    catch (const std::exception& error)
    {
        impl_->diagnostic = error.what();
        return impl_->endpoint.Ready() ? StreamResult::Error : StreamResult::Unavailable;
    }
#else
    impl_->diagnostic = "External streams require Windows D3D11/WGL";
    return StreamResult::Unsupported;
#endif
}
StreamResult StreamProducer::Poll()
{
#if defined(_WIN32)
    if (!impl_->endpoint.Ready() || !impl_->registered) return StreamResult::Disconnected;
    try
    {
        detail::TryLock lock(impl_->endpoint.Mutex());
        if (lock.Abandoned()) throw std::runtime_error("Abandoned stream control mutex");
        if (!lock.Acquired()) return StreamResult::Unavailable;
        return impl_->Synchronize(impl_->endpoint.Header());
    }
    catch (const std::exception& error)
    { impl_->diagnostic = error.what(); return StreamResult::Error; }
#else
    return StreamResult::Unsupported;
#endif
}
StreamResult StreamProducer::TryAcquire(ID3D11Texture2D*& texture, ID3D11DeviceContext*& context)
{
    texture = nullptr; context = nullptr;
    const StreamResult result = Poll();
    if (result != StreamResult::Ok) return result;
#if defined(_WIN32)
    if (impl_->writing != detail::kNoSlot) return StreamResult::InvalidArgument;
    detail::TryLock lock(impl_->endpoint.Mutex());
    if (!lock.Acquired()) return lock.Abandoned() ? StreamResult::Error : StreamResult::Unavailable;
    auto& header = impl_->endpoint.Header();
    if (header.generation != impl_->generation) return StreamResult::Reconfigured;
    const auto index = detail::FindWritable(header);
    if (index == detail::kNoSlot) return StreamResult::Unavailable;
    auto& slot = header.slots[index];
    slot.state = static_cast<std::uint32_t>(detail::SlotState::Writing);
    slot.epoch = header.epoch;
    impl_->writing = index;
    texture = impl_->textures[index].Get(); context = impl_->context.Get();
    return StreamResult::Ok;
#else
    return StreamResult::Unsupported;
#endif
}
StreamResult StreamProducer::Publish(const std::uint64_t frameNumber)
{
#if defined(_WIN32)
    if (impl_->writing == detail::kNoSlot || frameNumber == 0 || frameNumber <= impl_->lastFrame)
        return StreamResult::InvalidArgument;
    detail::TryLock lock(impl_->endpoint.Mutex());
    if (!lock.Acquired()) return lock.Abandoned() ? StreamResult::Error : StreamResult::Unavailable;
    auto& header = impl_->endpoint.Header();
    if (header.generation != impl_->generation) { impl_->writing = detail::kNoSlot; return StreamResult::Reconfigured; }
    const auto index = impl_->writing;
    impl_->context->End(impl_->queries[index].Get());
    impl_->context->Flush();
    header.slots[index].frame = frameNumber;
    header.slots[index].state = static_cast<std::uint32_t>(detail::SlotState::Pending);
    impl_->pending[index] = true; impl_->discard[index] = false;
    impl_->writing = detail::kNoSlot; impl_->lastFrame = frameNumber;
    return StreamResult::Ok;
#else
    return StreamResult::Unsupported;
#endif
}
void StreamProducer::Cancel() noexcept
{
#if defined(_WIN32)
    if (impl_->writing == detail::kNoSlot) return;
    const auto index = impl_->writing;
    impl_->context->End(impl_->queries[index].Get()); impl_->context->Flush();
    impl_->pending[index] = true; impl_->discard[index] = true;
    impl_->writing = detail::kNoSlot;
    // Slot remains Writing until Poll observes completion; it cannot be reused early.
#endif
}
void StreamProducer::Close() noexcept
{
#if defined(_WIN32)
    if (impl_->endpoint.Ready() && impl_->registered)
    {
        detail::TryLock lock(impl_->endpoint.Mutex());
        if (lock.Acquired() && impl_->endpoint.Header().producerPid == GetCurrentProcessId())
        {
            impl_->endpoint.Header().producerPid = 0;
            impl_->endpoint.Header().requestedWidth = 0;
        }
    }
    impl_->registered = false;
    if (impl_->leaseOwned) ReleaseMutex(impl_->endpoint.ProducerLease());
    impl_->leaseOwned = false;
    impl_->endpoint.Close();
    for (auto& texture : impl_->textures) texture.Reset();
    for (auto& query : impl_->queries) query.Reset();
    impl_->context.Reset(); impl_->device.Reset();
    impl_->pending.fill(false); impl_->discard.fill(false);
    impl_->generation = 0; impl_->session = 0; impl_->lastFrame = 0;
    impl_->writing = detail::kNoSlot;
#endif
    if (!impl_->localOwner.empty())
    {
        std::lock_guard<std::mutex> ownerLock(impl_->owners->mutex);
        impl_->owners->keys.erase(impl_->localOwner);
        impl_->localOwner.clear();
    }
}
} // namespace mfd::stream
