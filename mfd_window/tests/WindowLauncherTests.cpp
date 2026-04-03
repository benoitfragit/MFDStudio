/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include <array>
#include <cstddef>
#include <span>
#include <gtest/gtest.h>

#include <string>

#include "mfd/window/WindowLauncher.h"

TEST(WindowLauncherTests, BuildUsageTextReflectsConfiguredApplicationAndWindow)
{
    mfd::window::LauncherConfig config;
    config.applicationName = "cockpit_host";
    config.defaultWindowFile = "assets/windows/demo_pages_cockpit.json";

    const std::string usage = mfd::window::BuildUsageText(config);

    EXPECT_NE(usage.find("cockpit_host --window <window.json>"), std::string::npos);
    EXPECT_NE(usage.find("assets/windows/demo_pages_cockpit.json"), std::string::npos);
    EXPECT_NE(usage.find("1..9 activate the first nine authored pages"), std::string::npos);
}

TEST(WindowLauncherTests, ParseCommandLineUsesConfiguredDefaultWindow)
{
    mfd::window::LauncherConfig config;
    config.defaultWindowFile = "assets/windows/demo_pages_minimal.json";

    char program[] = "mfd_demo_minimal";
    char* argv[] = {program, nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(1, argv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_FALSE(options.showHelp);
    EXPECT_EQ(options.windowFile.generic_string(), "assets/windows/demo_pages_minimal.json");
}

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

    char first[] = "one.json";
    char second[] = "two.json";
    char* extraArgv[] = {program, first, second, nullptr};
    error.clear();
    EXPECT_FALSE(mfd::window::ParseLauncherCommandLine(3, extraArgv, config, options, error));
    EXPECT_NE(error.find("Only one window JSON path"), std::string::npos);
}

TEST(WindowLauncherTests, FramebufferCallbackReceivesDimensionsAndByteSpan)
{
    int receivedWidth = 0;
    int receivedHeight = 0;
    std::size_t receivedByteCount = 0;

    mfd::window::LauncherFramebufferCallback callback =
        [&](const int width, const int height, const std::span<const std::byte> rgba32Bytes)
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
