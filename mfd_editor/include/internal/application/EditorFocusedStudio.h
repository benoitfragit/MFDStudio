/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Framing, cached layout and interaction helpers shared by focused LEVEL 1 studios.
 */

#include <cstddef>
#include <optional>
#include <vector>

#include "mfd/apilevel1/ContainerTypes.h"
#include "mfd/apilevel1/widgets/WidgetTypes.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/Types.h"

namespace editor::app
{
/**
 * @brief Owns the derived layout and widget visuals reused by the focused LEVEL 1 studio.
 *
 * The cache is keyed by the page object, selected projected source index and studio kind.
 * Authored mutations must call @ref Invalidate before resolving the next frame.
 */
class FocusedStudioLayoutCache final
{
public:
    /**
     * @brief Resolves and caches the selected layout or selected container subtree.
     * @param page Authored page used to evaluate the mixed container/widget hierarchy.
     * @param sourceIndex Stable projected index of the selected container or widget.
     * @param includeSubtree `true` for a container subtree; `false` for one widget leaf.
     * @return `true` when the selected layout was resolved successfully.
     */
    bool Resolve(const mfd::PageDefinition& page, std::size_t sourceIndex, bool includeSubtree);

    /** @brief Marks all derived data stale while retaining vector capacity for the next rebuild. */
    void Invalidate() noexcept;

    /** @brief Returns whether the current cached selection resolved successfully. */
    [[nodiscard]] bool IsValid() const noexcept;

    /** @brief Returns the selected widget layout or selected container subtree. */
    [[nodiscard]] const std::vector<mfd::api1::ContainerLayoutNode>& Layouts() const noexcept;

    /** @brief Returns the mixed projected catalog used to render container decorations. */
    [[nodiscard]] const std::vector<mfd::api1::ContainerDefinition>& ProjectedNodes() const noexcept;

    /**
     * @brief Finds a cached widget visual by its mixed projected source index.
     * @param sourceIndex Mixed container/widget source index.
     * @param containerCount Number of authored containers preceding widgets in that catalog.
     * @return Cached visual, or `nullptr` for a container, an absent widget or an invalid index.
     */
    [[nodiscard]] const mfd::ReticleGroup* FindWidgetVisual(
        std::size_t sourceIndex,
        std::size_t containerCount) const noexcept;

private:
    const mfd::PageDefinition* pageIdentity_ = nullptr;
    std::size_t sourceIndex_ = 0U;
    bool includeSubtree_ = false;
    bool resolved_ = false;
    bool valid_ = false;
    std::vector<mfd::api1::ContainerDefinition> projectedNodes_ {};
    std::vector<mfd::api1::ContainerLayoutNode> layouts_ {};
    std::vector<std::optional<mfd::ReticleGroup>> widgetVisuals_ {};
};

/**
 * @brief Computes a centered view fitting one logical rectangle inside a studio viewport.
 * @param bounds Logical rectangle to frame.
 * @param viewportWidth Available viewport width in pixels.
 * @param viewportHeight Available viewport height in pixels.
 * @param padding Fraction of the viewport available to content, clamped to `[0.1, 1.0]`.
 * @return Centered view with a finite zoom in `[0.1, 20.0]`, or the default view for invalid input.
 */
[[nodiscard]] mfd::PageViewState ComputeFocusedStudioView(
    const mfd::api1::ContainerRect& bounds,
    float viewportWidth,
    float viewportHeight,
    float padding = 0.78f) noexcept;

/**
 * @brief Finds an evaluated layout by its stable source-catalog index.
 * @param layouts Evaluated mixed container/widget layouts.
 * @param sourceIndex Index in the projected source catalog.
 * @return Matching layout, or `nullptr` when the source is absent.
 * @note Mixed LEVEL 1 layout identifiers are prefixed during projection; source indices therefore
 *       remain the authoritative link back to authored containers and widgets.
 */
[[nodiscard]] const mfd::api1::ContainerLayoutNode* FindFocusedStudioLayout(
    const std::vector<mfd::api1::ContainerLayoutNode>& layouts,
    std::size_t sourceIndex) noexcept;

/**
 * @brief Extracts one container and its contiguous preorder descendants from evaluated layouts.
 * @param layouts Full validated layout in parent-before-child preorder.
 * @param rootSourceIndex Source index of the selected container.
 * @return Independent focused layout whose first node is promoted to a root; empty when absent or
 *         when the input exceeds the documented combined container/widget limit.
 * @note Widget leaves below the selected container are retained because the operation is based on
 *       preorder depth rather than on the authored container catalog alone.
 */
[[nodiscard]] std::vector<mfd::api1::ContainerLayoutNode> CollectFocusedContainerLayouts(
    const std::vector<mfd::api1::ContainerLayoutNode>& layouts,
    std::size_t rootSourceIndex);

/**
 * @brief Reports whether a container resize produced a real authored geometry change.
 * @param currentGeometry Current live geometry.
 * @param nextGeometry Candidate geometry computed from the resize gesture.
 * @return `true` when at least one center or size component differs.
 */
[[nodiscard]] bool ContainerGeometryChanged(
    const mfd::api1::ContainerRect& currentGeometry,
    const mfd::api1::ContainerRect& nextGeometry) noexcept;
} // namespace editor::app
