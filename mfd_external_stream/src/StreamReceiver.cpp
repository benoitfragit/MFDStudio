#include "mfd/stream/StreamReceiver.h"
#include "NativeStream.h"
#include "StreamGeneration.h"
#include "StreamBudget.h"
#include <atomic>
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
    detail::GlInterop api;
    std::unique_ptr<detail::Generation> current;
    std::unique_ptr<detail::Generation> retained;
    detail::Generation* display = nullptr;
    void* fence = nullptr;
    std::uint64_t appliedRequest = 0;
    std::uint64_t stopRevision = 0;
    std::uint32_t copyingSlot = detail::kNoSlot;
    detail::ConfigurationRetry retry;
    bool clearDisplay = false;
    bool invalidateEpoch = false;
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
        if (display == current.get() && index < detail::kSlotCount)
            header.slots[index].state = static_cast<std::uint32_t>(detail::SlotState::Free);
        display = nullptr;
        status.hasImage = false;
        retained.reset();
        if (fence) { api.deleteFence(fence); fence = nullptr; }
    }
    /** @brief Latches playback intent even if this tick cannot acquire the control mutex. */
    void ObservePlayback(const StreamPlayback playback, const std::uint64_t revision)
    {
        const bool enteringStop = playback == StreamPlayback::Stop && previousPlayback != StreamPlayback::Stop;
        const bool restarting = playback == StreamPlayback::Play && previousPlayback == StreamPlayback::Stop;
        if (revision != stopRevision || enteringStop || restarting) invalidateEpoch = true;
        if (revision != stopRevision || playback == StreamPlayback::Stop)
        {
            clearDisplay = true;
            status.hasImage = false;
        }
        previousPlayback = playback;
    }
    void ApplyPlayback(detail::WireHeader& header, const StreamPlayback playback, const std::uint64_t revision)
    {
        if (invalidateEpoch)
        {
            ++header.epoch;
            detail::InvalidateReady(header);
            invalidateEpoch = false;
        }
        if (revision != stopRevision || playback == StreamPlayback::Stop) detail::InvalidateReady(header);
        stopRevision = revision;
        if (clearDisplay && ReadCompleted()) { ReleaseDisplay(header); clearDisplay = false; }
    }
    void Configure(detail::WireHeader& header)
    {
        if (!IsValidStreamConfiguration(header.requestedWidth, header.requestedHeight,
                                        static_cast<StreamPixelFormat>(header.requestedFormat)))
            throw std::runtime_error("Rejected stream dimensions/format (maximum 1920x1080)");
        if (copyingSlot != detail::kNoSlot || !retry.Ready(header.hostHeartbeat)) return;
        if (!device) detail::CreateDevice(device, context);
        const LUID adapter = detail::AdapterOf(device.Get());
        std::unique_ptr<detail::Generation> next;
        try
        {
            next = std::make_unique<detail::Generation>(api, device.Get(), header.requestedWidth,
                                header.requestedHeight, header.requestedFormat, header.generation + 1);
        }
        catch (const detail::BudgetUnavailable&)
        {
            retry.Defer(header.hostHeartbeat);
            header.result = static_cast<std::uint32_t>(StreamResult::Unavailable);
            return;
        }
        // Commit only after every allocation and adapter query has succeeded.
        if (display == current.get()) retained = std::move(current);
        current = std::move(next);
        header.width = header.requestedWidth; header.height = header.requestedHeight;
        header.format = header.requestedFormat; ++header.generation;
        header.adapterLow = adapter.LowPart; header.adapterHigh = adapter.HighPart;
        for (std::size_t index = 0; index < detail::kSlotCount; ++index)
        { header.slots[index] = {}; header.slots[index].handle = current->Handle(index); }
        appliedRequest = header.requestSerial;
        retry.Reset();
        header.result = static_cast<std::uint32_t>(StreamResult::Ok);
    }
    void Adopt(const detail::WireHeader& header, const std::uint32_t index)
    {
        display = current.get();
        status.hasImage = true;
        status.consumedFrame = header.slots[index].frame;
        status.generation = current->Number();
    }
    /** @brief Retires completed copies even while hidden, without mapping discarded pixels. */
    void PollCopy(detail::WireHeader& header, const bool consume)
    {
        if (copyingSlot == detail::kNoSlot || !current->CopyReady(context.Get())) return;
        const auto& slot = header.slots[copyingSlot];
        if (consume && slot.epoch == header.epoch)
        {
            if (!ReadCompleted() || !current->UploadCopy(context.Get())) return;
            ReleaseDisplay(header);
            Adopt(header, copyingSlot);
        }
        header.slots[copyingSlot].state = static_cast<std::uint32_t>(detail::SlotState::Free);
        copyingSlot = detail::kNoSlot;
    }
    void Consume(detail::WireHeader& header)
    {
        if (copyingSlot != detail::kNoSlot || !ReadCompleted()) return;
        const auto latest = detail::FindLatest(header);
        if (latest == detail::kNoSlot) return;
        if (!current->UsesCpu())
        {
            if (!retry.Ready(header.hostHeartbeat)) return;
            ReleaseDisplay(header);
            try { current->Acquire(latest, device.Get()); }
            catch (const detail::BudgetUnavailable&)
            { retry.Defer(header.hostHeartbeat); return; }
            retry.Reset();
        }
        header.slots[latest].state = static_cast<std::uint32_t>(detail::SlotState::Reading);
        if (current->UsesCpu())
        {
            current->BeginCopy(context.Get(), latest);
            copyingSlot = latest;
        }
        else Adopt(header, latest);
        detail::InvalidateReady(header);
    }
    void RefreshStatus()
    {
        status.result = status.producerConnected ? StreamResult::Ok : StreamResult::Disconnected;
        status.diagnostic = current ? current->Diagnostic() + "; " : "";
        if (retry.Deferred())
        {
            if (status.producerConnected) status.result = StreamResult::Unavailable;
            status.diagnostic += "Temporary resource budget shortage (128 MiB); retry at most every 250 ms; retained image remains valid";
        }
        else status.diagnostic += status.producerConnected ?
            (status.fresh ? "Connected" : "Producer heartbeat stale; retained image remains valid") :
            "Waiting for producer; last valid image is retained";
    }
    void Fail(const StreamResult result, const char* diagnostic)
    {
        status.diagnostic = diagnostic;
        status.result = result;
        failed = true;
        status.hasImage = false;
        status.fresh = false;
        detail::TryLock lock(endpoint.Mutex());
        if (lock.Acquired()) endpoint.Header().result = static_cast<std::uint32_t>(result);
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
    state.ObservePlayback(playback, stopRevision);
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
        state.ApplyPlayback(header, playback, stopRevision);
        if (state.device) detail::CheckHr(state.device->GetDeviceRemovedReason(), "Fatal stream D3D11 device loss");
        const bool consume = visible && playback == StreamPlayback::Play && !state.clearDisplay;
        state.PollCopy(header, consume);
        if (header.producerPid && header.requestedWidth && header.requestSerial != state.appliedRequest)
            state.Configure(header);
        if (consume && state.current) state.Consume(header);
        state.RefreshStatus();
    }
    catch (const detail::UnsupportedDisplay& error)
    { state.Fail(StreamResult::Unsupported, error.what()); }
    catch (const std::exception& error)
    { state.Fail(StreamResult::Error, error.what()); }
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
        impl_->Fail(StreamResult::Error, "Cannot fence stream draw");
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
