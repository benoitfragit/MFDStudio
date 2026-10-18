/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once
/** @file @brief Private capture seam; no dependency on a live GL context in regression tests. */
#include <array>
#include <cstddef>
#include <optional>
#include <vector>
#include "mfd/window/WindowLauncherPlugin.h"
#include "mfd/core/ArrayView.h"

namespace mfd::window::detail
{
using GlSyncHandle = void*;
enum class FenceStatus { Ready, Pending, Failed };
/** @brief Converts the OpenGL client-wait status without conflating timeout and failure. */
FenceStatus DecodeFenceStatus(unsigned int status) noexcept;

/** @brief Owned pixels from one completed framebuffer capture. */
struct CapturedFramebuffer
{
    int width = 0;
    int height = 0;
    MfdWindowFramebufferPixelFormat pixelFormat = MfdWindowFramebufferPixelFormat_Rgba32;
    std::vector<std::byte> pixels;

    [[nodiscard]] bool Empty() const noexcept
    {
        return pixels.empty();
    }

    [[nodiscard]] std::size_t ByteSize() const noexcept
    {
        return pixels.size();
    }

    [[nodiscard]] mfd::ByteView Bytes() const noexcept
    {
        return {pixels.data(), pixels.size()};
    }
};

/** @brief Borrowed, context-bound readback backend; outlives every capture using it. */
class FramebufferReadbackApi
{
public:
    virtual ~FramebufferReadbackApi() = default;
    virtual bool ContextReady() const noexcept = 0;
    virtual bool Available() const noexcept = 0;
    virtual void FlushRendering() const = 0;
    virtual CapturedFramebuffer ReadSynchronously(int width, int height, MfdWindowFramebufferPixelFormat format) const = 0;
    virtual void ReportFailure() const noexcept = 0;
    virtual void GenBuffers(int count, unsigned int* buffers) const noexcept = 0;
    virtual void DeleteBuffers(int count, const unsigned int* buffers) const noexcept = 0;
    virtual void BindPixelPackBuffer(unsigned int bufferId) const noexcept = 0;
    virtual void AllocatePixelPackBuffer(std::size_t sizeInBytes) const noexcept = 0;
    virtual void ReadPixels(int width, int height, MfdWindowFramebufferPixelFormat pixelFormat) const noexcept = 0;
    virtual void* MapPixelPackBufferReadOnly() const noexcept = 0;
    virtual bool UnmapPixelPackBuffer() const noexcept = 0;
    virtual GlSyncHandle CreateFence() const noexcept = 0;
    virtual FenceStatus PollFence(GlSyncHandle fence) const noexcept = 0;
    virtual void DeleteFence(GlSyncHandle fence) const noexcept = 0;
};

/** @brief Owns the capture ring; the borrowed backend and context outlive normal cleanup. */
class AsyncFramebufferCapture
{
public:
    explicit AsyncFramebufferCapture(const MfdWindowFramebufferPixelFormat pixelFormat,
                                     const FramebufferReadbackApi& api) noexcept;

    ~AsyncFramebufferCapture();
    AsyncFramebufferCapture(const AsyncFramebufferCapture&) = delete;
    AsyncFramebufferCapture& operator=(const AsyncFramebufferCapture&) = delete;

    [[nodiscard]] std::optional<CapturedFramebuffer> Capture(const int captureWidth, const int captureHeight);

private:
    struct CaptureSlot
    {
        unsigned int pboId = 0;
        std::size_t capacityBytes = 0;
        int width = 0;
        int height = 0;
        GlSyncHandle fence = nullptr;

        void ReleaseFence(const FramebufferReadbackApi& api) noexcept;

        void ResetMetadata() noexcept;
    };

    [[nodiscard]] std::optional<CapturedFramebuffer> CaptureSynchronously(const int captureWidth,
                                                                         const int captureHeight);

    void DisablePbo() noexcept;

    bool EnsureResources(const int width, const int height);

    bool SubmitReadback(const unsigned int slotIndex, const int width, const int height);

    [[nodiscard]] std::optional<CapturedFramebuffer> TryConsume(const unsigned int slotIndex);

    void Release() noexcept;

    static constexpr unsigned int kCaptureRingSize = 2;
    const FramebufferReadbackApi& api_;
    std::array<CaptureSlot, kCaptureRingSize> slots_ {};
    CapturedFramebuffer latestFramebuffer_ {};
    MfdWindowFramebufferPixelFormat pixelFormat_ = MfdWindowFramebufferPixelFormat_Rgba32;
    unsigned int frameIndex_ = 0;
    bool initialized_ = false;
    bool pboSupportChecked_ = false;
    bool pboAvailable_ = true;
};

}
