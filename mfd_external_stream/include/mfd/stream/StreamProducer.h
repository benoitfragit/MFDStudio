#pragma once
#include <memory>
#include <string>
#include "mfd/model/ExternalStreamTypes.h"

struct ID3D11Texture2D;
struct ID3D11DeviceContext;

namespace mfd::stream
{
/** @brief Single-threaded, noncopyable adapter for a local native D3D11 producer.
 * @note Native pointers returned here are process-local borrowed objects, never wire identifiers.
 */
class StreamProducer
{
public:
    StreamProducer();
    ~StreamProducer();
    StreamProducer(const StreamProducer&) = delete;
    StreamProducer& operator=(const StreamProducer&) = delete;
    /** @brief Opens a consumer endpoint and requests its effective configuration.
     * @param instance Consumer instance printed by the runtime.
     * @param streamId Authored stream identifier.
     * @param width Desired pixel width.
     * @param height Desired pixel height.
     * @param format Desired byte order.
     * @return Ok, temporary Unavailable or an explicit failure.
     * @note Repeated calls after Unavailable are safe. Call Close before changing the request.
     */
    StreamResult Open(const std::string& instance, const std::string& streamId,
                      std::uint32_t width, std::uint32_t height, StreamPixelFormat format);
    /** @brief Polls GPU completion once and obtains one available surface without waiting.
     * @param texture Receives a borrowed writable surface on Ok, otherwise nullptr.
     * @param context Receives its device's immediate context on Ok, otherwise nullptr.
     * @return Ok, Unavailable, Reconfigured, Disconnected or Error.
     * @pre All calls and D3D submissions use this adapter's owning thread.
     * @note The adapter creates its own D3D11 device on the consumer's GPU. A copy
     * source must be accessible on that device; arbitrary external texture handles
     * are not imported by this API. Submit writes on the returned immediate context.
     * @note Do not release the borrowed pointers or use them after Publish, Cancel,
     * Close or reconfiguration. Acquire again before writing another frame.
     */
    StreamResult TryAcquire(ID3D11Texture2D*& texture, ID3D11DeviceContext*& context);
    /** @brief Submits the acquired surface; availability to the reader follows GPU completion.
     * @param frameNumber Strictly increasing, nonzero producer frame number within this connection.
     * @return Ok for accepted submission; this does not imply consumption or presentation.
     */
    StreamResult Publish(std::uint64_t frameNumber);
    /** @brief Discards the active write after GPU completion, without waiting. */
    void Cancel() noexcept;
    /** @brief Polls pending GPU publications and refreshes producer liveness once.
     * @return Current connection/configuration result.
     */
    StreamResult Poll();
    /** @brief Closes the endpoint; safe with an acquired or pending surface. */
    void Close() noexcept;
    /** @brief Returns the last diagnostic without I/O or GPU work. */
    const std::string& Diagnostic() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mfd::stream
