#pragma once
#include "StreamGl.h"
#include "StreamBudget.h"
#include <vector>
#if defined(_WIN32)
namespace mfd::stream::detail
{
/** @brief Reusable staging, packed CPU rows and ordinary GL texture for one generation.
 * @note The receiver keeps the source slot Reading until Ready(), including across Stop.
 * @pre Dimensions and format satisfy IsValidStreamConfiguration; the GL context is current.
 */
class CpuReadback
{
public:
    CpuReadback(const GlInterop& api, ID3D11Device* device, std::uint32_t width,
                std::uint32_t height, std::uint32_t format);
    /** @brief Enqueues one copy and submits its event query, without waiting. */
    void Begin(ID3D11DeviceContext* context, ID3D11Texture2D* source);
    /** @brief Polls completion once, without implicitly flushing or waiting. */
    bool Ready(ID3D11DeviceContext* context) const;
    /** @brief Maps without waiting and uploads packed rows; false means try on a later tick.
     * @pre Ready() and the previous display's GL fence have completed.
     */
    bool Upload(ID3D11DeviceContext* context);
    GLuint Texture() const noexcept { return texture_.Get(); }
private:
    MemoryReservation memory_;
    GlInterop api_;
    ComPtr<ID3D11Texture2D> staging_;
    ComPtr<ID3D11Query> completion_;
    GlTexture texture_;
    std::vector<unsigned char> pixels_;
    std::uint32_t width_, height_;
    GLenum pixelFormat_;
};
} // namespace mfd::stream::detail
#endif
