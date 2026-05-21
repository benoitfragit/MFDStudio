/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Helpers for resolving and applying host-window branding assets.
 */

#include <filesystem>
#include <string>

namespace mfd
{
/**
 * @brief Resolves one application icon file, falling back to the bundled MFDStudio branding image.
 * @param preferredIconFile Explicit icon file chosen by the caller. May be empty.
 * @param sourceAnchor Optional source JSON file or asset folder used to look up the bundled branding image.
 * @return Resolved icon file path. The returned path may still not exist when no branding file is available.
 *
 * @note This helper belongs to the repository host-side raylib render layer.
 */
std::filesystem::path ResolveWindowBrandingIconFile(std::filesystem::path preferredIconFile = {},
                                                    std::filesystem::path sourceAnchor = {});

/**
 * @brief Applies one image file as the desktop icon of the currently open host window.
 * @param iconFile Path to a raylib-supported image file such as a PNG.
 * @param error Optional error string filled on failure.
 * @return `true` when the icon was applied successfully.
 *
 * @note On Windows this updates the icon used by the title bar and taskbar thumbnail.
 * @note This helper belongs to the repository host-side raylib render layer.
 */
bool ApplyWindowIconFile(const std::filesystem::path& iconFile, std::string* error = nullptr);
} // namespace mfd
