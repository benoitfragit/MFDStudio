/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the stable framebuffer plugin ABI helpers.
 */

#include "mfd/window/WindowLauncherPlugin.h"

#include <limits>

extern "C"
{
MFD_WINDOW_PLUGIN_API size_t MFD_WINDOW_PLUGIN_CALL MfdWindowComputeFramebufferRgba32ByteCount(
    const int32_t width,
    const int32_t height) noexcept
{
    if (width <= 0 || height <= 0)
    {
        return 0;
    }

    const size_t widthBytes = static_cast<size_t>(width);
    const size_t heightBytes = static_cast<size_t>(height);
    constexpr size_t kBytesPerPixel = 4U;

    if (widthBytes > std::numeric_limits<size_t>::max() / heightBytes)
    {
        return 0;
    }

    const size_t pixelCount = widthBytes * heightBytes;
    if (pixelCount > std::numeric_limits<size_t>::max() / kBytesPerPixel)
    {
        return 0;
    }

    return pixelCount * kBytesPerPixel;
}

MFD_WINDOW_PLUGIN_API int MFD_WINDOW_PLUGIN_CALL MfdWindowValidateFramebufferRgba32Layout(
    const int32_t width,
    const int32_t height,
    const void* pixels,
    const size_t pixel_bytes) noexcept
{
    const size_t expectedByteCount = MfdWindowComputeFramebufferRgba32ByteCount(width, height);
    return expectedByteCount != 0 && pixels != nullptr && pixel_bytes == expectedByteCount ? 1 : 0;
}

MFD_WINDOW_PLUGIN_API int MFD_WINDOW_PLUGIN_CALL MfdWindowValidateFramebufferFrame(
    const MfdWindowFramebufferFrame* frame) noexcept
{
    if (frame == nullptr || frame->struct_size < sizeof(MfdWindowFramebufferFrame) ||
        frame->pixel_format != MfdWindowFramebufferPixelFormat_Rgba32)
    {
        return 0;
    }

    const size_t expectedByteCount = MfdWindowComputeFramebufferRgba32ByteCount(frame->width, frame->height);
    if (expectedByteCount == 0 || frame->pixels == nullptr || frame->pixel_bytes != expectedByteCount)
    {
        return 0;
    }

    const size_t expectedRowStride = expectedByteCount / static_cast<size_t>(frame->height);
    return frame->row_stride_bytes == expectedRowStride ? 1 : 0;
}
} // extern "C"
