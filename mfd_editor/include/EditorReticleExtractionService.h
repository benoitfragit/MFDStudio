/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Planning and execution helpers for extracting selected page reticles into one reusable template.
 */

#include <filesystem>
#include <string>
#include <vector>

#include "EditorDocumentSerializer.h"
#include "mfd/io/JsonLoader.h"

namespace editor
{
/**
 * @brief Input used to extract one reusable template from selected page reticles.
 */
struct ReticleExtractionRequest
{
    /** @brief Zero-based page index containing the current page-reticle selection. */
    int pageIndex = -1;
    /** @brief Zero-based page-reticle indices selected for extraction. */
    std::vector<int> reticleIndices {};
    /** @brief User-requested template id, before automatic collision handling. */
    std::string requestedTemplateId {};
    /** @brief Optional preferred JSON file for the new template. */
    std::filesystem::path requestedTemplateFile {};
};

/**
 * @brief Read-only extraction plan built before mutating the document.
 */
struct ReticleExtractionPlan
{
    /** @brief Indicates whether `Execute()` may apply this plan safely. */
    bool canExecute = false;
    /** @brief Blocking validation error when the selection cannot be extracted yet. */
    std::string error {};
    /** @brief Zero-based page index targeted by the extraction. */
    int pageIndex = -1;
    /** @brief Selected page-reticle indices sorted in execution order. */
    std::vector<int> reticleIndices {};
    /** @brief Selected page-reticle ids surfaced in the review popup. */
    std::vector<std::string> sourceReticleIds {};
    /** @brief Requested template id before automatic collision handling. */
    std::string requestedTemplateId {};
    /** @brief Final template id reserved for the extracted library asset. */
    std::string targetTemplateId {};
    /** @brief Indicates whether the final template id was auto-renamed to avoid a collision. */
    bool templateIdAdjusted = false;
    /** @brief Final JSON file staged for the extracted template. */
    std::filesystem::path targetTemplateFile {};
    /** @brief Indicates whether the target JSON file was auto-renamed to avoid a collision. */
    bool templateFileAdjusted = false;
    /** @brief Page-reticle id reserved for the replacement instance inserted back into the page. */
    std::string replacementInstanceId {};
    /** @brief Earliest selected reticle index used as the insertion point. */
    int insertionIndex = -1;
    /** @brief Number of flattened primitives copied into the extracted template. */
    std::size_t extractedPrimitiveCount = 0U;
    /** @brief Editor layer assigned to the replacement page reticle. */
    std::string targetLayerId {};
    /** @brief Shared draw-order flag preserved on the replacement reticle. */
    bool drawOnTop = false;
    /** @brief Translation anchor used to localize extracted primitive transforms. */
    mfd::Vec2 anchor {};
    /** @brief New library template staged by the extraction. */
    mfd::ReticleGroup extractedTemplate {};
    /** @brief Replacement instance inserted back into the page. */
    mfd::ReticleGroup replacementInstance {};
};

/**
 * @brief Mutation result returned after one successful extraction execution.
 */
struct ReticleExtractionResult
{
    /** @brief Final template id created in the shared library. */
    std::string templateId {};
    /** @brief Final JSON file staged for the extracted template. */
    std::filesystem::path templateFile {};
    /** @brief Zero-based page-reticle index of the replacement instance. */
    int insertedReticleIndex = -1;
    /** @brief Number of primitives copied into the new template. */
    std::size_t extractedPrimitiveCount = 0U;
};

/**
 * @brief Stateless service extracting one selected page-reticle block into a reusable library template.
 */
class ReticleExtractionService
{
public:
    /**
     * @brief Builds a read-only extraction plan for the current page-reticle selection.
     * @param loaded Current authored window and document state.
     * @param files Current authored file layout tracked by the editor.
     * @param request Extraction request built from the UI selection.
     * @return Full extraction plan, including collision-adjusted names and the replacement preview.
     */
    [[nodiscard]] ReticleExtractionPlan BuildPlan(const mfd::LoadedWindowConfiguration& loaded,
                                                  const EditorFileLayout& files,
                                                  const ReticleExtractionRequest& request) const;

    /**
     * @brief Applies one extraction plan to the in-memory document and file layout.
     * @param plan Plan previously produced by `BuildPlan()`.
     * @param loaded Mutable authored document to update.
     * @param files Mutable authored file layout to update.
     * @param result Optional execution result receiving the staged template/file information.
     * @param error Optional blocking error output when execution fails.
     * @return `true` when the extraction succeeded and the document now contains the replacement instance.
     */
    bool Execute(const ReticleExtractionPlan& plan,
                 mfd::LoadedWindowConfiguration& loaded,
                 EditorFileLayout& files,
                 ReticleExtractionResult* result,
                 std::string* error) const;
};
} // namespace editor
