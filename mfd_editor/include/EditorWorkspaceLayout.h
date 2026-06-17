/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Pure layout helper used by the editor workspace to dock preview-side panels.
 */

namespace editor
{
/**
 * @brief One rectangular region inside the page-preview workspace.
 */
struct WorkspaceLayoutRegion
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    /**
     * @brief Returns whether the region still owns a visible surface.
     * @return `true` when both dimensions are strictly positive.
     */
    [[nodiscard]] bool IsVisible() const noexcept;
};

/**
 * @brief Input parameters describing the docked page-preview workspace.
 */
struct WorkspaceLayoutRequest
{
    float width = 0.0f;
    float height = 0.0f;
    float spacing = 0.0f;
    bool showLeadingPanel = false;
    float leadingPanelWidth = 0.0f;
    float minLeadingPanelWidth = 160.0f;
    bool showBottomPanel = false;
    float bottomPanelHeight = 0.0f;
    float minBottomPanelHeight = 96.0f;
    float minCenterWidth = 180.0f;
    float minCenterHeight = 140.0f;
};

/**
 * @brief Concrete workspace slices returned to the editor shell.
 */
struct WorkspaceLayoutResult
{
    WorkspaceLayoutRegion leadingPanel {};
    WorkspaceLayoutRegion previewPanel {};
    WorkspaceLayoutRegion bottomPanel {};
};

/**
 * @brief Splits the available workspace into an optional leading dock, one preview area and an optional bottom dock.
 * @param request Requested workspace dimensions and panel preferences.
 * @return Clamped visible regions preserving the requested minimum preview size.
 */
[[nodiscard]] WorkspaceLayoutResult ComputeWorkspaceLayout(const WorkspaceLayoutRequest& request) noexcept;

/**
 * @brief Input describing the horizontal page-context / reticle-studio split.
 *
 * @note The reticle studio is the primary surface here: the page-context pane
 * (secondary) compresses toward its minimum and then auto-collapses before the
 * studio loses its own minimum width.
 */
struct StudioSplitRequest
{
    float width = 0.0f;
    float spacing = 0.0f;
    bool showSecondary = false;
    float secondaryPreferredWidth = 0.0f;
    float minSecondaryWidth = 0.0f;
    float minPrimaryWidth = 0.0f;
};

/**
 * @brief Resolved widths of the page-context / reticle-studio split.
 */
struct StudioSplitResult
{
    bool secondaryVisible = false;
    bool secondaryAutoCollapsed = false;
    float secondaryWidth = 0.0f;
    float primaryWidth = 0.0f;
};

/**
 * @brief Splits the library-studio workspace into the page-context pane and the reticle studio.
 * @param request Available width, spacing and the page-context preference and minima.
 * @return Clamped widths; the secondary pane is hidden when both minima cannot coexist.
 *
 * @note Inputs are sanitized: non-finite, negative or too-small values are clamped so
 * the result always stays consistent and the studio keeps a usable surface.
 */
[[nodiscard]] StudioSplitResult ComputeStudioSplitLayout(const StudioSplitRequest& request) noexcept;
} // namespace editor
