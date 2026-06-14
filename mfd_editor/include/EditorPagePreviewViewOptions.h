/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Editor-only view preferences used by the page preview panel.
 */

namespace editor
{
/**
 * @brief Session-scoped display options controlling the page preview overlays.
 *
 * @details These preferences belong to the editor UI only. They must never be
 * serialized into window, page, or reticle JSON assets.
 */
struct PagePreviewViewOptions
{
    /** @brief Shows the layer inspector strip when enabled. */
    bool showLayerInspector = false;
    /** @brief Shows the page minimap overlay when enabled. */
    bool showMinimap = false;
    /** @brief Shows the problems panel when enabled. */
    bool showProblemsPanel = false;
    /** @brief Enables reticle-usage highlighting once that workflow is available. */
    bool highlightReticleUsages = false;
    /** @brief Shows per-reticle labels in the page preview when enabled. */
    bool showReticleNames = true;
    /** @brief Shows page-preview gizmos such as selection bounds and handles when enabled. */
    bool showGizmos = true;
    /** @brief Keeps the page-context panel visible while editing one library reticle. */
    bool showPageContext = true;
    /** @brief Snaps reticle drags and keyboard nudges to a logical grid when enabled. */
    bool snapToGrid = false;
    /** @brief Logical spacing of the snapping grid. */
    float gridStepLogical = 0.05f;
};
} // namespace editor
