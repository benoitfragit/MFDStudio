/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "AsyncFramebufferCapture.h"
#include <algorithm>
#include <cstring>
namespace mfd::window::detail
{
namespace
{
void FlipRows(std::vector<std::byte>& pixels, const std::size_t rowStrideBytes, const int height)
{
    if (rowStrideBytes == 0U || height <= 1)
    {
        return;
    }

    std::vector<std::byte> scratchRow(rowStrideBytes);
    for (int top = 0, bottom = height - 1; top < bottom; ++top, --bottom)
    {
        std::byte* topRow = pixels.data() + static_cast<std::size_t>(top) * rowStrideBytes;
        std::byte* bottomRow = pixels.data() + static_cast<std::size_t>(bottom) * rowStrideBytes;
        std::memcpy(scratchRow.data(), topRow, rowStrideBytes);
        std::memcpy(topRow, bottomRow, rowStrideBytes);
        std::memcpy(bottomRow, scratchRow.data(), rowStrideBytes);
    }
}

}
FenceStatus DecodeFenceStatus(const unsigned int status) noexcept
{
    // OpenGL sync status constants (the backend may use dynamically loaded GL).
    switch (status)
    {
    case 0x911A: // GL_ALREADY_SIGNALED
    case 0x911C: // GL_CONDITION_SATISFIED
        return FenceStatus::Ready;
    case 0x911B: // GL_TIMEOUT_EXPIRED
        return FenceStatus::Pending;
    default: // GL_WAIT_FAILED, missing sync, or unexpected driver response
        return FenceStatus::Failed;
    }
}

AsyncFramebufferCapture::AsyncFramebufferCapture(const MfdWindowFramebufferPixelFormat pixelFormat,
                                     const FramebufferReadbackApi& api) noexcept :
        api_(api), pixelFormat_(pixelFormat)
{
}

AsyncFramebufferCapture::~AsyncFramebufferCapture()
{
    if (api_.ContextReady())
    {
        Release();
    }
}

std::optional<CapturedFramebuffer> AsyncFramebufferCapture::Capture(const int captureWidth, const int captureHeight)
{
    if (!api_.ContextReady() || captureWidth <= 0 || captureHeight <= 0 ||
        (pixelFormat_ != MfdWindowFramebufferPixelFormat_Rgba32 && pixelFormat_ != MfdWindowFramebufferPixelFormat_Bgra32))
    {
        return std::nullopt;
    }

    if (!pboSupportChecked_)
    {
        pboSupportChecked_ = true;
        pboAvailable_ = api_.Available();
    }

    if (!pboAvailable_)
    {
        return CaptureSynchronously(captureWidth, captureHeight);
    }

    if (!EnsureResources(captureWidth, captureHeight))
    {
        DisablePbo();
        return CaptureSynchronously(captureWidth, captureHeight);
    }

    api_.FlushRendering();

    const unsigned int writeIndex = frameIndex_ % kCaptureRingSize;
    // cppcheck-suppress knownConditionTrueFalse
    if (!SubmitReadback(writeIndex, captureWidth, captureHeight))
    {
        DisablePbo();
        return CaptureSynchronously(captureWidth, captureHeight);
    }

    std::optional<CapturedFramebuffer> readyFrame;
    const unsigned int readIndex = (frameIndex_ + 1) % kCaptureRingSize;
    readyFrame = TryConsume(readIndex);
    if (!pboAvailable_)
    {
        return CaptureSynchronously(captureWidth, captureHeight);
    }

    frameIndex_ = readIndex;

    return readyFrame;
}

void AsyncFramebufferCapture::CaptureSlot::ReleaseFence(const FramebufferReadbackApi& api) noexcept
{
    if (fence != nullptr)
    {
        api.DeleteFence(fence);
        fence = nullptr;
    }
}

void AsyncFramebufferCapture::CaptureSlot::ResetMetadata() noexcept
{
    capacityBytes = 0;
    width = 0;
    height = 0;
}

std::optional<CapturedFramebuffer> AsyncFramebufferCapture::CaptureSynchronously(const int captureWidth,
                                                                         const int captureHeight)
{
    CapturedFramebuffer frame = api_.ReadSynchronously(captureWidth, captureHeight, pixelFormat_);
    if (frame.Empty())
    {
        return std::nullopt;
    }
    return frame;
}

void AsyncFramebufferCapture::DisablePbo() noexcept
{
    Release();
    pboAvailable_ = false;
}

bool AsyncFramebufferCapture::EnsureResources(const int width, const int height)
{
    if (!pboAvailable_)
    {
        return false;
    }

    const FramebufferReadbackApi& api = api_;
    const std::size_t requiredBytes = mfd::window::ComputeFramebufferByteCount(pixelFormat_, width, height);
    if (requiredBytes == 0)
    {
        return false;
    }

    if (!initialized_)
    {
        for (CaptureSlot& slot : slots_)
        {
            api.GenBuffers(1, &slot.pboId);
            if (slot.pboId == 0)
            {
                Release();
                return false;
            }
        }
        initialized_ = true;
        frameIndex_ = 0;
    }

    for (CaptureSlot& slot : slots_)
    {
        if (slot.capacityBytes == requiredBytes && slot.width == width && slot.height == height)
        {
            continue;
        }

        slot.ReleaseFence(api);
        api.BindPixelPackBuffer(slot.pboId);
        api.AllocatePixelPackBuffer(requiredBytes);
        api.BindPixelPackBuffer(0);
        slot.capacityBytes = requiredBytes;
        slot.width = width;
        slot.height = height;
    }

    return true;
}

bool AsyncFramebufferCapture::SubmitReadback(const unsigned int slotIndex, const int width, const int height)
{
    const FramebufferReadbackApi& api = api_;
    CaptureSlot& slot = slots_[slotIndex];
    if (slot.pboId == 0 || slot.capacityBytes == 0 || slot.width != width || slot.height != height)
    {
        return false;
    }

    if (slot.fence != nullptr)
    {
        return true;
    }
    api.BindPixelPackBuffer(slot.pboId);
    api.ReadPixels(width, height, pixelFormat_);
    api.BindPixelPackBuffer(0);

    slot.fence = api.CreateFence();
    // cppcheck-suppress knownConditionTrueFalse
    return slot.fence != nullptr;
}

std::optional<CapturedFramebuffer> AsyncFramebufferCapture::TryConsume(const unsigned int slotIndex)
{
    const FramebufferReadbackApi& api = api_;
    CaptureSlot& slot = slots_[slotIndex];
    if (slot.pboId == 0 || slot.capacityBytes == 0 || slot.fence == nullptr)
    {
        return std::nullopt;
    }

    const FenceStatus status = api.PollFence(slot.fence);
    if (status == FenceStatus::Pending)
    {
        return std::nullopt;
    }
    if (status == FenceStatus::Failed)
    {
        api.ReportFailure();
        DisablePbo();
        return std::nullopt;
    }

    latestFramebuffer_.pixels.resize(slot.capacityBytes);
    api.BindPixelPackBuffer(slot.pboId);
    void* mappedBuffer = api.MapPixelPackBufferReadOnly();
    // cppcheck-suppress knownConditionTrueFalse
    if (mappedBuffer == nullptr)
    {
        api.BindPixelPackBuffer(0);
        slot.ReleaseFence(api);
        DisablePbo();
        return std::nullopt;
    }

    latestFramebuffer_.width = slot.width;
    latestFramebuffer_.height = slot.height;
    latestFramebuffer_.pixelFormat = pixelFormat_;
    std::memcpy(latestFramebuffer_.pixels.data(), mappedBuffer, slot.capacityBytes);

    const bool unmapSucceeded = api.UnmapPixelPackBuffer();
    api.BindPixelPackBuffer(0);
    slot.ReleaseFence(api);
    // cppcheck-suppress knownConditionTrueFalse
    if (!unmapSucceeded)
    {
        DisablePbo();
        return std::nullopt;
    }

    FlipRows(latestFramebuffer_.pixels,
             mfd::window::ComputeFramebufferByteCount(pixelFormat_, latestFramebuffer_.width, 1),
             latestFramebuffer_.height);
    return latestFramebuffer_;
}

void AsyncFramebufferCapture::Release() noexcept
{
    const FramebufferReadbackApi& api = api_;
    for (CaptureSlot& slot : slots_)
    {
        slot.ReleaseFence(api);
        if (slot.pboId != 0)
        {
            api.DeleteBuffers(1, &slot.pboId);
            slot.pboId = 0;
        }

        slot.ResetMetadata();
    }

    initialized_ = false;
    frameIndex_ = 0;
    latestFramebuffer_ = {};
}
}
