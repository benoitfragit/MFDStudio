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
#include <cstdint>
#include <gtest/gtest.h>

#include <string>

#include "mfd/window/WindowLauncher.h"
#include "mfd/window/WindowLauncherPlugin.h"
#include "WindowLauncherInternal.h"

/**
 * @brief Verifies custom config values are reflected in usage text.
 */
TEST(WindowLauncherTests, BuildUsageTextReflectsConfiguredApplicationAndWindow)
{
    mfd::window::LauncherConfig config;
    config.applicationName = "radar_host";
    config.defaultWindowFile = "examples/radar_load/assets/windows/radar_load_window.json";

    const std::string usage = mfd::window::BuildUsageText(config);

    EXPECT_NE(usage.find("radar_host --window <window.json>"), std::string::npos);
    EXPECT_NE(usage.find("examples/radar_load/assets/windows/radar_load_window.json"), std::string::npos);
    EXPECT_NE(usage.find("--framebuffer-plugin <plugin.dll>"), std::string::npos);
    EXPECT_NE(usage.find("--no-snapshot"), std::string::npos);
    EXPECT_NE(usage.find(mfd::window::kLauncherFramebufferPluginEntryPointName), std::string::npos);
    EXPECT_NE(usage.find("mfd_window parses only this launcher contract"), std::string::npos);
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
    EXPECT_NE(usage.find("mfd_window parses only this launcher contract"), std::string::npos);
}

/**
 * @brief Parses no arguments and keeps the configured default JSON path.
 */
TEST(WindowLauncherTests, ParseCommandLineUsesConfiguredDefaultWindow)
{
    mfd::window::LauncherConfig config;
    config.defaultWindowFile = "examples/demo/assets/windows/demo_window.json";

    char program[] = "mfd_window";
    char* argv[] = {program, nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(1, argv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_FALSE(options.showHelp);
    EXPECT_EQ(options.windowFile.generic_string(), "examples/demo/assets/windows/demo_window.json");
    EXPECT_TRUE(options.framebufferPluginFile.empty());
    EXPECT_FALSE(options.noSnapshot);
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
    EXPECT_FALSE(options.noSnapshot);

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
    EXPECT_FALSE(options.noSnapshot);
}

/**
 * @brief Allows plugin-specific arguments once a framebuffer plugin is configured.
 */
TEST(WindowLauncherTests, ParseCommandLineAcceptsPluginSpecificArgumentsWhenPluginIsConfigured)
{
    mfd::window::LauncherConfig config;

    char program[] = "mfd_window";
    char pluginOption[] = "--stdout-label";
    char pluginOptionValue[] = "Cockpit";
    char windowFlag[] = "--window";
    char windowPath[] = "custom/window.json";
    char pluginFlag[] = "--framebuffer-plugin";
    char pluginPath[] = "plugins/framebuffer.dll";
    char pluginPositional[] = "capture.raw";
    char* argv[] = {
        program,
        pluginOption,
        pluginOptionValue,
        windowFlag,
        windowPath,
        pluginFlag,
        pluginPath,
        pluginPositional,
        nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(8, argv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(options.windowFile.generic_string(), "custom/window.json");
    EXPECT_EQ(options.framebufferPluginFile.generic_string(), "plugins/framebuffer.dll");
    EXPECT_FALSE(options.showHelp);
    EXPECT_FALSE(options.noSnapshot);
}

/**
 * @brief Keeps the configured default window when an unknown plugin option has a value.
 */
TEST(WindowLauncherTests, ParseCommandLineKeepsDefaultWindowForPluginSpecificOptionValue)
{
    mfd::window::LauncherConfig config;
    config.defaultWindowFile = "default/window.json";

    char program[] = "mfd_window";
    char pluginOption[] = "--stdout-label";
    char pluginOptionValue[] = "Cockpit";
    char pluginFlag[] = "--framebuffer-plugin";
    char pluginPath[] = "plugins/framebuffer.dll";
    char* argv[] = {program, pluginOption, pluginOptionValue, pluginFlag, pluginPath, nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(5, argv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(options.windowFile.generic_string(), "default/window.json");
    EXPECT_EQ(options.framebufferPluginFile.generic_string(), "plugins/framebuffer.dll");
    EXPECT_FALSE(options.showHelp);
    EXPECT_FALSE(options.noSnapshot);
}

/**
 * @brief Parses the no-snapshot option independently from the window path.
 */
TEST(WindowLauncherTests, ParseCommandLineAcceptsNoSnapshotFlag)
{
    mfd::window::LauncherConfig config;

    char program[] = "mfd_window";
    char windowFlag[] = "--window";
    char windowPath[] = "custom/window.json";
    char noSnapshot[] = "--no-snapshot";
    char* argv[] = {program, windowFlag, windowPath, noSnapshot, nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(4, argv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(options.windowFile.generic_string(), "custom/window.json");
    EXPECT_TRUE(options.noSnapshot);
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
    EXPECT_FALSE(options.noSnapshot);
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
    EXPECT_FALSE(options.noSnapshot);
}

/**
 * @brief Supports one positional window path argument.
 */
TEST(WindowLauncherTests, ParseCommandLineAcceptsPositionalWindowPath)
{
    mfd::window::LauncherConfig config;

    char program[] = "mfd_window";
    char positional[] = "examples/radar_load/assets/windows/radar_load_window.json";
    char* argv[] = {program, positional, nullptr};

    mfd::window::LauncherOptions options;
    std::string error;
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(2, argv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(options.windowFile.generic_string(), "examples/radar_load/assets/windows/radar_load_window.json");
    EXPECT_TRUE(options.framebufferPluginFile.empty());
    EXPECT_FALSE(options.noSnapshot);
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
 * @brief Ignores arguments outside the window launcher contract.
 */
TEST(WindowLauncherTests, ParseCommandLineIgnoresArgumentsOutsideWindowContract)
{
    mfd::window::LauncherConfig config;
    config.defaultWindowFile = "default/window.json";

    mfd::window::LauncherOptions options;
    std::string error;

    char program[] = "mfd_window";
    char unknown[] = "--bad";
    char unknownValue[] = "plugin-value";
    char* unknownArgv[] = {program, unknown, unknownValue, nullptr};
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(3, unknownArgv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(options.windowFile.generic_string(), "default/window.json");

    char windowFlag[] = "--window";
    char windowPath[] = "custom/window.json";
    char extra[] = "plugin-positional";
    char* extraArgv[] = {program, windowFlag, windowPath, extra, nullptr};
    EXPECT_TRUE(mfd::window::ParseLauncherCommandLine(4, extraArgv, config, options, error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(options.windowFile.generic_string(), "custom/window.json");
}

/**
 * @brief Rejects malformed known launcher options and reports precise errors.
 */
TEST(WindowLauncherTests, ParseCommandLineRejectsMalformedKnownOptions)
{
    mfd::window::LauncherConfig config;
    mfd::window::LauncherOptions options;
    std::string error;

    char program[] = "mfd_window";
    char unknown[] = "--bad";
    char* unknownArgv[] = {program, unknown, nullptr};
    EXPECT_FALSE(mfd::window::ParseLauncherCommandLine(2, unknownArgv, config, options, error));
    EXPECT_NE(error.find("No window JSON path"), std::string::npos);

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
 * @brief Builds the plugin host ABI descriptor with the original launcher arguments.
 */
TEST(WindowLauncherTests, FramebufferPluginHostApiCarriesLauncherArguments)
{
    const std::array<const char*, 5> launchArguments {
        "mfd_window",
        "--window",
        "custom/window.json",
        "--framebuffer-plugin",
        "plugins/framebuffer.dll"};

    const MfdWindowFramebufferPluginHostApi hostApi =
        mfd::window::detail::BuildFramebufferPluginHostApi(
            MfdWindowFramebufferPixelFormat_Bgra32,
            static_cast<int>(launchArguments.size()),
            launchArguments.data());

    EXPECT_EQ(hostApi.struct_size, sizeof(MfdWindowFramebufferPluginHostApi));
    EXPECT_EQ(hostApi.abi_version, MFD_WINDOW_FRAMEBUFFER_PLUGIN_ABI_VERSION);
    EXPECT_EQ(hostApi.output_pixel_format, MfdWindowFramebufferPixelFormat_Bgra32);
    EXPECT_EQ(hostApi.launch_argc, static_cast<int>(launchArguments.size()));
    ASSERT_NE(hostApi.launch_argv, nullptr);
    EXPECT_STREQ(hostApi.launch_argv[0], "mfd_window");
    EXPECT_STREQ(hostApi.launch_argv[2], "custom/window.json");
    EXPECT_STREQ(hostApi.launch_argv[4], "plugins/framebuffer.dll");
}

/**
 * @brief Normalizes absent launcher arguments to a null plugin argument view.
 */
TEST(WindowLauncherTests, FramebufferPluginHostApiNormalizesMissingLauncherArguments)
{
    const MfdWindowFramebufferPluginHostApi hostApi =
        mfd::window::detail::BuildFramebufferPluginHostApi(
            MfdWindowFramebufferPixelFormat_Rgba32,
            2,
            nullptr);

    EXPECT_EQ(hostApi.struct_size, sizeof(MfdWindowFramebufferPluginHostApi));
    EXPECT_EQ(hostApi.abi_version, MFD_WINDOW_FRAMEBUFFER_PLUGIN_ABI_VERSION);
    EXPECT_EQ(hostApi.output_pixel_format, MfdWindowFramebufferPixelFormat_Rgba32);
    EXPECT_EQ(hostApi.launch_argc, 0);
    EXPECT_EQ(hostApi.launch_argv, nullptr);
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
        [&receivedWidth, &receivedHeight, &receivedByteCount](const int width,
                                                              const int height,
                                                              const mfd::ByteView rgba32Bytes)
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

/**
 * @brief Ensures framebuffer plugins only observe the page viewport width even when the host window is wider.
 */
TEST(WindowLauncherTests, PluginFramebufferCaptureSizeTracksOnlyPageViewport)
{
    const mfd::window::detail::FramebufferCaptureSize captureSize =
        mfd::window::detail::ResolvePluginFramebufferCaptureSize(1260, 900, 640);

    EXPECT_EQ(captureSize.width, 640);
    EXPECT_EQ(captureSize.height, 900);
}

/**
 * @brief Clamps invalid viewport widths to one valid host-backed capture extent.
 */
TEST(WindowLauncherTests, PluginFramebufferCaptureSizeClampsToHostFramebuffer)
{
    const mfd::window::detail::FramebufferCaptureSize oversizedCaptureSize =
        mfd::window::detail::ResolvePluginFramebufferCaptureSize(800, 600, 1600);
    const mfd::window::detail::FramebufferCaptureSize nonPositiveViewportCaptureSize =
        mfd::window::detail::ResolvePluginFramebufferCaptureSize(800, 600, 0);
    const mfd::window::detail::FramebufferCaptureSize invalidHostCaptureSize =
        mfd::window::detail::ResolvePluginFramebufferCaptureSize(0, 600, 320);

    EXPECT_EQ(oversizedCaptureSize.width, 800);
    EXPECT_EQ(oversizedCaptureSize.height, 600);
    EXPECT_EQ(nonPositiveViewportCaptureSize.width, 0);
    EXPECT_EQ(nonPositiveViewportCaptureSize.height, 0);
    EXPECT_EQ(invalidHostCaptureSize.width, 0);
    EXPECT_EQ(invalidHostCaptureSize.height, 0);
}

/**
 * @brief Validates the stable raw-frame helper accepts one complete tightly packed `RGBA32` frame.
 */
TEST(WindowLauncherTests, StableFramebufferFrameValidationAcceptsCompleteRgba32Frame)
{
    const std::array<std::uint8_t, 16> pixels {};

    MfdWindowFramebufferFrame frame {};
    frame.struct_size = sizeof(frame);
    frame.pixel_format = MfdWindowFramebufferPixelFormat_Rgba32;
    frame.width = 2;
    frame.height = 2;
    frame.row_stride_bytes = 8;
    frame.pixels = pixels.data();
    frame.pixel_bytes = pixels.size();

    EXPECT_TRUE(mfd::window::ValidateFramebufferFrame(frame));
    EXPECT_TRUE(mfd::window::ValidateFramebufferRgba32Layout(
        frame.width,
        frame.height,
        frame.pixels,
        frame.pixel_bytes));
    EXPECT_EQ(mfd::window::ComputeFramebufferByteCount(MfdWindowFramebufferPixelFormat_Rgba32, 2, 2), pixels.size());
}

/**
 * @brief Validates the stable raw-frame helpers also accept one complete tightly packed `BGRA32` frame.
 */
TEST(WindowLauncherTests, StableFramebufferFrameValidationAcceptsCompleteBgra32Frame)
{
    const std::array<std::uint8_t, 16> pixels {};

    MfdWindowFramebufferFrame frame {};
    frame.struct_size = sizeof(frame);
    frame.pixel_format = MfdWindowFramebufferPixelFormat_Bgra32;
    frame.width = 2;
    frame.height = 2;
    frame.row_stride_bytes = 8;
    frame.pixels = pixels.data();
    frame.pixel_bytes = pixels.size();

    EXPECT_TRUE(mfd::window::ValidateFramebufferFrame(frame));
    EXPECT_TRUE(mfd::window::ValidateFramebufferLayout(
        MfdWindowFramebufferPixelFormat_Bgra32,
        frame.width,
        frame.height,
        frame.pixels,
        frame.pixel_bytes));
    EXPECT_EQ(mfd::window::ComputeFramebufferByteCount(MfdWindowFramebufferPixelFormat_Bgra32, 2, 2), pixels.size());
}

/**
 * @brief Rejects unsupported framebuffer pixel formats in the stable helpers.
 */
TEST(WindowLauncherTests, StableFramebufferHelpersRejectUnsupportedPixelFormat)
{
    const std::array<std::uint8_t, 16> pixels {};

    EXPECT_EQ(mfd::window::ComputeFramebufferByteCount(static_cast<MfdWindowFramebufferPixelFormat>(0), 2, 2), 0U);
    EXPECT_FALSE(mfd::window::ValidateFramebufferLayout(
        static_cast<MfdWindowFramebufferPixelFormat>(0),
        2,
        2,
        pixels.data(),
        pixels.size()));
}

/**
 * @brief Rejects malformed stable raw-frame descriptors.
 */
TEST(WindowLauncherTests, StableFramebufferFrameValidationRejectsInvalidLayout)
{
    const std::array<std::uint8_t, 12> pixels {};

    MfdWindowFramebufferFrame frame {};
    frame.struct_size = sizeof(frame);
    frame.pixel_format = MfdWindowFramebufferPixelFormat_Rgba32;
    frame.width = 2;
    frame.height = 2;
    frame.row_stride_bytes = 6;
    frame.pixels = pixels.data();
    frame.pixel_bytes = pixels.size();

    EXPECT_FALSE(mfd::window::ValidateFramebufferFrame(frame));
    EXPECT_FALSE(mfd::window::ValidateFramebufferRgba32Layout(
        frame.width,
        frame.height,
        frame.pixels,
        frame.pixel_bytes));
}
