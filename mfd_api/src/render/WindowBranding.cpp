/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for host-window branding helpers.
 */

#include "mfd/render/WindowBranding.h"

#include <array>
#include <system_error>

#include <raylib.h>

namespace mfd
{
namespace
{
constexpr const char* kBundledBrandingIconRelativePath = "branding/mfdstudio_app_icon.png";

std::filesystem::path NormalizePath(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return {};
    }

    return path.is_absolute() ? path.lexically_normal() : std::filesystem::absolute(path).lexically_normal();
}

std::filesystem::path ResolveAnchorDirectory(const std::filesystem::path& sourceAnchor)
{
    if (sourceAnchor.empty())
    {
        return {};
    }

    return sourceAnchor.has_extension() ? sourceAnchor.parent_path() : sourceAnchor;
}

bool PathExists(const std::filesystem::path& path)
{
    std::error_code errorCode;
    return !path.empty() && std::filesystem::exists(path, errorCode);
}
} // namespace

std::filesystem::path ResolveWindowBrandingIconFile(std::filesystem::path preferredIconFile,
                                                    std::filesystem::path sourceAnchor)
{
    if (!preferredIconFile.empty())
    {
        return NormalizePath(preferredIconFile);
    }

    const std::filesystem::path anchorDirectory = ResolveAnchorDirectory(sourceAnchor);
    const std::array<std::filesystem::path, 5> candidates {
        anchorDirectory.empty() ? std::filesystem::path {}
                                : anchorDirectory / ".." / ".." / "branding" / "mfdstudio_app_icon.png",
        anchorDirectory.empty() ? std::filesystem::path {}
                                : anchorDirectory / ".." / "branding" / "mfdstudio_app_icon.png",
        anchorDirectory.empty() ? std::filesystem::path {}
                                : anchorDirectory / "branding" / "mfdstudio_app_icon.png",
        anchorDirectory.empty() ? std::filesystem::path {}
                                : anchorDirectory / "assets" / "branding" / "mfdstudio_app_icon.png",
        std::filesystem::path {kBundledBrandingIconRelativePath}};

    for (const std::filesystem::path& candidate : candidates)
    {
        const std::filesystem::path normalizedCandidate = NormalizePath(candidate);
        if (PathExists(normalizedCandidate))
        {
            return normalizedCandidate;
        }
    }

    return NormalizePath(std::filesystem::path {kBundledBrandingIconRelativePath});
}

bool ApplyWindowIconFile(const std::filesystem::path& iconFile, std::string* error)
{
    if (error != nullptr)
    {
        error->clear();
    }

    if (!IsWindowReady())
    {
        if (error != nullptr)
        {
            *error = "The host window is not ready yet.";
        }
        return false;
    }

    if (iconFile.empty())
    {
        if (error != nullptr)
        {
            *error = "The icon file path is empty.";
        }
        return false;
    }

    Image iconImage = LoadImage(iconFile.string().c_str());
    if (iconImage.data == nullptr || iconImage.width <= 0 || iconImage.height <= 0)
    {
        if (error != nullptr)
        {
            *error = "Unable to load the window icon '" + iconFile.string() + "'.";
        }
        return false;
    }

    SetWindowIcon(iconImage);
    UnloadImage(iconImage);
    return true;
}
} // namespace mfd
