/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal LEVEL 1 decoration adapter compiled into the host renderer.
 */

#include <cstddef>
#include <limits>
#include <vector>

#include "mfd/apilevel1/ContainerTypes.h"
#include "mfd/model/PageDefinition.h"

namespace mfd::detail
{
/** @brief Optional pointer state used to select scrollbar hover and active colours. */
struct ContainerDecorationPointerState
{
    Vec2 positionPixels {};
    bool available = false;
    bool primaryButtonDown = false;
    std::size_t activeSourceIndex = std::numeric_limits<std::size_t>::max();
    bool activeHorizontal = false;
};

/**
 * @brief Draws resolved container backgrounds, borders and scrollbars through the host raylib context.
 * @param definitions Authored container catalog indexed by `ContainerLayoutNode::sourceIndex`.
 * @param layouts Resolved layout nodes in parent-before-child order.
 * @param view Active Page view transform.
 * @param width Render-target width in pixels.
 * @param height Render-target height in pixels.
 * @param pointer Optional pointer state; omit it for a non-interactive editor preview.
 */
void DrawContainerDecorations(
    const std::vector<api1::ContainerDefinition>& definitions,
    const std::vector<api1::ContainerLayoutNode>& layouts,
    const PageViewState& view,
    int width,
    int height,
    const ContainerDecorationPointerState& pointer = {});
} // namespace mfd::detail
