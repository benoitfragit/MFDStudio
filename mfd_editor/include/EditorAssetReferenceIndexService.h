/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Central asset reference scanner used by editor rename, import, highlight and diagnostics workflows.
 */

#include <filesystem>
#include <string>
#include <vector>

namespace editor
{
/**
 * @brief Kinds of non-page asset references discovered while scanning editor assets.
 */
enum class ReticleReferenceKind
{
    PageReticleTemplate,
    PageStrobeTemplate,
    PageDynamicTemplate,
    ReticleImageAsset,
    WindowFontAsset,
    WindowIconAsset
};

/**
 * @brief Kinds of missing assets reported by the reference index.
 */
enum class MissingAssetKind
{
    PageFile,
    ReticleTemplate,
    ImageFile,
    FontFile,
    IconFile
};

/**
 * @brief Input options controlling one asset reference scan.
 */
struct AssetReferenceIndexRequest
{
    /** @brief Root folder scanned recursively for source JSON assets. */
    std::filesystem::path assetsRoot;
};

/**
 * @brief One `window -> page` relationship discovered in source assets.
 */
struct PageReference
{
    /** @brief Window JSON file owning the page reference. */
    std::filesystem::path windowFile;
    /** @brief Resolved page JSON path. */
    std::filesystem::path pageFile;
    /** @brief Resolved page name when the page JSON could be parsed. */
    std::string pageName;
    /** @brief Indicates whether the window declares this page as its default page. */
    bool defaultPage = false;
};

/**
 * @brief One asset dependency discovered from a window, page or reticle JSON file.
 *
 * @note The type keeps a generic shape because later editor workflows consume a
 * single dependency stream even when the target is a template, an image, a
 * font, or a window icon.
 */
struct ReticleReference
{
    /** @brief Semantic kind of dependency. */
    ReticleReferenceKind kind = ReticleReferenceKind::PageReticleTemplate;
    /** @brief JSON file owning the dependency. */
    std::filesystem::path ownerFile;
    /** @brief Resolved target asset path when one concrete file exists. */
    std::filesystem::path targetFile;
    /** @brief Page name owning the dependency when relevant. */
    std::string pageName;
    /** @brief Reticle id owning the dependency when relevant. */
    std::string reticleId;
    /** @brief Referenced template id when relevant. */
    std::string templateId;
    /** @brief Referenced primitive id when relevant. */
    std::string primitiveId;
};

/**
 * @brief One missing asset discovered while traversing references.
 */
struct MissingAssetReference
{
    /** @brief Semantic kind of missing dependency. */
    MissingAssetKind kind = MissingAssetKind::PageFile;
    /** @brief File that declared the missing dependency. */
    std::filesystem::path ownerFile;
    /**
     * @brief Resolved missing path when one path was available.
     *
     * @note Template-id-only failures may leave this path empty when no single
     * file target can be resolved yet.
     */
    std::filesystem::path referencedPath;
    /** @brief Page name owning the missing dependency when relevant. */
    std::string pageName;
    /** @brief Reticle id owning the missing dependency when relevant. */
    std::string reticleId;
    /** @brief Referenced template id when relevant. */
    std::string templateId;
    /** @brief Referenced primitive id when relevant. */
    std::string primitiveId;
    /** @brief Human-readable detail attached to the failure. */
    std::string detail;
};

/**
 * @brief One non-fatal scan error such as invalid JSON or ambiguous metadata.
 */
struct AssetScanError
{
    /** @brief File that triggered the scan error. */
    std::filesystem::path file;
    /** @brief Error message describing the scan failure. */
    std::string message;
};

/**
 * @brief Full result of one asset reference scan.
 */
struct AssetReferenceIndex
{
    /** @brief Window-to-page relationships. */
    std::vector<PageReference> pages;
    /** @brief Non-page asset dependencies. */
    std::vector<ReticleReference> reticles;
    /** @brief Missing assets discovered while walking dependencies. */
    std::vector<MissingAssetReference> missingAssets;
    /** @brief Non-fatal scan errors discovered while parsing files. */
    std::vector<AssetScanError> errors;
};

/**
 * @brief Scans one asset tree and builds the editor reference graph without mutating any file.
 */
class AssetReferenceIndexService
{
public:
    /**
     * @brief Scans the requested asset tree and returns the discovered references.
     * @param request Scan options describing the asset root.
     * @return Structured reference graph, missing assets and scan errors.
     */
    [[nodiscard]] AssetReferenceIndex BuildIndex(const AssetReferenceIndexRequest& request) const;
};
} // namespace editor
