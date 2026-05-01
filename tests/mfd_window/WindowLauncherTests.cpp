/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for WindowLauncherTests.
 */

#include <array>
#include <cstddef>
#include <gtest/gtest.h>

#include <string>

#include "mfd/window/WindowLauncher.h"

/**
 * @brief Verifies custom config values are reflected in usage text.
 */
TEST(WindowLauncherTests, BuildUsageTextReflectsConfiguredApplicationAndWindow)
{
    mfd::window::LauncherConfig config;
    config.applicationName = "cockpit_host";
    config.defaultWindowFile = "assets/windows/demo_pages_cockpit.json";

    const std::string usage = mfd::window::BuildUsageText(config);

    EXPECT_NE(usage.find("cockpit_host --window <window.json>"), std::string::npos);
    EXPECT_NE(usage.find("assets/windows/demo_pages_cockpit.json"), std::string::npos);
    EXPECT_NE(usage.find("--framebuffer-plugin <plugin.dll>"), std::string::npos);
    EXPECT_NE(usage.find(mfd::window::kLauncherFramebufferPluginEntryPointName), std::string::npos);
    EXPECT_NE(usage.find("F1 toggles the integrated runtime debug overlay"), std::string::npos);
    EXPECT_NE(usage.find("1..9 activate the first nine authored pages"), std::string::npos);
}

/**
 * @brief Ensures usage text explains that a path is required when no default is configured.
 */
TEST(WindowLauncherTests, BuildUsageTextRequiresExplicitWindowWhenNoDefaultIsConfigured)
{
    mfd::window::LauncherConfig config;
    config.applicationName.clear();
    config.defaultWindowFile.clear();

    const std::string usage = mfd::window::BuildUsageText(config);

    EXPECT_NE(usage.find("mfd_window <window.json>"), std::string::npos);
    EXPECT_NE(usage.find("No default window JSON is configured"), std::string::npos);
    EXPECT_NE(usage.find("--framebuffer-plugin <plugin.dll>"), std::string::npos);
}

/**
 * @brief Parses no arguments and keeps the configured default JSON path.
 */
TEST(WindowLauncherTests, ParseCommandLineUsesConfiguredDefaultWindow)
{
    mfd::window::LauncherConfig config;
    config.defaultWindowFile = "assets/windows/demo_pages_minimal.json";

    char program[] = "mfd_window";
    char* argv[] = {program, nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(1, argv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_FALSE(options.showHelp);
    EXPECT_EQ(options.windowFile.generic_string(), "assets/windows/demo_pages_minimal.json");
    EXPECT_TRUE(options.framebufferPluginFile.empty());
}

/**
 * @brief Parses the explicit --window long option.
 */
TEST(WindowLauncherTests, ParseCommandLineAcceptsWindowFlagAndHelp)
{
    mfd::window::LauncherConfig config;

    char program[] = "mfd_window";
    char flag[] = "--window";
    char windowPath[] = "custom/window.json";
    char* argv[] = {program, flag, windowPath, nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(3, argv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(options.windowFile.generic_string(), "custom/window.json");
    EXPECT_FALSE(options.showHelp);

    char help[] = "--help";
    char* helpArgv[] = {program, help, nullptr};
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(2, helpArgv, config, options, error));
    EXPECT_TRUE(options.showHelp);
}

/**
 * @brief Parses the framebuffer plugin option independently from the window path.
 */
TEST(WindowLauncherTests, ParseCommandLineAcceptsFramebufferPluginFlag)
{
    mfd::window::LauncherConfig config;

    char program[] = "mfd_window";
    char windowFlag[] = "--window";
    char windowPath[] = "custom/window.json";
    char pluginFlag[] = "--framebuffer-plugin";
    char pluginPath[] = "plugins/framebuffer.dll";
    char* argv[] = {program, windowFlag, windowPath, pluginFlag, pluginPath, nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(5, argv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(options.windowFile.generic_string(), "custom/window.json");
    EXPECT_EQ(options.framebufferPluginFile.generic_string(), "plugins/framebuffer.dll");
    EXPECT_FALSE(options.showHelp);
}

/**
 * @brief Parses short aliases -w, -p and -h as valid options.
 */
TEST(WindowLauncherTests, ParseCommandLineAcceptsShortAliases)
{
    mfd::window::LauncherConfig config;

    char program[] = "mfd_window";
    char shortWindow[] = "-w";
    char windowPath[] = "short/path.json";
    char* windowArgv[] = {program, shortWindow, windowPath, nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(3, windowArgv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(options.windowFile.generic_string(), "short/path.json");
    EXPECT_TRUE(options.framebufferPluginFile.empty());
    EXPECT_FALSE(options.showHelp);

    char shortHelp[] = "-h";
    char* helpArgv[] = {program, shortHelp, nullptr};
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(2, helpArgv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(options.showHelp);

    char shortPlugin[] = "-p";
    char pluginPath[] = "short/plugin.dll";
    char* pluginArgv[] = {program, shortWindow, windowPath, shortPlugin, pluginPath, nullptr};
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(5, pluginArgv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(options.framebufferPluginFile.generic_string(), "short/plugin.dll");
    EXPECT_EQ(options.windowFile.generic_string(), "short/path.json");
}

/**
 * @brief Supports one positional window path argument.
 */
TEST(WindowLauncherTests, ParseCommandLineAcceptsPositionalWindowPath)
{
    mfd::window::LauncherConfig config;

    char program[] = "mfd_window";
    char positional[] = "assets/windows/radar.json";
    char* argv[] = {program, positional, nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(2, argv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(options.windowFile.generic_string(), "assets/windows/radar.json");
    EXPECT_TRUE(options.framebufferPluginFile.empty());
    EXPECT_FALSE(options.showHelp);
}

/**
 * @brief Prefers the last explicit path when positional and --window are mixed.
 */
TEST(WindowLauncherTests, ParseCommandLineAllowsMixingPositionalAndWindowFlag)
{
    mfd::window::LauncherConfig config;

    char program[] = "mfd_window";
    char positional[] = "first.json";
    char flag[] = "--window";
    char explicitPath[] = "second.json";
    char* argv[] = {program, positional, flag, explicitPath, nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(4, argv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(options.windowFile.generic_string(), "second.json");
}

/**
 * @brief Rejects a missing window path when the launcher has no configured default.
 */
TEST(WindowLauncherTests, ParseCommandLineRejectsMissingWindowWhenNoDefaultIsConfigured)
{
    mfd::window::LauncherConfig config;

    char program[] = "mfd_window";
    char* argv[] = {program, nullptr, nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_FALSE(mfd::window::ParseLauncherCommandLine(2, argv, config, options, error));
    EXPECT_NE(error.find("No window JSON path was provided"), std::string::npos);
}

/**
 * @brief Rejects malformed command line combinations and reports precise errors.
 */
TEST(WindowLauncherTests, ParseCommandLineRejectsInvalidArgumentCombinations)
{
    mfd::window::LauncherConfig config;
    mfd::window::LauncherOptions options;
    std::string error;

    char program[] = "mfd_window";
    char unknown[] = "--bad";
    char* unknownArgv[] = {program, unknown, nullptr};
    EXPECT_FALSE(mfd::window::ParseLauncherCommandLine(2, unknownArgv, config, options, error));
    EXPECT_NE(error.find("Unknown option"), std::string::npos);

    char windowFlag[] = "--window";
    char* missingArgv[] = {program, windowFlag, nullptr};
    error.clear();
    EXPECT_FALSE(mfd::window::ParseLauncherCommandLine(2, missingArgv, config, options, error));
    EXPECT_NE(error.find("Missing path"), std::string::npos);

    char pluginFlag[] = "--framebuffer-plugin";
    char* missingPluginArgv[] = {program, pluginFlag, nullptr};
    error.clear();
    EXPECT_FALSE(mfd::window::ParseLauncherCommandLine(2, missingPluginArgv, config, options, error));
    EXPECT_NE(error.find("Missing path"), std::string::npos);

    char first[] = "one.json";
    char second[] = "two.json";
    char* extraArgv[] = {program, first, second, nullptr};
    error.clear();
    EXPECT_FALSE(mfd::window::ParseLauncherCommandLine(3, extraArgv, config, options, error));
    EXPECT_NE(error.find("Only one window JSON path"), std::string::npos);
}

/**
 * @brief Confirms the launcher can keep the framebuffer path completely disabled.
 */
TEST(WindowLauncherTests, FramebufferCallbackCanRemainEmpty)
{
    mfd::window::LauncherFramebufferCallback callback;
    EXPECT_FALSE(static_cast<bool>(callback));
}

/**
 * @brief Validates the framebuffer callback signature can observe dimensions and byte span size.
 */
TEST(WindowLauncherTests, FramebufferCallbackReceivesDimensionsAndByteSpan)
{
    int receivedWidth = 0;
    int receivedHeight = 0;
    std::size_t receivedByteCount = 0;

    mfd::window::LauncherFramebufferCallback callback =
        [&](const int width, const int height, const mfd::ByteView rgba32Bytes)
    {
        receivedWidth = width;
        receivedHeight = height;
        receivedByteCount = rgba32Bytes.size();
    };

    const std::array<std::byte, 16> pixels {};
    callback(2, 2, pixels);

    EXPECT_EQ(receivedWidth, 2);
    EXPECT_EQ(receivedHeight, 2);
    EXPECT_EQ(receivedByteCount, pixels.size());
}
