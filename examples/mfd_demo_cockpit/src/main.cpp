/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "mfd/window/WindowLauncher.h"

int main(int argc, char** argv)
{
    mfd::window::LauncherConfig config;
    config.applicationName = "mfd_demo_cockpit";
    config.defaultWindowFile = "assets/windows/demo_pages_cockpit.json";
    return mfd::window::RunLauncher(argc, argv, config);
}
