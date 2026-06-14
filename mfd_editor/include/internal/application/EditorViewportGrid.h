/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal viewport-grid helpers shared by the page preview and reticle studio.
 */

#include <cstdint>

#include <raylib.h>

#include "mfd/model/Types.h"

namespace editor::app
{
/** @brief Minimum logical step supported by the editor grid. */
inline constexpr float kMinGridStepLogical = 0.01f;
/** @brief Maximum logical step supported by the editor grid. */
inline constexpr float kMaxGridStepLogical = 0.5f;
/** @brief Default logical step used when no valid grid configuration exists. */
inline constexpr float kDefaultGridStepLogical = 0.05f;
/** @brief Default major-line cadence expressed in minor grid steps. */
inline constexpr int kViewportGridMajorLineMultiple = 5;
/** @brief Hard cap applied to the visible minor lines per viewport axis. */
inline constexpr std::int64_t kViewportGridMaxMinorLinesPerAxis = 256;
/** @brief Hard cap applied to the visible major lines per viewport axis. */
inline constexpr std::int64_t kViewportGridMaxMajorLinesPerAxis = 96;

/**
 * @brief Rendering input required to build one editor-only viewport grid.
 */
struct ViewportGridInput
{
    /** @brief Viewport width in pixels. */
    int width = 0;
    /** @brief Viewport height in pixels. */
    int height = 0;
    /** @brief Page view used to convert logical coordinates into viewport pixels. */
    mfd::PageViewState view {};
    /** @brief Shared logical snapping step that also drives the visible grid. */
    float gridStepLogical = kDefaultGridStepLogical;
};

/**
 * @brief Bounded grid layout derived from the visible logical viewport range.
 *
 * @details The line indices are expressed in multiples of `gridStepLogical`.
 * Minor lines always use one-step spacing; major lines skip by
 * `majorLineMultiple` steps when rendered.
 */
struct ViewportGridLayout
{
    /** @brief Indicates whether the layout is safe to render. */
    bool valid = false;
    /** @brief Indicates whether minor grid lines remain visible at the current zoom. */
    bool showMinorLines = false;
    /** @brief Sanitized shared logical grid step. */
    float gridStepLogical = kDefaultGridStepLogical;
    /** @brief Major-line cadence expressed in multiples of `gridStepLogical`. */
    int majorLineMultiple = kViewportGridMajorLineMultiple;
    /** @brief First visible vertical line index, expressed in grid-step multiples. */
    std::int64_t firstVerticalIndex = 0;
    /** @brief Last visible vertical line index, expressed in grid-step multiples. */
    std::int64_t lastVerticalIndex = -1;
    /** @brief First visible horizontal line index, expressed in grid-step multiples. */
    std::int64_t firstHorizontalIndex = 0;
    /** @brief Last visible horizontal line index, expressed in grid-step multiples. */
    std::int64_t lastHorizontalIndex = -1;
};

/**
 * @brief Clamps one editor grid step to the supported logical range.
 * @param gridStepLogical Requested logical step.
 * @return A finite positive logical step usable by both grid rendering and snapping.
 */
[[nodiscard]] float SanitizeGridStepLogical(float gridStepLogical) noexcept;

/**
 * @brief Builds the visible grid layout for one viewport without allocating.
 * @param input Viewport pixel size, view, and shared logical step.
 * @return Safe bounded layout describing which grid lines may be rendered.
 */
[[nodiscard]] ViewportGridLayout BuildViewportGridLayout(const ViewportGridInput& input) noexcept;

/**
 * @brief Draws one discreet editor-only grid under the authored content.
 * @param input Viewport pixel size, view, and shared logical step.
 * @param backgroundColor Current viewport background color used to derive a readable neutral palette.
 */
void DrawViewportGrid(const ViewportGridInput& input, Color backgroundColor) noexcept;
} // namespace editor::app
