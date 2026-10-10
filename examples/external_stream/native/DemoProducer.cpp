#include "mfd/stream/StreamProducer.h"
#include <d3d11.h>
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

namespace
{
struct DemoStream
{
    mfd::stream::StreamProducer producer;
    std::string instance, stream;
    std::uint32_t width = 0, height = 0;
    mfd::StreamPixelFormat format = mfd::StreamPixelFormat::Rgba32;
    bool opened = false;
};
std::unordered_map<std::uint32_t, std::unique_ptr<DemoStream>> streams;
std::uint32_t nextId = 0;
int Code(const mfd::StreamResult result) noexcept { return static_cast<int>(result); }
}
extern "C"
{
/** @brief Creates a demo adapter; arguments are validated before opening native resources. */
__declspec(dllexport) std::uint32_t mfd_stream_create(const char* instance, const char* stream,
    std::uint32_t width, std::uint32_t height, std::uint32_t format) noexcept
{
    try
    {
        if (!instance || !stream || !mfd::IsValidStreamId(instance) || !mfd::IsValidStreamId(stream) ||
            !mfd::IsValidStreamConfiguration(width, height, static_cast<mfd::StreamPixelFormat>(format)) ||
            streams.size() >= 4 || nextId == UINT32_MAX) return 0;
        auto value = std::make_unique<DemoStream>();
        value->instance = instance; value->stream = stream; value->width = width; value->height = height;
        value->format = static_cast<mfd::StreamPixelFormat>(format);
        const auto id = ++nextId; streams.emplace(id, std::move(value)); return id;
    }
    catch (...) { return 0; }
}
/** @brief Uploads top-left, tightly packed CPU pixels through the real shared GPU adapter. */
__declspec(dllexport) int mfd_stream_submit(std::uint32_t id, const void* pixels, std::uint32_t bytes,
                                          std::uint64_t frame) noexcept
{
    try
    {
        const auto found = streams.find(id);
        if (found == streams.end() || !pixels) return Code(mfd::StreamResult::InvalidArgument);
        DemoStream& stream = *found->second;
        if (bytes != stream.width * stream.height * 4) return Code(mfd::StreamResult::InvalidArgument);
        if (!stream.opened)
        {
            const auto result = stream.producer.Open(stream.instance, stream.stream, stream.width, stream.height, stream.format);
            if (result == mfd::StreamResult::Disconnected)
            {
                stream.producer.Close();
                return Code(result);
            }
            if (result != mfd::StreamResult::Ok && result != mfd::StreamResult::Reconfigured) return Code(result);
            stream.opened = true;
        }
        ID3D11Texture2D* texture = nullptr;
        ID3D11DeviceContext* context = nullptr;
        const auto acquired = stream.producer.TryAcquire(texture, context);
        if (acquired == mfd::StreamResult::Disconnected)
        { stream.producer.Close(); stream.opened = false; return Code(acquired); }
        if (acquired != mfd::StreamResult::Ok) return Code(acquired);
        context->UpdateSubresource(texture, 0, nullptr, pixels, stream.width * 4, 0);
        const auto published = stream.producer.Publish(frame);
        if (published != mfd::StreamResult::Ok) stream.producer.Cancel();
        return Code(published);
    }
    catch (...) { return Code(mfd::StreamResult::Error); }
}
/** @brief Polls completion even when the animation is paused. */
__declspec(dllexport) int mfd_stream_poll(std::uint32_t id) noexcept
{
    try
    {
        const auto found = streams.find(id);
        return found == streams.end() ? Code(mfd::StreamResult::InvalidArgument) : Code(found->second->producer.Poll());
    }
    catch (...) { return Code(mfd::StreamResult::Error); }
}
/** @brief Copies the last diagnostic to a bounded caller-owned UTF-8 buffer. */
__declspec(dllexport) void mfd_stream_error(std::uint32_t id, char* text, std::uint32_t capacity) noexcept
{
    if (!text || !capacity) return;
    text[0] = '\0';
    const auto found = streams.find(id);
    if (found == streams.end()) return;
    const auto& message = found->second->producer.Diagnostic();
    const auto length = std::min<std::size_t>(message.size(), capacity - 1);
    std::memcpy(text, message.data(), length); text[length] = '\0';
}
/** @brief Closes one demo adapter, including active or pending writes. */
__declspec(dllexport) void mfd_stream_close(std::uint32_t id) noexcept { streams.erase(id); }
}
