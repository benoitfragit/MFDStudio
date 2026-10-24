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
    /** @brief Whether the first section is expanded and may absorb spare height. */
    bool pagesExpanded = true;
    /** @brief Whether the second section is expanded and may absorb spare height. */
    bool reticlesExpanded = true;
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
 * @param request Total height, preferred gap, preferred first-section height, both minimum heights
 *        and both expanded states.
 * @return Clamped section heights whose sum never exceeds the requested total height.
 * @note A collapsed section keeps only its minimum header height. When both sections are collapsed,
 *       their pair deliberately leaves the remaining height unused instead of displaying empty panes.
 */
[[nodiscard]] SidebarSectionLayoutResult ComputeSidebarSectionLayout(
    const SidebarSectionLayoutRequest& request) noexcept;

/**
 * @brief Resolves a preferred first-section height without wasting space on an empty peer.
 * @param distributableHeight Height shared by both sections after removing the splitter.
 * @param preferredFirstFraction Persisted user preference in the `[0, 1]` range.
 * @param firstHasContent Whether the first section contains visible authored items.
 * @param secondHasContent Whether the second section contains visible authored items.
 * @param firstMinimumHeight Minimum chrome height retained for the first section.
 * @param secondMinimumHeight Minimum chrome height retained for the second section.
 * @return Preferred first-section height; the final split still clamps it to both minima.
 * @note When only one section has content, it receives all height not required by the empty
 *       section. When both are populated, the persisted fraction remains authoritative.
 */
[[nodiscard]] float ResolveContentAwareSidebarPreferredHeight(
    float distributableHeight,
    float preferredFirstFraction,
    bool firstHasContent,
    bool secondHasContent,
    float firstMinimumHeight,
    float secondMinimumHeight) noexcept;

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
