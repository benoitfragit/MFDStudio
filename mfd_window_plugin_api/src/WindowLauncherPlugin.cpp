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

namespace
{
[[nodiscard]] size_t FramebufferBytesPerPixel(const uint32_t pixelFormat) noexcept
{
    switch (static_cast<MfdWindowFramebufferPixelFormat>(pixelFormat))
    {
    case MfdWindowFramebufferPixelFormat_Rgba32:
    case MfdWindowFramebufferPixelFormat_Bgra32:
        return 4U;
    default:
        return 0U;
    }
}
} // namespace

extern "C"
{
MFD_WINDOW_PLUGIN_API size_t MFD_WINDOW_PLUGIN_CALL MfdWindowComputeFramebufferByteCount(
    const uint32_t pixel_format,
    const int32_t width,
    const int32_t height) noexcept
{
    if (width <= 0 || height <= 0)
    {
        return 0;
    }

    const size_t bytesPerPixel = FramebufferBytesPerPixel(pixel_format);
    if (bytesPerPixel == 0U)
    {
        return 0;
    }

    const size_t widthBytes = static_cast<size_t>(width);
    const size_t heightBytes = static_cast<size_t>(height);

    if (widthBytes > std::numeric_limits<size_t>::max() / heightBytes)
    {
        return 0;
    }

    const size_t pixelCount = widthBytes * heightBytes;
    if (pixelCount > std::numeric_limits<size_t>::max() / bytesPerPixel)
    {
        return 0;
    }

    return pixelCount * bytesPerPixel;
}

MFD_WINDOW_PLUGIN_API int MFD_WINDOW_PLUGIN_CALL MfdWindowValidateFramebufferLayout(
    const uint32_t pixel_format,
    const int32_t width,
    const int32_t height,
    const void* pixels,
    const size_t pixel_bytes) noexcept
{
    const size_t expectedByteCount = MfdWindowComputeFramebufferByteCount(pixel_format, width, height);
    return expectedByteCount != 0U && pixels != nullptr && pixel_bytes == expectedByteCount ? 1 : 0;
}

MFD_WINDOW_PLUGIN_API size_t MFD_WINDOW_PLUGIN_CALL MfdWindowComputeFramebufferRgba32ByteCount(
    const int32_t width,
    const int32_t height) noexcept
{
    return MfdWindowComputeFramebufferByteCount(MfdWindowFramebufferPixelFormat_Rgba32, width, height);
}

MFD_WINDOW_PLUGIN_API int MFD_WINDOW_PLUGIN_CALL MfdWindowValidateFramebufferRgba32Layout(
    const int32_t width,
    const int32_t height,
    const void* pixels,
    const size_t pixel_bytes) noexcept
{
    return MfdWindowValidateFramebufferLayout(
        MfdWindowFramebufferPixelFormat_Rgba32,
        width,
        height,
        pixels,
        pixel_bytes);
}

MFD_WINDOW_PLUGIN_API int MFD_WINDOW_PLUGIN_CALL MfdWindowValidateFramebufferFrame(
    const MfdWindowFramebufferFrame* frame) noexcept
{
    if (frame == nullptr || frame->struct_size < sizeof(MfdWindowFramebufferFrame))
    {
        return 0;
    }

    const size_t bytesPerPixel = FramebufferBytesPerPixel(frame->pixel_format);
    if (bytesPerPixel == 0U)
    {
        return 0;
    }

    const size_t expectedByteCount = MfdWindowComputeFramebufferByteCount(frame->pixel_format, frame->width, frame->height);
    if (expectedByteCount == 0 || frame->pixels == nullptr || frame->pixel_bytes != expectedByteCount)
    {
        return 0;
    }

    const size_t expectedRowStride = static_cast<size_t>(frame->width) * bytesPerPixel;
    return frame->row_stride_bytes == expectedRowStride ? 1 : 0;
}
} // extern "C"
