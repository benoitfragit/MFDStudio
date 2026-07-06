/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Shared private helpers used by the `mfd_window` launcher implementation and its tests.
 */

#include <algorithm>
#include <cstdint>

#include "mfd/window/WindowLauncherPlugin.h"

namespace mfd::window::detail
{
/**
 * @brief CPU readback dimensions forwarded to framebuffer plugins.
 */
struct FramebufferCaptureSize
{
    int width = 0;
    int height = 0;
};

/**
 * @brief Resolves the framebuffer size that must stay visible to plugins.
 * @param hostRenderWidth Current host framebuffer width in pixels.
 * @param hostRenderHeight Current host framebuffer height in pixels.
 * @param pageViewportWidth Width of the runtime page viewport rendered on screen.
 * @return Capture size constrained to the authored page viewport.
 *
 * @note The runtime debug side panel is intentionally excluded from this size
 * so framebuffer plugins only observe the page image currently rendered by the
 * launcher, regardless of the overlay visibility.
 */
inline FramebufferCaptureSize ResolvePluginFramebufferCaptureSize(const int hostRenderWidth,
                                                                 const int hostRenderHeight,
                                                                 const int pageViewportWidth) noexcept
{
    if (hostRenderWidth <= 0 || hostRenderHeight <= 0 || pageViewportWidth <= 0)
    {
        return {};
    }

    return FramebufferCaptureSize {std::clamp(pageViewportWidth, 1, hostRenderWidth), hostRenderHeight};
}

/**
 * @brief Builds the host ABI descriptor passed to a framebuffer plugin.
 * @param outputPixelFormat Pixel layout selected after reading the plugin descriptor.
 * @param launchArgumentCount Number of launcher arguments forwarded to the plugin.
 * @param launchArguments Borrowed launcher argument pointer array.
 * @return Fully initialized host ABI descriptor.
 */
inline MfdWindowFramebufferPluginHostApi BuildFramebufferPluginHostApi(
    const MfdWindowFramebufferPixelFormat outputPixelFormat,
    const int launchArgumentCount,
    const char* const* launchArguments) noexcept
{
    MfdWindowFramebufferPluginHostApi hostApi {};
    hostApi.struct_size = sizeof(hostApi);
    hostApi.abi_version = MFD_WINDOW_FRAMEBUFFER_PLUGIN_ABI_VERSION;
    hostApi.output_pixel_format = static_cast<std::uint32_t>(outputPixelFormat);

    if (launchArgumentCount > 0 && launchArguments != nullptr)
    {
        hostApi.launch_argc = static_cast<std::int32_t>(launchArgumentCount);
        hostApi.launch_argv = launchArguments;
    }

    return hostApi;
}
} // namespace mfd::window::detail
