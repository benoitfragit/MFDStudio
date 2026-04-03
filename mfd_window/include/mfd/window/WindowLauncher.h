/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include <filesystem>
#include <string>

namespace mfd::window
{
struct LauncherConfig
{
    std::string applicationName = "mfd_window";
    std::filesystem::path defaultWindowFile = "assets/windows/demo_pages.json";
};

struct LauncherOptions
{
    std::filesystem::path windowFile;
    bool showHelp = false;
};

std::string BuildUsageText(const LauncherConfig& config);
bool ParseLauncherCommandLine(int argc,
                              char** argv,
                              const LauncherConfig& config,
                              LauncherOptions& options,
                              std::string& error);
int RunLauncher(int argc, char** argv, const LauncherConfig& config = {});
} // namespace mfd::window
