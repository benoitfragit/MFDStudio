/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorViewportGrid.h"

/**
 * @file
 * @brief Implementation of the shared editor viewport-grid helpers.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace editor::app
{
namespace
{
constexpr float kMinMinorLineSpacingPixels = 10.0f;
constexpr float kMinMajorLineSpacingPixels = 24.0f;
constexpr int kMaxMajorLineMultiple = 4096;

struct LogicalViewportBounds
{
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    bool valid = false;
};

struct ViewportGridPalette
{
    Color minor {};
    Color major {};
    Color axis {};
};

double LogicalScalePixels(const ViewportGridInput& input) noexcept
{
    if (input.width <= 0 || input.height <= 0)
    {
        return 0.0;
    }

    return static_cast<double>(std::min(input.width, input.height)) * 0.5;
}

LogicalViewportBounds ComputeLogicalViewportBounds(const ViewportGridInput& input) noexcept
{
    LogicalViewportBounds bounds;

    const double scalePixels = LogicalScalePixels(input);
    const double zoom = static_cast<double>(mfd::SanitizeZoom(input.view.zoom));
    if (!std::isfinite(scalePixels) || !std::isfinite(zoom) || scalePixels <= 0.0 || zoom <= 0.0)
    {
        return bounds;
    }

    const double halfWidthLogical = static_cast<double>(input.width) * 0.5 / (scalePixels * zoom);
    const double halfHeightLogical = static_cast<double>(input.height) * 0.5 / (scalePixels * zoom);
    if (!std::isfinite(halfWidthLogical) || !std::isfinite(halfHeightLogical))
    {
        return bounds;
    }

    bounds.minX = static_cast<double>(input.view.center.x) - halfWidthLogical;
    bounds.maxX = static_cast<double>(input.view.center.x) + halfWidthLogical;
    bounds.minY = static_cast<double>(input.view.center.y) - halfHeightLogical;
    bounds.maxY = static_cast<double>(input.view.center.y) + halfHeightLogical;
    bounds.valid = std::isfinite(bounds.minX) && std::isfinite(bounds.maxX) && std::isfinite(bounds.minY) &&
                   std::isfinite(bounds.maxY) && bounds.minX <= bounds.maxX && bounds.minY <= bounds.maxY;
    return bounds;
}

std::int64_t ClampGridIndex(const double value) noexcept
{
    constexpr double kMinIndex = static_cast<double>(std::numeric_limits<std::int64_t>::min() / 4);
    constexpr double kMaxIndex = static_cast<double>(std::numeric_limits<std::int64_t>::max() / 4);
    if (!std::isfinite(value))
    {
        return 0;
    }

    return static_cast<std::int64_t>(std::clamp(value, kMinIndex, kMaxIndex));
}

std::int64_t FloorGridIndex(const double logicalValue, const double gridStepLogical) noexcept
{
    if (!std::isfinite(logicalValue) || !std::isfinite(gridStepLogical) || gridStepLogical <= 0.0)
    {
        return 0;
    }

    constexpr double kBoundaryPadding = 1.0e-6;
    return ClampGridIndex(std::floor((logicalValue - kBoundaryPadding) / gridStepLogical));
}

std::int64_t CeilGridIndex(const double logicalValue, const double gridStepLogical) noexcept
{
    if (!std::isfinite(logicalValue) || !std::isfinite(gridStepLogical) || gridStepLogical <= 0.0)
    {
        return -1;
    }

    constexpr double kBoundaryPadding = 1.0e-6;
    return ClampGridIndex(std::ceil((logicalValue + kBoundaryPadding) / gridStepLogical));
}

std::int64_t CountIndexRange(const std::int64_t first, const std::int64_t last) noexcept
{
    if (last < first)
    {
        return 0;
    }

    const long double span = static_cast<long double>(last) - static_cast<long double>(first);
    if (!std::isfinite(static_cast<double>(span)))
    {
        return std::numeric_limits<std::int64_t>::max();
    }

    const long double count = span + 1.0L;
    if (count >= static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
    {
        return std::numeric_limits<std::int64_t>::max();
    }

    return static_cast<std::int64_t>(count);
}

std::int64_t CountVisibleGridLines(const double logicalMin,
                                   const double logicalMax,
                                   const double gridStepLogical) noexcept
{
    return CountIndexRange(
        FloorGridIndex(logicalMin, gridStepLogical),
        CeilGridIndex(logicalMax, gridStepLogical));
}

int ResolveMajorLineMultiple(const LogicalViewportBounds& bounds,
                             const ViewportGridInput& input,
                             const float gridStepLogical) noexcept
{
    const double scalePixels = LogicalScalePixels(input);
    const double zoom = static_cast<double>(mfd::SanitizeZoom(input.view.zoom));
    const double gridPixels = static_cast<double>(gridStepLogical) * scalePixels * zoom;

    int majorLineMultiple = kViewportGridMajorLineMultiple;
    int guardCount = 0;
    while (guardCount < 16)
    {
        const double majorStepLogical = static_cast<double>(gridStepLogical) * static_cast<double>(majorLineMultiple);
        const std::int64_t verticalCount = CountVisibleGridLines(bounds.minX, bounds.maxX, majorStepLogical);
        const std::int64_t horizontalCount = CountVisibleGridLines(bounds.minY, bounds.maxY, majorStepLogical);
        const bool enoughPixelSpacing =
            std::isfinite(gridPixels) && gridPixels * static_cast<double>(majorLineMultiple) >= kMinMajorLineSpacingPixels;
        const bool boundedLineCounts =
            verticalCount <= kViewportGridMaxMajorLinesPerAxis &&
            horizontalCount <= kViewportGridMaxMajorLinesPerAxis;
        if (enoughPixelSpacing && boundedLineCounts)
        {
            break;
        }

        if (majorLineMultiple >= kMaxMajorLineMultiple / 2)
        {
            majorLineMultiple = kMaxMajorLineMultiple;
            break;
        }

        majorLineMultiple *= 2;
        ++guardCount;
    }

    return majorLineMultiple;
}

ViewportGridPalette BuildViewportGridPalette(const Color backgroundColor) noexcept
{
    const float red = static_cast<float>(backgroundColor.r) / 255.0f;
    const float green = static_cast<float>(backgroundColor.g) / 255.0f;
    const float blue = static_cast<float>(backgroundColor.b) / 255.0f;
    const float luminance = 0.2126f * red + 0.7152f * green + 0.0722f * blue;

    if (luminance < 0.45f)
    {
        return ViewportGridPalette {
            Color {224, 232, 238, 28},
            Color {224, 232, 238, 48},
            Color {236, 241, 245, 84}};
    }

    return ViewportGridPalette {
        Color {28, 34, 40, 22},
        Color {28, 34, 40, 40},
        Color {24, 30, 36, 72}};
}

Color BuildPageBorderColor(const Color backgroundColor) noexcept
{
    const float red = static_cast<float>(backgroundColor.r) / 255.0f;
    const float green = static_cast<float>(backgroundColor.g) / 255.0f;
    const float blue = static_cast<float>(backgroundColor.b) / 255.0f;
    const float luminance = 0.2126f * red + 0.7152f * green + 0.0722f * blue;

    // The page border is a stronger guide than the grid, so it keeps a higher
    // opacity while staying tinted toward a readable contrast over the background.
    if (luminance < 0.45f)
    {
        return Color {236, 241, 245, 168};
    }

    return Color {24, 30, 36, 150};
}

bool ShouldDrawLineCoordinate(const float coordinatePixels, const int extentPixels) noexcept
{
    return std::isfinite(coordinatePixels) &&
           coordinatePixels >= -1.0f &&
           coordinatePixels <= static_cast<float>(extentPixels) + 1.0f;
}

float ToTextureX(const ViewportGridInput& input, const double logicalX) noexcept
{
    const double viewedX = (logicalX - static_cast<double>(input.view.center.x)) *
                           static_cast<double>(mfd::SanitizeZoom(input.view.zoom));
    return static_cast<float>(static_cast<double>(input.width) * 0.5 + viewedX * LogicalScalePixels(input));
}

float ToTextureY(const ViewportGridInput& input, const double logicalY) noexcept
{
    const double viewedY = (logicalY - static_cast<double>(input.view.center.y)) *
                           static_cast<double>(mfd::SanitizeZoom(input.view.zoom));
    return static_cast<float>(static_cast<double>(input.height) * 0.5 - viewedY * LogicalScalePixels(input));
}

void DrawVerticalGridLine(const ViewportGridInput& input,
                          const double logicalX,
                          const Color color,
                          const float thicknessPixels) noexcept
{
    const float screenX = ToTextureX(input, logicalX);
    if (!ShouldDrawLineCoordinate(screenX, input.width))
    {
        return;
    }

    DrawLineEx(
        Vector2 {screenX, 0.0f},
        Vector2 {screenX, static_cast<float>(input.height)},
        thicknessPixels,
        color);
}

void DrawHorizontalGridLine(const ViewportGridInput& input,
                            const double logicalY,
                            const Color color,
                            const float thicknessPixels) noexcept
{
    const float screenY = ToTextureY(input, logicalY);
    if (!ShouldDrawLineCoordinate(screenY, input.height))
    {
        return;
    }

    DrawLineEx(
        Vector2 {0.0f, screenY},
        Vector2 {static_cast<float>(input.width), screenY},
        thicknessPixels,
        color);
}

std::int64_t FirstIndexMatchingMultiple(const std::int64_t firstIndex, const int multiple) noexcept
{
    if (multiple <= 1)
    {
        return firstIndex;
    }

    std::int64_t candidateIndex = firstIndex;
    int attempts = 0;
    while ((candidateIndex % multiple) != 0 && attempts < multiple)
    {
        ++candidateIndex;
        ++attempts;
    }

    return candidateIndex;
}
} // namespace

float SanitizeGridStepLogical(const float gridStepLogical) noexcept
{
    if (!std::isfinite(gridStepLogical) || gridStepLogical <= 0.0f)
    {
        return kDefaultGridStepLogical;
    }

    return std::clamp(gridStepLogical, kMinGridStepLogical, kMaxGridStepLogical);
}

ViewportGridLayout BuildViewportGridLayout(const ViewportGridInput& input) noexcept
{
    ViewportGridLayout layout;
    layout.gridStepLogical = SanitizeGridStepLogical(input.gridStepLogical);

    if (input.width <= 0 || input.height <= 0)
    {
        return layout;
    }

    const LogicalViewportBounds bounds = ComputeLogicalViewportBounds(input);
    if (!bounds.valid)
    {
        return layout;
    }

    layout.valid = true;
    layout.firstVerticalIndex = FloorGridIndex(bounds.minX, layout.gridStepLogical);
    layout.lastVerticalIndex = CeilGridIndex(bounds.maxX, layout.gridStepLogical);
    layout.firstHorizontalIndex = FloorGridIndex(bounds.minY, layout.gridStepLogical);
    layout.lastHorizontalIndex = CeilGridIndex(bounds.maxY, layout.gridStepLogical);

    const double scalePixels = LogicalScalePixels(input);
    const double zoom = static_cast<double>(mfd::SanitizeZoom(input.view.zoom));
    const double gridPixels = static_cast<double>(layout.gridStepLogical) * scalePixels * zoom;
    const std::int64_t visibleVerticalMinorLines = CountIndexRange(layout.firstVerticalIndex, layout.lastVerticalIndex);
    const std::int64_t visibleHorizontalMinorLines = CountIndexRange(layout.firstHorizontalIndex, layout.lastHorizontalIndex);
    layout.showMinorLines =
        std::isfinite(gridPixels) &&
        gridPixels >= kMinMinorLineSpacingPixels &&
        visibleVerticalMinorLines <= kViewportGridMaxMinorLinesPerAxis &&
        visibleHorizontalMinorLines <= kViewportGridMaxMinorLinesPerAxis;
    layout.majorLineMultiple = ResolveMajorLineMultiple(bounds, input, layout.gridStepLogical);
    return layout;
}

void DrawViewportGrid(const ViewportGridInput& input, const Color backgroundColor) noexcept
{
    const ViewportGridLayout layout = BuildViewportGridLayout(input);
    if (!layout.valid)
    {
        return;
    }

    const ViewportGridPalette palette = BuildViewportGridPalette(backgroundColor);

    if (layout.showMinorLines)
    {
        for (std::int64_t index = layout.firstVerticalIndex; index <= layout.lastVerticalIndex; ++index)
        {
            if (index == 0 || (index % layout.majorLineMultiple) == 0)
            {
                continue;
            }

            DrawVerticalGridLine(
                input,
                static_cast<double>(index) * static_cast<double>(layout.gridStepLogical),
                palette.minor,
                1.0f);
        }

        for (std::int64_t index = layout.firstHorizontalIndex; index <= layout.lastHorizontalIndex; ++index)
        {
            if (index == 0 || (index % layout.majorLineMultiple) == 0)
            {
                continue;
            }

            DrawHorizontalGridLine(
                input,
                static_cast<double>(index) * static_cast<double>(layout.gridStepLogical),
                palette.minor,
                1.0f);
        }
    }

    for (std::int64_t index = FirstIndexMatchingMultiple(layout.firstVerticalIndex, layout.majorLineMultiple);
         index <= layout.lastVerticalIndex;
         index += layout.majorLineMultiple)
    {
        if (index == 0)
        {
            continue;
        }

        DrawVerticalGridLine(
            input,
            static_cast<double>(index) * static_cast<double>(layout.gridStepLogical),
            palette.major,
            1.0f);
    }

    for (std::int64_t index = FirstIndexMatchingMultiple(layout.firstHorizontalIndex, layout.majorLineMultiple);
         index <= layout.lastHorizontalIndex;
         index += layout.majorLineMultiple)
    {
        if (index == 0)
        {
            continue;
        }

        DrawHorizontalGridLine(
            input,
            static_cast<double>(index) * static_cast<double>(layout.gridStepLogical),
            palette.major,
            1.0f);
    }

    if (layout.firstVerticalIndex <= 0 && layout.lastVerticalIndex >= 0)
    {
        DrawVerticalGridLine(input, 0.0, palette.axis, 1.25f);
    }

    if (layout.firstHorizontalIndex <= 0 && layout.lastHorizontalIndex >= 0)
    {
        DrawHorizontalGridLine(input, 0.0, palette.axis, 1.25f);
    }
}

mfd::Vec2 ComputePageBorderHalfExtent(const int windowWidth, const int windowHeight) noexcept
{
    if (windowWidth <= 0 || windowHeight <= 0)
    {
        return mfd::Vec2 {1.0f, 1.0f};
    }

    const float minDimension = static_cast<float>(std::min(windowWidth, windowHeight));
    return mfd::Vec2 {static_cast<float>(windowWidth) / minDimension,
                      static_cast<float>(windowHeight) / minDimension};
}

void DrawViewportPageBorder(const ViewportGridInput& input,
                            const mfd::Vec2 halfExtent,
                            const Color backgroundColor) noexcept
{
    if (input.width <= 0 || input.height <= 0 || !std::isfinite(halfExtent.x) ||
        !std::isfinite(halfExtent.y) || halfExtent.x <= 0.0f || halfExtent.y <= 0.0f)
    {
        return;
    }

    const Vector2 topLeft {ToTextureX(input, -static_cast<double>(halfExtent.x)),
                           ToTextureY(input, static_cast<double>(halfExtent.y))};
    const Vector2 topRight {ToTextureX(input, static_cast<double>(halfExtent.x)),
                            ToTextureY(input, static_cast<double>(halfExtent.y))};
    const Vector2 bottomRight {ToTextureX(input, static_cast<double>(halfExtent.x)),
                               ToTextureY(input, -static_cast<double>(halfExtent.y))};
    const Vector2 bottomLeft {ToTextureX(input, -static_cast<double>(halfExtent.x)),
                              ToTextureY(input, -static_cast<double>(halfExtent.y))};
    if (!std::isfinite(topLeft.x) || !std::isfinite(topLeft.y) || !std::isfinite(topRight.x) ||
        !std::isfinite(topRight.y) || !std::isfinite(bottomRight.x) || !std::isfinite(bottomRight.y) ||
        !std::isfinite(bottomLeft.x) || !std::isfinite(bottomLeft.y))
    {
        return;
    }

    const Color borderColor = BuildPageBorderColor(backgroundColor);
    constexpr float kBorderThicknessPixels = 2.0f;
    DrawLineEx(topLeft, topRight, kBorderThicknessPixels, borderColor);
    DrawLineEx(topRight, bottomRight, kBorderThicknessPixels, borderColor);
    DrawLineEx(bottomRight, bottomLeft, kBorderThicknessPixels, borderColor);
    DrawLineEx(bottomLeft, topLeft, kBorderThicknessPixels, borderColor);
}
} // namespace editor::app
