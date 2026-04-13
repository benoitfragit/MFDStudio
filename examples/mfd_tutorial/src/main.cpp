/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include <cstddef>
#include <iostream>
#include <span>

#include "mfd/window/WindowLauncher.h"

int main(int argc, char** argv)
{
    mfd::window::LauncherConfig config;
    config.applicationName = "mfd_tutorial";
    config.defaultWindowFile = "assets/windows/mfd_tutorial.json";
    return mfd::window::RunLauncher(
        argc,
        argv,
        config,
        [](int width, int height, std::span<const std::byte> pixels)
        {
            static bool printed = false;
            if (!printed)
            {
                std::cout << "RGBA32 framebuffer callback active: " << width << "x" << height
                          << " pixels=" << pixels.size() << '\n';
                printed = true;
            }
        });
}
