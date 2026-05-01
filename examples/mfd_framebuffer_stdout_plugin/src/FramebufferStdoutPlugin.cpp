/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Sample framebuffer plugin printing one confirmation line on first frame.
 */

#include <iostream>

#include "mfd/window/WindowLauncherPlugin.h"

/**
 * @brief Sample plugin entry point discovered by `mfd_window --framebuffer-plugin`.
 * @param width Framebuffer width in pixels.
 * @param height Framebuffer height in pixels.
 * @param pixels Temporary `RGBA32` byte span for the current frame.
 */
extern "C" __declspec(dllexport) void MfdWindowFramebufferCallback(
    const int width,
    const int height,
    const mfd::ByteView pixels)
{
    if (!mfd::window::ValidateFramebufferRgba32Layout(width, height, pixels))
    {
        std::cerr << "Unexpected framebuffer layout received from mfd_window.\n";
        return;
    }

    static bool printed = false;
    if (!printed)
    {
        std::cout << "RGBA32 framebuffer callback active: " << width << "x" << height
                  << " pixels=" << pixels.size() << '\n';
        printed = true;
    }
}
