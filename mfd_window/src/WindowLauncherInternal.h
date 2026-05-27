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
} // namespace mfd::window::detail
