#pragma once

/**
 * @file
 * @brief Shared deterministic discovery and identifier validation for JSON assets.
 */

#include "mfd/model/PageName.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mfd::detail
{
using AssetIdentifierOrigins = std::unordered_map<std::string, std::filesystem::path>;

/**
 * @brief Discovers regular JSON files in deterministic path order.
 * @param folder Asset directory to inspect.
 * @return Sorted paths whose extension compares equal to `.json` ignoring case.
 * @throws std::runtime_error When the directory does not exist.
 */
inline std::vector<std::filesystem::path> DiscoverSortedJsonAssetFiles(const std::filesystem::path& folder)
{
    if (!std::filesystem::exists(folder))
    {
        throw std::runtime_error("Asset folder does not exist: " + folder.string());
    }

    std::vector<std::filesystem::path> files;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.is_regular_file() && PageNamesEqual(entry.path().extension().string(), ".json"))
        {
            files.push_back(entry.path().lexically_normal());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

/**
 * @brief Registers an asset identifier and rejects normalized duplicates.
 * @param origins Previously registered normalized identifiers and their source paths.
 * @param identifier Identifier declared by the asset.
 * @param sourcePath Source file declaring the identifier.
 * @param assetKind Human-readable asset kind used in diagnostics.
 * @throws std::runtime_error When another source already declares the same normalized identifier.
 */
inline void RegisterUniqueAssetIdentifier(AssetIdentifierOrigins& origins,
                                          const std::string_view identifier,
                                          const std::filesystem::path& sourcePath,
                                          const std::string_view assetKind)
{
    const std::string normalizedIdentifier = NormalizePageName(identifier);
    const auto insertion = origins.emplace(normalizedIdentifier, sourcePath.lexically_normal());
    if (insertion.second)
    {
        return;
    }

    throw std::runtime_error(
        "Duplicate " + std::string(assetKind) + " id '" + std::string(identifier) + "' in assets '" +
        insertion.first->second.string() + "' and '" + sourcePath.lexically_normal().string() + "'");
}
} // namespace mfd::detail
