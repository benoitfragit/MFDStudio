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
} // namespace editor
