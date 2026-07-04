/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorAssetPathService.h"

/**
 * @file
 * @brief Implementation of editor asset path defaults and guards.
 */

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

namespace editor
{
namespace
{
std::string Lowercase(const std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());

    for (const char character : value)
    {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }

    return lowered;
}

std::optional<std::filesystem::path> FindProjectRoot(const std::filesystem::path& start)
{
    std::filesystem::path current = std::filesystem::absolute(start).lexically_normal();

    while (true)
    {
        if (std::filesystem::exists(current / "CMakeLists.txt") &&
            std::filesystem::exists(current / "examples") &&
            std::filesystem::exists(current / "mfd_editor"))
        {
            return current;
        }

        if (!current.has_parent_path() || current == current.parent_path())
        {
            break;
        }

        current = current.parent_path();
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> FindAncestorNamed(std::filesystem::path path,
                                                       const std::string_view folderName)
{
    if (path.empty())
    {
        return std::nullopt;
    }

    path = std::filesystem::absolute(path).lexically_normal();
    const std::string normalizedFolderName = Lowercase(folderName);

    while (!path.empty())
    {
        if (Lowercase(path.filename().string()) == normalizedFolderName)
        {
            return path;
        }

        if (!path.has_parent_path() || path == path.parent_path())
        {
            break;
        }

        path = path.parent_path();
    }

    return std::nullopt;
}

bool PathContainsSegment(const std::filesystem::path& path, const std::string_view segment)
{
    if (segment.empty())
    {
        return false;
    }

    const std::string normalizedSegment = Lowercase(segment);
    for (const auto& part : path)
    {
        if (Lowercase(part.string()) == normalizedSegment)
        {
            return true;
        }
    }

    return false;
}

std::filesystem::path StripLeadingAssetRootPrefix(const std::filesystem::path& path)
{
    bool containsAssetRoot = false;
    for (const auto& part : path)
    {
        if (Lowercase(part.string()) == "assets")
        {
            containsAssetRoot = true;
            break;
        }
    }

    if (!containsAssetRoot)
    {
        return path;
    }

    std::filesystem::path stripped;
    bool insideAssetRoot = false;

    for (const auto& part : path)
    {
        if (!insideAssetRoot)
        {
            insideAssetRoot = Lowercase(part.string()) == "assets";
            continue;
        }

        stripped /= part;
    }

    return stripped;
}
} // namespace

EditorAssetPathService::EditorAssetPathService(std::filesystem::path assetDirectory)
{
    if (!assetDirectory.empty())
    {
        configuredAssetDirectory_ = std::filesystem::absolute(assetDirectory).lexically_normal();
    }
}

const std::filesystem::path& EditorAssetPathService::ConfiguredAssetDirectory() const noexcept
{
    return configuredAssetDirectory_;
}

bool EditorAssetPathService::HasConfiguredAssetDirectory() const noexcept
{
    return !configuredAssetDirectory_.empty();
}

std::filesystem::path EditorAssetPathService::DefaultAssetPath(const std::string_view relativeAssetPath) const
{
    std::filesystem::path requested(relativeAssetPath);
    if (requested.is_absolute())
    {
        return requested.lexically_normal();
    }

    if (HasConfiguredAssetDirectory())
    {
        return (configuredAssetDirectory_ / StripLeadingAssetRootPrefix(requested)).lexically_normal();
    }

    if (const auto projectRoot = FindProjectRoot(std::filesystem::current_path()); projectRoot.has_value())
    {
        return (*projectRoot / requested).lexically_normal();
    }

    return std::filesystem::absolute(requested).lexically_normal();
}

std::filesystem::path EditorAssetPathService::DefaultSiblingAssetFile(const std::filesystem::path& anchorFile,
                                                                     const std::string_view siblingFolder,
                                                                     const std::string_view fileName) const
{
    if (anchorFile.empty())
    {
        return (DefaultAssetPath((std::filesystem::path("examples/demo/assets") / std::filesystem::path(siblingFolder)).generic_string()) /
                std::filesystem::path(fileName))
            .lexically_normal();
    }

    const std::filesystem::path anchorFolder = anchorFile.parent_path();
    const std::filesystem::path candidateRoot =
        anchorFolder.filename() == std::filesystem::path(siblingFolder)
            ? anchorFolder
            : (anchorFolder.has_parent_path() ? anchorFolder.parent_path() / std::filesystem::path(siblingFolder)
                                              : anchorFolder / std::filesystem::path(siblingFolder));
    return (candidateRoot / std::filesystem::path(fileName)).lexically_normal();
}

std::filesystem::path EditorAssetPathService::ResolveAssetRootForPath(const std::filesystem::path& path) const
{
    if (const auto assetsRoot = FindAncestorNamed(path, "assets"); assetsRoot.has_value())
    {
        return assetsRoot->lexically_normal();
    }

    if (HasConfiguredAssetDirectory())
    {
        return configuredAssetDirectory_;
    }

    return DefaultAssetPath("examples/demo/assets");
}

std::filesystem::path EditorAssetPathService::CurrentPageImportTargetFolder(const std::filesystem::path& windowFile,
                                                                            const EditorFileLayout& files) const
{
    if (!files.pageFiles.empty())
    {
        const std::filesystem::path firstFolder = files.pageFiles.front().parent_path();
        const bool allPageFoldersMatch = std::all_of(files.pageFiles.begin(),
                                                     files.pageFiles.end(),
                                                     [&firstFolder](const std::filesystem::path& pageFile)
                                                     {
                                                         return pageFile.parent_path().lexically_normal() ==
                                                                firstFolder.lexically_normal();
                                                     });
        if (allPageFoldersMatch && !firstFolder.empty())
        {
            return firstFolder.lexically_normal();
        }
    }

    if (Lowercase(windowFile.parent_path().filename().string()) == "windows" && windowFile.parent_path().has_parent_path())
    {
        return (windowFile.parent_path().parent_path() / "pages").lexically_normal();
    }

    if (const auto assetsRoot = FindAncestorNamed(windowFile.parent_path(), "assets"); assetsRoot.has_value())
    {
        return (*assetsRoot / "pages").lexically_normal();
    }

    return windowFile.empty() ? DefaultAssetPath("examples/demo/assets/pages") : windowFile.parent_path().lexically_normal();
}

std::filesystem::path EditorAssetPathService::ConfiguredPathFolder(const std::filesystem::path& configuredPath)
{
    if (configuredPath.empty())
    {
        return {};
    }

    return configuredPath.has_extension() ? configuredPath.parent_path() : configuredPath;
}

std::filesystem::path EditorAssetPathService::JsonFileNameOrFallback(const std::filesystem::path& candidate,
                                                                     const std::string_view fallbackFileName)
{
    std::filesystem::path fileName = candidate.filename();
    if (fileName.empty() || fileName == "." || fileName == "..")
    {
        fileName = std::filesystem::path(fallbackFileName);
    }

    if (fileName.extension().empty())
    {
        fileName += ".json";
    }

    return fileName;
}

bool EditorAssetPathService::IsExecStagingPath(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return false;
    }

    const std::filesystem::path absolutePath =
        path.is_absolute() ? path.lexically_normal() : std::filesystem::absolute(path).lexically_normal();
    return PathContainsSegment(absolutePath, "_Exec");
}
} // namespace editor
