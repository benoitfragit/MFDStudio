#pragma once
#include <memory>
#include <string>
#include "mfd/model/ExternalStreamTypes.h"

namespace mfd::stream
{
/** @brief One consumer-owned triple surface bound to the current OpenGL context.
 * @note Construct, update, and destroy on the context thread before destroying that context.
 */
class StreamReceiver
{
public:
    /** @brief Creates a local endpoint; GPU resources are negotiated by the producer.
     * @param instance Unique host-renderer instance identifier.
     * @param streamId Validated logical stream identifier.
     */
    StreamReceiver(const std::string& instance, const std::string& streamId);
    ~StreamReceiver();
    StreamReceiver(const StreamReceiver&) = delete;
    StreamReceiver& operator=(const StreamReceiver&) = delete;
    /** @brief Performs bounded control work and optionally consumes the newest completed frame.
     * @param playback Consumption mode.
     * @param stopRevision Monotonic Stop revision, including Stops followed by Play in one batch.
     * @param visible False prevents consumption and all pixel work.
     */
    void Update(StreamPlayback playback, std::uint64_t stopRevision, bool visible);
    /** @brief Marks the end of all OpenGL draws using this surface for this frame. */
    void EndFrame();
    /** @brief Returns a borrowed GL texture name, or zero without a valid image. */
    unsigned int Texture() const noexcept;
    /** @brief Returns effective pixel width without GPU work. */
    std::uint32_t Width() const noexcept;
    /** @brief Returns effective pixel height without GPU work. */
    std::uint32_t Height() const noexcept;
    /** @brief Returns the last local status snapshot without I/O. */
    const ExternalStreamStatus& Status() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
/** @brief Creates a process/renderer-specific endpoint namespace. */
std::string NewStreamInstance();
} // namespace mfd::stream
