/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include "mfd/window/WindowLauncherPlugin.h"
#include <string>

namespace mfd::window
{
struct LauncherConfig
{
    std::string applicationName = "mfd_window";
    std::filesystem::path defaultWindowFile {};
};

/**
 * @brief Optional callback receiving the final RGBA32 window buffer once per frame.
 *
 * The byte span is only valid during the callback invocation. Copy it if the
 * data must outlive the call.
 */
using LauncherFramebufferCallback = std::function<void(int width,
                                                       int height,
                                                       mfd::ByteView rgba32Bytes)>;

struct LauncherOptions
{
    /** @brief Resolved root window JSON file to open. */
    std::filesystem::path windowFile;
    /** @brief Optional framebuffer plugin DLL passed from the command line. */
    std::filesystem::path framebufferPluginFile;
    /** @brief Whether the caller requested the usage text. */
    bool showHelp = false;
};

/**
 * @brief Builds the human-readable usage text for one launcher configuration.
 * @param config Launcher defaults such as application name and default window.
 * @return Usage text ready to print to stdout or stderr.
 */
std::string BuildUsageText(const LauncherConfig& config);

/**
 * @brief Parses the generic launcher command line.
 * @param argc Command-line argument count.
 * @param argv Command-line argument values.
 * @param config Launcher defaults such as application name and default window.
 * @param options Parsed options written on success.
 * @param error Error message written on failure.
 * @return `true` on success, otherwise `false`.
 */
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
 * `RGBA32` window buffer as raw bytes. Leave it empty to disable all
 * framebuffer capture logic in the launcher.
 */
int RunLauncher(int argc,
                char** argv,
                const LauncherConfig& config = {},
                LauncherFramebufferCallback framebufferCallback = {});
} // namespace mfd::window
