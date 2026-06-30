/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal vertical layout helper used by the editor sidebar sections.
 */

namespace editor::app
{
/**
 * @brief Requested vertical split for the sidebar Pages and Reticle-library sections.
 */
struct SidebarSectionLayoutRequest
{
    /** @brief Total vertical space available for both sections combined. */
    float height = 0.0f;
    /** @brief Preferred gap between both sections when room allows it. */
    float sectionSpacing = 0.0f;
    /** @brief Preferred total height of the Pages section, including its fixed chrome. */
    float pagesPreferredHeight = 0.0f;
    /** @brief Minimum usable total height of the Pages section. */
    float pagesMinHeight = 0.0f;
    /** @brief Minimum usable total height of the Reticle-library section. */
    float reticlesMinHeight = 0.0f;
};

/**
 * @brief Concrete sidebar-section heights returned to the shell.
 */
struct SidebarSectionLayoutResult
{
    /** @brief Total height assigned to the Pages section. */
    float pagesHeight = 0.0f;
    /** @brief Actual gap retained between both sections after clamping. */
    float sectionSpacing = 0.0f;
    /** @brief Total height assigned to the Reticle-library section. */
    float reticlesHeight = 0.0f;
};

/**
 * @brief Splits the sidebar body into two stacked sections while preserving both headers.
 * @param request Total height, preferred gap, preferred Pages height and both minimum heights.
 * @return Clamped section heights whose sum never exceeds the requested total height.
 */
[[nodiscard]] SidebarSectionLayoutResult ComputeSidebarSectionLayout(
    const SidebarSectionLayoutRequest& request) noexcept;

/**
 * @brief Translates a section splitter drag into a new Pages fraction of the body height.
 * @param currentPagesHeight Pages-section height drawn this frame, used as the drag origin.
 * @param dragDeltaY Vertical mouse delta applied to the splitter this frame.
 * @param distributableHeight Height shared by both sections, excluding the splitter gap.
 * @return New Pages fraction clamped to `[0, 1]`, or `0.5` when no height is distributable.
 */
[[nodiscard]] float ResolveSidebarPagesFraction(
    float currentPagesHeight, float dragDeltaY, float distributableHeight) noexcept;
} // namespace editor::app
