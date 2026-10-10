#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "mfd/model/Types.h"

namespace mfd
{
/** @brief Runtime consumption mode; does not control the producer application. */
enum class StreamPlayback : std::uint32_t { Play, Pause, Stop };
/** @brief Pixel byte order. Both formats use straight alpha and top-left row zero. */
enum class StreamPixelFormat : std::uint32_t { Rgba32 = 1, Bgra32 = 2 };
/** @brief Display fitting in the authored logical rectangle. */
enum class StreamFit : std::uint32_t { Contain, Stretch };
/** @brief Producer adapter result; Unavailable never waits for the consumer. */
enum class StreamResult : std::uint32_t
{
    Ok, Unavailable, Disconnected, Reconfigured, InvalidArgument, Unsupported, Error
};
/** @brief Persistent external surface configuration, independent of native resource lifetime. */
struct ExternalStreamGeometry
{
    std::string streamId = "camera";
    float width = 1.6f;
    float height = 0.9f;
    ColorRgba fallback {24, 24, 32, 255};
    StreamFit fit = StreamFit::Contain;
    StreamPlayback defaultPlayback = StreamPlayback::Play;
    /** @brief Runtime-only mode; serialization preserves defaultPlayback instead. */
    StreamPlayback playback = StreamPlayback::Play;
    /** @brief Runtime-only Stop revision, retained across subsequent Play in the same batch. */
    std::uint64_t stopRevision = 0;
};
/** @brief Cached observation; consumedFrame is not a screen presentation counter. */
struct ExternalStreamStatus
{
    bool producerConnected = false;
    bool hasImage = false;
    bool fresh = false;
    StreamPlayback playback = StreamPlayback::Play;
    StreamResult result = StreamResult::Unavailable;
    std::uint64_t consumedFrame = 0;
    std::uint64_t generation = 0;
    std::string diagnostic;
};
/** @brief Validates an object-name component without allowing namespace injection.
 * @param id ASCII stream or instance identifier.
 * @return Whether the identifier contains 1..64 letters, digits, underscores or hyphens.
 */
inline bool IsValidStreamId(const std::string_view id) noexcept
{
    if (id.empty() || id.size() > 64) return false;
    for (const char character : id)
    {
        if (!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '_' || character == '-')) return false;
    }
    return true;
}
/** @brief Validates bounded wire dimensions and format before allocation.
 * @param width Pixel width, at most 1920.
 * @param height Pixel height, at most 1080.
 * @param format RGBA32 or BGRA32.
 * @return Whether all fields satisfy the version-one contract.
 */
inline bool IsValidStreamConfiguration(const std::uint32_t width, const std::uint32_t height,
                                      const StreamPixelFormat format) noexcept
{
    return width > 0 && height > 0 && width <= 1920 && height <= 1080 &&
           (format == StreamPixelFormat::Rgba32 || format == StreamPixelFormat::Bgra32);
}
} // namespace mfd
