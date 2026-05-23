/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Typed page-preview hit targets used by the editor selection logic.
 */

namespace editor
{
/**
 * @brief Distinguishes the authored object hit in the page preview.
 */
enum class PagePreviewHitKind
{
    StaticReticle,
    PageStrobe,
    PageTitle
};

/**
 * @brief Identifies one selectable target in the page preview without index encoding.
 */
struct PagePreviewHitTarget
{
    /** @brief Kind of authored object matched by the hit-test. */
    PagePreviewHitKind kind = PagePreviewHitKind::StaticReticle;
    /**
     * @brief Zero-based authored index when the target is one reticle or one strobe.
     * @note The page title ignores this field.
     */
    int index = -1;

    /**
     * @brief Builds a hit target referencing one static page reticle.
     * @param reticleIndex Zero-based index inside `PageDefinition::staticReticles`.
     * @return Typed static-reticle hit target.
     */
    [[nodiscard]] static constexpr PagePreviewHitTarget StaticReticle(const int reticleIndex) noexcept
    {
        return {PagePreviewHitKind::StaticReticle, reticleIndex};
    }

    /**
     * @brief Builds a hit target referencing one authored page strobe.
     * @param strobeIndex Zero-based index inside `PageDefinition::strobes`.
     * @return Typed strobe hit target.
     */
    [[nodiscard]] static constexpr PagePreviewHitTarget PageStrobe(const int strobeIndex) noexcept
    {
        return {PagePreviewHitKind::PageStrobe, strobeIndex};
    }

    /**
     * @brief Builds a hit target referencing the generated page title chrome.
     * @return Typed page-title hit target.
     */
    [[nodiscard]] static constexpr PagePreviewHitTarget PageTitle() noexcept
    {
        return {PagePreviewHitKind::PageTitle, 0};
    }
};

/**
 * @brief Compares two typed page-preview targets.
 * @param lhs Left target to compare.
 * @param rhs Right target to compare.
 * @return `true` when both targets reference the same authored object.
 */
[[nodiscard]] constexpr bool operator==(const PagePreviewHitTarget& lhs, const PagePreviewHitTarget& rhs) noexcept
{
    return lhs.kind == rhs.kind && lhs.index == rhs.index;
}

/**
 * @brief Compares two typed page-preview targets for inequality.
 * @param lhs Left target to compare.
 * @param rhs Right target to compare.
 * @return `true` when the targets reference different authored objects.
 */
[[nodiscard]] constexpr bool operator!=(const PagePreviewHitTarget& lhs, const PagePreviewHitTarget& rhs) noexcept
{
    return !(lhs == rhs);
}
} // namespace editor
