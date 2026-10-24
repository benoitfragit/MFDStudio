/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Pure framing and hierarchy-selection helpers shared by focused LEVEL 1 studios.
 */

#include <cstddef>
#include <vector>

#include "mfd/apilevel1/ContainerTypes.h"
#include "mfd/apilevel1/widgets/WidgetTypes.h"
#include "mfd/model/Types.h"

namespace editor::app
{
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
} // namespace editor::app
