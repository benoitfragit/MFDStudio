#pragma once
#include "StreamReadback.h"
#include <array>
#include <memory>
#if defined(_WIN32)
namespace mfd::stream::detail
{
/** @brief Neither display path can be initialized on the active graphics context. */
class UnsupportedDisplay final : public std::runtime_error
{
public:
    explicit UnsupportedDisplay(const std::string& reason) : std::runtime_error(reason) {}
};
/** @brief Owns a complete triple-buffer generation and its chosen display resources.
 * @note Shared handles are published only after construction succeeds. CPU mode is sticky
 * within a generation; fallback never changes the producer's textures or protocol.
 * @pre Dimensions and format satisfy IsValidStreamConfiguration; the GL context is current.
 */
class Generation
{
public:
    Generation(const GlInterop& api, ID3D11Device* device, std::uint32_t width,
               std::uint32_t height, std::uint32_t format, std::uint64_t number);
    ~Generation();
    Generation(const Generation&) = delete;
    Generation& operator=(const Generation&) = delete;
    /** @brief Locks the completed direct surface, or switches to CPU on a WGL lock failure.
     * @pre No direct surface is locked; the producer has completed this slot.
     */
    void Acquire(std::uint32_t index, ID3D11Device* device);
    void Unlock();
    bool UsesCpu() const noexcept { return readback_ != nullptr; }
    void BeginCopy(ID3D11DeviceContext* context, std::uint32_t index);
    bool CopyReady(ID3D11DeviceContext* context) const;
    bool UploadCopy(ID3D11DeviceContext* context);
    GLuint Texture() const noexcept;
    std::uint32_t Slot() const noexcept { return locked_; }
    std::uint32_t Width() const noexcept { return width_; }
    std::uint32_t Height() const noexcept { return height_; }
    std::uint64_t Number() const noexcept { return number_; }
    std::uint64_t Handle(std::size_t index) const noexcept { return handles_[index]; }
    const std::string& Diagnostic() const noexcept { return diagnostic_; }
private:
    bool InitializeInterop(ID3D11Device* device);
    void EnableCpu(ID3D11Device* device);
    void ReleaseInterop(bool checkErrors);
    void SetTextureParameters(std::uint32_t index);
    MemoryReservation memory_;
    GlInterop api_;
    std::array<ComPtr<ID3D11Texture2D>, kSlotCount> textures_;
    std::array<GlTexture, kSlotCount> names_;
    std::array<HANDLE, kSlotCount> objects_ {};
    std::array<std::uint64_t, kSlotCount> handles_ {};
    std::unique_ptr<CpuReadback> readback_;
    HANDLE bridge_ = nullptr;
    std::uint32_t width_, height_, format_;
    std::uint64_t number_;
    std::uint32_t locked_ = kNoSlot;
    bool interopRejected_ = false;
    std::string diagnostic_;
};
} // namespace mfd::stream::detail
#endif
