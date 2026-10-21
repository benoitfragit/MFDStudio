/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "ContainerDecorationRenderer.h"

#include <algorithm>
#include <cmath>

#include <raylib.h>

namespace mfd::detail
{
namespace
{
Color ToRayColor(const ColorRgba& color) noexcept
{
    return Color {color.r, color.g, color.b, color.a};
}

Rectangle ToScreenRect(const api1::ContainerRect& rect,
                       const PageViewState& view,
                       const int width,
                       const int height) noexcept
{
    const Vec2 center = ApplyPageView(rect.center, view);
    const float screenExtent = static_cast<float>(std::min(width, height));
    const float scale = screenExtent * 0.5f * SanitizeZoom(view.zoom);
    const float pixelWidth = std::max(0.0f, rect.size.x * scale);
    const float pixelHeight = std::max(0.0f, rect.size.y * scale);
    return {static_cast<float>(width) * 0.5f + center.x * screenExtent * 0.5f - pixelWidth * 0.5f,
            static_cast<float>(height) * 0.5f - center.y * screenExtent * 0.5f - pixelHeight * 0.5f,
            pixelWidth,
            pixelHeight};
}

Rectangle IntersectRectangles(const Rectangle first, const Rectangle second) noexcept
{
    const float left = std::max(first.x, second.x);
    const float top = std::max(first.y, second.y);
    const float right = std::min(first.x + first.width, second.x + second.width);
    const float bottom = std::min(first.y + first.height, second.y + second.height);
    return {left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top)};
}

bool Contains(const Rectangle rectangle, const Vec2 point) noexcept
{
    return point.x >= rectangle.x && point.x <= rectangle.x + rectangle.width &&
           point.y >= rectangle.y && point.y <= rectangle.y + rectangle.height;
}

class ScissorModeScope final
{
public:
    /** @brief Begins one clipped drawing scope. */
    explicit ScissorModeScope(const Rectangle clip) noexcept
    {
        const int x = static_cast<int>(std::floor(clip.x));
        const int y = static_cast<int>(std::floor(clip.y));
        const int right = static_cast<int>(std::ceil(clip.x + clip.width));
        const int bottom = static_cast<int>(std::ceil(clip.y + clip.height));
        BeginScissorMode(x, y, std::max(0, right - x), std::max(0, bottom - y));
    }

    /** @brief Restores the previous unclipped drawing state. */
    ~ScissorModeScope() { EndScissorMode(); }

    ScissorModeScope(const ScissorModeScope&) = delete;
    ScissorModeScope& operator=(const ScissorModeScope&) = delete;
};

void DrawRoundedRectangle(const Rectangle rectangle,
                          const float logicalRadius,
                          const float logicalScale,
                          const Color color)
{
    const float pixelRadius = std::max(0.0f, logicalRadius * logicalScale);
    const float minimumExtent = std::min(rectangle.width, rectangle.height);
    const float roundness = minimumExtent <= 0.0f ? 0.0f : std::clamp(pixelRadius / minimumExtent, 0.0f, 0.5f);
    DrawRectangleRounded(rectangle, roundness, 8, color);
}

Color ResolveThumbColor(const api1::ScrollbarAppearance& appearance,
                        const Rectangle thumb,
                        const ContainerDecorationPointerState& pointer,
                        const bool active) noexcept
{
    if (active)
    {
        return ToRayColor(appearance.thumbActiveColor);
    }
    if (!pointer.available || !Contains(thumb, pointer.positionPixels))
    {
        return ToRayColor(appearance.thumbColor);
    }
    return ToRayColor(pointer.primaryButtonDown ? appearance.thumbActiveColor : appearance.thumbHoverColor);
}

/** @brief Draws foreground decorations after a container's descendants. */
void DrawContainerForeground(const api1::ContainerDefinition& definition,
                             const api1::ContainerLayoutNode& layout,
                             const PageViewState& view,
                             const int width,
                             const int height,
                             const float logicalScale,
                             const ContainerDecorationPointerState& pointer)
{
    const Rectangle bounds = ToScreenRect(layout.bounds, view, width, height);
    if (definition.appearance.borderVisible)
    {
        const float thickness = std::max(1.0f, definition.appearance.borderThickness * logicalScale);
        const float minimumExtent = std::min(bounds.width, bounds.height);
        const float roundness = minimumExtent <= 0.0f
                                    ? 0.0f
                                    : std::clamp(definition.appearance.cornerRadius * logicalScale /
                                                     minimumExtent,
                                                 0.0f,
                                                 0.5f);
        DrawRectangleRoundedLinesEx(bounds, roundness, 8, thickness,
                                    ToRayColor(definition.appearance.borderColor));
    }

    const api1::ScrollbarAppearance& appearance = definition.scrolling.appearance;
    const api1::ScrollbarGeometry* scrollbars[] = {&layout.horizontalScrollbar, &layout.verticalScrollbar};
    for (std::size_t axis = 0U; axis < 2U; ++axis)
    {
        const auto& scrollbar = *scrollbars[axis];
        if (!scrollbar.visible)
        {
            continue;
        }
        DrawRoundedRectangle(ToScreenRect(scrollbar.track, view, width, height),
                             appearance.cornerRadius, logicalScale, ToRayColor(appearance.trackColor));
        const Rectangle thumb = ToScreenRect(scrollbar.thumb, view, width, height);
        DrawRoundedRectangle(thumb, appearance.cornerRadius, logicalScale,
                             ResolveThumbColor(appearance, thumb, pointer,
                                               pointer.activeSourceIndex == layout.sourceIndex &&
                                                   pointer.activeHorizontal == (axis == 0U)));
    }
}
} // namespace

void DrawContainerDecorations(const std::vector<api1::ContainerDefinition>& definitions,
                              const std::vector<api1::ContainerLayoutNode>& layouts,
                              const PageViewState& view,
                              const int width,
                              const int height,
                              const ContainerDecorationPointerState& pointer)
{
    if (width <= 0 || height <= 0 || definitions.empty() || layouts.empty())
    {
        return;
    }

    const Rectangle fullViewport {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
    const float logicalScale = static_cast<float>(std::min(width, height)) * 0.5f * SanitizeZoom(view.zoom);
    std::vector<Rectangle> effectiveClips(definitions.size(), fullViewport);
    // Layout is preorder: close each subtree before painting its next sibling.
    // This keeps parent scrollbars above children without lifting earlier overlay
    // siblings above later ones. The validated hierarchy bounds this stack to 64.
    std::vector<const api1::ContainerLayoutNode*> foregroundStack;
    foregroundStack.reserve(api1::kMaximumContainerDepth);
    for (const api1::ContainerLayoutNode& layout : layouts)
    {
        while (!foregroundStack.empty() && foregroundStack.back()->sourceIndex != layout.parentSourceIndex)
        {
            const api1::ContainerLayoutNode& completed = *foregroundStack.back();
            const Rectangle clip = completed.parentSourceIndex < effectiveClips.size()
                                       ? effectiveClips[completed.parentSourceIndex] : fullViewport;
            ScissorModeScope scissor(clip);
            DrawContainerForeground(definitions[completed.sourceIndex], completed, view, width, height,
                                    logicalScale, pointer);
            foregroundStack.pop_back();
        }
        if (layout.sourceIndex >= definitions.size() || !layout.effectiveVisible)
        {
            continue;
        }

        const api1::ContainerDefinition& definition = definitions[layout.sourceIndex];
        const Rectangle parentClip = layout.parentSourceIndex < effectiveClips.size()
                                         ? effectiveClips[layout.parentSourceIndex]
                                         : fullViewport;
        const Rectangle bounds = ToScreenRect(layout.bounds, view, width, height);
        const Rectangle viewport = ToScreenRect(layout.viewport, view, width, height);
        const Rectangle ownClip = IntersectRectangles(parentClip, viewport);
        effectiveClips[layout.sourceIndex] = definition.scrolling.clipContent ? ownClip : parentClip;
        foregroundStack.push_back(&layout);

        ScissorModeScope scissor(parentClip);
        if (definition.appearance.backgroundVisible)
        {
            DrawRoundedRectangle(bounds,
                                 definition.appearance.cornerRadius,
                                 logicalScale,
                                 ToRayColor(definition.appearance.backgroundColor));
        }
    }
    while (!foregroundStack.empty())
    {
        const api1::ContainerLayoutNode& completed = *foregroundStack.back();
        const Rectangle clip = completed.parentSourceIndex < effectiveClips.size()
                                   ? effectiveClips[completed.parentSourceIndex] : fullViewport;
        ScissorModeScope scissor(clip);
        DrawContainerForeground(definitions[completed.sourceIndex], completed, view, width, height,
                                logicalScale, pointer);
        foregroundStack.pop_back();
    }
}
} // namespace mfd::detail
