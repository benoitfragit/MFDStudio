/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <span>
#include <string>

namespace mfd::window
{
struct LauncherConfig
{
    std::string applicationName = "mfd_window";
    std::filesystem::path defaultWindowFile = "assets/windows/demo_pages.json";
};

/**
 * @brief Optional callback receiving the final RGBA32 window buffer once per frame.
 *
 * The byte span is only valid during the callback invocation. Copy it if the
 * data must outlive the call.
 */
using LauncherFramebufferCallback = std::function<void(int width,
                                                       int height,
                                                       std::span<const std::byte> rgba32Bytes)>;

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
/**
 * @brief Runs the generic window launcher.
 * @param argc Command-line argument count.
 * @param argv Command-line argument values.
 * @param config Launcher defaults such as application name and default window.
 * @param framebufferCallback Optional per-frame callback receiving the final
 * `RGBA32` window buffer as raw bytes.
 */
int RunLauncher(int argc,
                char** argv,
                const LauncherConfig& config = {},
                LauncherFramebufferCallback framebufferCallback = {});
} // namespace mfd::window
