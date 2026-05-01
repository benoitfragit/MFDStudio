/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "mfd/window/WindowLauncherPlugin.h"

#include <vector>

extern "C" __declspec(dllexport) void MfdWindowFramebufferCallback(
    const int width,
    const int height,
    const mfd::ByteView pixels)
{
    if (!mfd::window::ValidateFramebufferRgba32Layout(width, height, pixels))
    {
        return;
    }

    const std::size_t byte_count = mfd::window::ComputeFramebufferRgba32ByteCount(width, height);
    std::vector<std::byte> copy(pixels.begin(), pixels.end());
    if (copy.size() != byte_count)
    {
        copy.clear();
    }
}
