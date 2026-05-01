/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the framebuffer-plugin ABI helpers.
 */

#include "mfd/window/WindowLauncherPlugin.h"

#include <limits>

namespace mfd::window
{
std::size_t ComputeFramebufferRgba32ByteCount(const int width, const int height) noexcept
{
    if (width <= 0 || height <= 0)
    {
        return 0;
    }

    const std::size_t widthBytes = static_cast<std::size_t>(width);
    const std::size_t heightBytes = static_cast<std::size_t>(height);
    constexpr std::size_t kBytesPerPixel = 4U;

    if (widthBytes > std::numeric_limits<std::size_t>::max() / heightBytes)
    {
        return 0;
    }

    const std::size_t pixelCount = widthBytes * heightBytes;
    if (pixelCount > std::numeric_limits<std::size_t>::max() / kBytesPerPixel)
    {
        return 0;
    }

    return pixelCount * kBytesPerPixel;
}

bool ValidateFramebufferRgba32Layout(const int width,
                                     const int height,
                                     const mfd::ByteView rgba32Bytes) noexcept
{
    const std::size_t expectedByteCount = ComputeFramebufferRgba32ByteCount(width, height);
    return expectedByteCount != 0 && rgba32Bytes.size() == expectedByteCount;
}
} // namespace mfd::window
