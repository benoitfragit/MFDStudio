/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Editor-only controller used by the page-preview layer-focus workflow.
 */

#include <string>
#include <string_view>
#include <vector>

#include "mfd/model/PageDefinition.h"
#include "mfd/model/Reticle.h"

namespace editor
{
/**
 * @brief Page-preview focus state selecting one editor layer, or full view when empty.
 */
struct LayerFocusState
{
    /** @brief Focused editor-layer id, or empty when the preview stays in full-view mode. */
    std::string focusedLayerId {};
};

/**
 * @brief One entry rendered by the page-preview layer inspector strip.
 */
struct LayerFocusStripEntry
{
    /** @brief Underlying layer id, or empty for the synthetic Full View entry. */
    std::string layerId {};
    /** @brief User-facing label rendered by the overlay. */
    std::string label {};
    /** @brief Marks the synthetic Full View entry. */
    bool fullView = false;
    /** @brief Indicates whether the entry currently owns the focus selection. */
    bool selected = false;
    /** @brief Indicates whether the authored editor layer is visible in the page preview. */
    bool visible = true;
};

/**
 * @brief Full read-only model consumed by the page-preview layer inspector strip.
 */
struct LayerFocusStripModel
{
    /** @brief Indicates whether one real editor layer currently owns the focus. */
    bool focusActive = false;
    /** @brief Overlay entries shown from top to bottom. */
    std::vector<LayerFocusStripEntry> entries {};
};

/**
 * @brief Stateless controller centralizing page-preview layer-focus decisions.
 */
class LayerFocusController
{
public:
    /**
     * @brief Clears invalid focus ids that no longer resolve on the current page.
     * @param page Active page inspected by the preview.
     * @param state Mutable focus state owned by the editor shell.
     */
    void SanitizeFocusState(const mfd::PageDefinition& page, LayerFocusState& state) const;

    /**
     * @brief Builds the read-only strip model for the current page and focus state.
     * @param page Active page inspected by the preview.
     * @param state Current focus state owned by the editor shell.
     * @return Full-view entry plus every authored editor layer in display order.
     */
    [[nodiscard]] LayerFocusStripModel BuildStripModel(const mfd::PageDefinition& page,
                                                       const LayerFocusState& state) const;

    /**
     * @brief Returns whether the preview is currently focused on one concrete editor layer.
     * @param page Active page inspected by the preview.
     * @param state Current focus state owned by the editor shell.
     * @return `true` when `state.focusedLayerId` resolves on the page.
     */
    [[nodiscard]] bool IsFocusActive(const mfd::PageDefinition& page, const LayerFocusState& state) const;

    /**
     * @brief Returns whether one reticle stays editable under the current focus mode.
     * @param page Active page inspected by the preview.
     * @param reticle Reticle candidate evaluated for selection or editing.
     * @param state Current focus state owned by the editor shell.
     * @return `true` when full view is active or when the reticle belongs to the focused layer.
     */
    [[nodiscard]] bool IsReticleSelectable(const mfd::PageDefinition& page,
                                           const mfd::ReticleGroup& reticle,
                                           const LayerFocusState& state) const;

    /**
     * @brief Returns whether one reticle should stay visible but dimmed in the preview.
     * @param page Active page inspected by the preview.
     * @param reticle Reticle candidate evaluated for preview rendering.
     * @param state Current focus state owned by the editor shell.
     * @return `true` when another layer is focused and the reticle belongs elsewhere.
     */
    [[nodiscard]] bool ShouldReticleBeDimmed(const mfd::PageDefinition& page,
                                             const mfd::ReticleGroup& reticle,
                                             const LayerFocusState& state) const;

    /**
     * @brief Filters one page-reticle selection to the subset still editable in focus mode.
     * @param page Active page inspected by the preview.
     * @param candidateIndices Page-reticle indices proposed by the editor shell.
     * @param state Current focus state owned by the editor shell.
     * @return Indices that still resolve to editable reticles on the focused layer.
     */
    [[nodiscard]] std::vector<int> FilterSelectableReticleIndices(const mfd::PageDefinition& page,
                                                                  const std::vector<int>& candidateIndices,
                                                                  const LayerFocusState& state) const;

    /**
     * @brief Builds one focused state targeting the requested layer when it exists.
     * @param page Active page inspected by the preview.
     * @param layerId Requested editor-layer id.
     * @return Focus state referencing that layer, or full view when the layer does not exist.
     */
    [[nodiscard]] LayerFocusState MakeFocusedState(const mfd::PageDefinition& page, std::string_view layerId) const;
};
} // namespace editor
