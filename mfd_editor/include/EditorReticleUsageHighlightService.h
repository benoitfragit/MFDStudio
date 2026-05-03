/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Read-only helpers computing page-usage highlights for one selected reticle template.
 */

#include <filesystem>
#include <string>
#include <vector>

#include "EditorAssetReferenceIndexService.h"
#include "EditorDocumentSerializer.h"
#include "mfd/io/JsonLoader.h"

namespace editor
{
/**
 * @brief One page that references the selected reticle template.
 */
struct ReticleUsageHighlightPage
{
    /** @brief Backing page JSON file when known. */
    std::filesystem::path pageFile {};
    /** @brief Authored page name shown by the editor. */
    std::string pageName {};
    /** @brief Zero-based current editor page index when the page is open, or `-1`. */
    int currentPageIndex = -1;
    /** @brief Current page-reticle indices matching the selected template on the open page. */
    std::vector<int> matchingReticleIndices {};
    /** @brief Indicates whether the current page strobe also references the selected template. */
    bool matchingStrobe = false;
};

/**
 * @brief Full read-only highlight result for one reticle-template selection.
 */
struct ReticleUsageHighlightResult
{
    /** @brief Selected reticle template id. */
    std::string templateId {};
    /** @brief Asset root scanned for additional page usages. */
    std::filesystem::path assetsRoot {};
    /** @brief Pages that currently reference the selected template. */
    std::vector<ReticleUsageHighlightPage> pages {};
    /** @brief Number of exact page-level references found. */
    std::size_t totalReferenceCount = 0U;
    /** @brief Indicates whether the selected template is used at least once. */
    bool hasUsage = false;
};

/**
 * @brief Input used to compute page highlights for one selected reticle template.
 */
struct ReticleUsageHighlightRequest
{
    /** @brief Selected reticle template id. */
    std::string templateId {};
    /** @brief Asset root scanned for page usages outside the open document. */
    std::filesystem::path assetsRoot {};
};

/**
 * @brief Stateless service computing read-only reticle-template usages for the page preview.
 */
class ReticleUsageHighlightService
{
public:
    /**
     * @brief Computes highlighted pages and current-page instances for one selected template id.
     * @param loaded Current authored window and document state.
     * @param files Current authored file layout tracked by the editor.
     * @param request Selected template id plus asset-root scan options.
     * @return Structured highlight information ready for the page tree and preview overlays.
     */
    [[nodiscard]] ReticleUsageHighlightResult BuildHighlight(const mfd::LoadedWindowConfiguration& loaded,
                                                             const EditorFileLayout& files,
                                                             const ReticleUsageHighlightRequest& request) const;

private:
    AssetReferenceIndexService referenceIndexService_ {};
};
} // namespace editor
