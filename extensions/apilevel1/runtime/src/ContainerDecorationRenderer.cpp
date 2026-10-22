/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "ContainerDecorationRenderer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <raylib.h>

namespace mfd::detail
{
namespace
{
Color ToRayColor(const ColorRgba& color) noexcept
{
    return Color {color.r, color.g, color.b, color.a};
}

// Keep transforms and clipping in double: finite float geometry times a finite
// float zoom can exceed both float and int without exceeding double.
struct ScreenRect
{
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

ScreenRect ToScreenRect(const api1::ContainerRect& rect,
                        const PageViewState& view,
                        const int width,
                        const int height) noexcept
{
    const double scale = static_cast<double>(std::min(width, height)) * 0.5 * SanitizeZoom(view.zoom);
    const double pixelWidth = static_cast<double>(rect.size.x) * scale;
    const double pixelHeight = static_cast<double>(rect.size.y) * scale;
    return {width * 0.5 + (static_cast<double>(rect.center.x) - view.center.x) * scale - pixelWidth * 0.5,
            height * 0.5 - (static_cast<double>(rect.center.y) - view.center.y) * scale - pixelHeight * 0.5,
            pixelWidth, pixelHeight};
}

bool IsFiniteRect(const ScreenRect rect) noexcept
{
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
           std::isfinite(rect.width) && std::isfinite(rect.height) &&
           rect.width >= 0.0 && rect.height >= 0.0;
}

ScreenRect IntersectRectangles(const ScreenRect first, const ScreenRect second) noexcept
{
    if (!IsFiniteRect(first) || !IsFiniteRect(second)) return {};
    const double left = std::max(first.x, second.x);
    const double top = std::max(first.y, second.y);
    const double right = std::min(first.x + first.width, second.x + second.width);
    const double bottom = std::min(first.y + first.height, second.y + second.height);
    if (right <= left || bottom <= top) return {};
    return {left, top, right - left, bottom - top};
}

bool ToDrawingRect(const ScreenRect source, Rectangle& destination) noexcept
{
    // Leave headroom for raylib's float vertex arithmetic. Clipping still uses
    // the original double rectangle if its decorations cannot be represented.
    constexpr double limit = static_cast<double>(std::numeric_limits<float>::max()) / 16.0;
    if (!IsFiniteRect(source) || source.width <= 0.0 || source.height <= 0.0 ||
        std::abs(source.x) > limit || std::abs(source.y) > limit ||
        source.width > limit || source.height > limit) return false;
    destination = {static_cast<float>(source.x), static_cast<float>(source.y),
                   static_cast<float>(source.width), static_cast<float>(source.height)};
    return true;
}

bool Contains(const ScreenRect rectangle, const Vec2 point) noexcept
{
    return point.x >= rectangle.x && point.x <= rectangle.x + rectangle.width &&
           point.y >= rectangle.y && point.y <= rectangle.y + rectangle.height;
}

class ScissorModeScope final
{
public:
    /** @brief Begins one clipped drawing scope. */
    explicit ScissorModeScope(const ScreenRect clip, const int width, const int height) noexcept
    {
        // Intersect before rounding or narrowing. All four endpoints now lie
        // in [0, viewport extent], even for offscreen/invalid input rectangles.
        const ScreenRect bounded = IntersectRectangles(
            {0.0, 0.0, static_cast<double>(width), static_cast<double>(height)}, clip);
        const int x = static_cast<int>(std::floor(bounded.x));
        const int y = static_cast<int>(std::floor(bounded.y));
        const int right = static_cast<int>(std::min(static_cast<double>(width),
                                                  std::ceil(bounded.x + bounded.width)));
        const int bottom = static_cast<int>(std::min(static_cast<double>(height),
                                                   std::ceil(bounded.y + bounded.height)));
        BeginScissorMode(x, y, right - x, bottom - y);
    }

    /** @brief Restores the previous unclipped drawing state. */
    ~ScissorModeScope() { EndScissorMode(); }

    ScissorModeScope(const ScissorModeScope&) = delete;
    ScissorModeScope& operator=(const ScissorModeScope&) = delete;
};

void DrawRoundedRectangle(const ScreenRect screenRectangle,
                          const float logicalRadius,
                          const double logicalScale,
                          const Color color)
{
    Rectangle rectangle {};
    if (!ToDrawingRect(screenRectangle, rectangle)) return;
    const float pixelRadius = static_cast<float>(std::min(
        static_cast<double>(logicalRadius) * logicalScale,
        static_cast<double>(std::min(rectangle.width, rectangle.height))));
    const float minimumExtent = std::min(rectangle.width, rectangle.height);
    const float roundness = minimumExtent <= 0.0f ? 0.0f : std::clamp(pixelRadius / minimumExtent, 0.0f, 0.5f);
    DrawRectangleRounded(rectangle, roundness, 8, color);
}

Color ResolveThumbColor(const api1::ScrollbarAppearance& appearance,
                        const ScreenRect thumb,
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
                             const double logicalScale,
                             const ContainerDecorationPointerState& pointer)
{
    Rectangle bounds {};
    if (definition.appearance.borderVisible &&
        ToDrawingRect(ToScreenRect(layout.bounds, view, width, height), bounds))
    {
        const float thickness = static_cast<float>(std::clamp(
            static_cast<double>(definition.appearance.borderThickness) * logicalScale,
            1.0, static_cast<double>(std::numeric_limits<float>::max()) / 16.0));
        const float minimumExtent = std::min(bounds.width, bounds.height);
        const float roundness = minimumExtent <= 0.0f
                                    ? 0.0f
                                    : static_cast<float>(std::clamp(static_cast<double>(definition.appearance.cornerRadius) * logicalScale /
                                                     minimumExtent,
                                                 0.0,
                                                 0.5));
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
        const ScreenRect thumb = ToScreenRect(scrollbar.thumb, view, width, height);
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
                              const ContainerDecorationPointerState& pointer,
                              const std::function<void(std::size_t)>& drawLeaf)
{
    if (width <= 0 || height <= 0 || definitions.empty() || layouts.empty())
    {
        return;
    }

    const ScreenRect fullViewport {0.0, 0.0, static_cast<double>(width), static_cast<double>(height)};
    const double logicalScale = static_cast<double>(std::min(width, height)) * 0.5 * SanitizeZoom(view.zoom);
    std::vector<ScreenRect> effectiveClips(definitions.size(), fullViewport);
    // Layout is preorder: close each subtree before painting its next sibling.
    // This keeps parent scrollbars above children without lifting earlier overlay
    // siblings above later ones. The validated hierarchy bounds this stack to 64.
    std::vector<const api1::ContainerLayoutNode*> foregroundStack;
    foregroundStack.reserve(api1::kMaximumContainerDepth);
    for (const api1::ContainerLayoutNode& layout : layouts)
    {
        // Invisible nodes never open a scope. Skip their complete preorder
        // subtree before comparing parents, so a hidden grandchild cannot pop
        // the still-visible ancestor shared with its later visible sibling.
        if (layout.sourceIndex >= definitions.size() || !layout.effectiveVisible)
        {
            continue;
        }
        while (!foregroundStack.empty() && foregroundStack.back()->sourceIndex != layout.parentSourceIndex)
        {
            const api1::ContainerLayoutNode& completed = *foregroundStack.back();
            const ScreenRect clip = completed.parentSourceIndex < effectiveClips.size()
                                       ? effectiveClips[completed.parentSourceIndex] : fullViewport;
            ScissorModeScope scissor(clip, width, height);
            DrawContainerForeground(definitions[completed.sourceIndex], completed, view, width, height,
                                    logicalScale, pointer);
            foregroundStack.pop_back();
        }

        const api1::ContainerDefinition& definition = definitions[layout.sourceIndex];
        const ScreenRect parentClip = layout.parentSourceIndex < effectiveClips.size()
                                         ? effectiveClips[layout.parentSourceIndex]
                                         : fullViewport;
        const ScreenRect bounds = ToScreenRect(layout.bounds, view, width, height);
        const ScreenRect viewport = ToScreenRect(layout.viewport, view, width, height);
        const ScreenRect ownClip = IntersectRectangles(parentClip, viewport);
        effectiveClips[layout.sourceIndex] = definition.scrolling.clipContent ? ownClip : parentClip;
        foregroundStack.push_back(&layout);

        ScissorModeScope scissor(parentClip, width, height);
        if (definition.appearance.backgroundVisible)
        {
            DrawRoundedRectangle(bounds,
                                 definition.appearance.cornerRadius,
                                 logicalScale,
                                 ToRayColor(definition.appearance.backgroundColor));
        }
        if (drawLeaf) drawLeaf(layout.sourceIndex);
    }
    while (!foregroundStack.empty())
    {
        const api1::ContainerLayoutNode& completed = *foregroundStack.back();
        const ScreenRect clip = completed.parentSourceIndex < effectiveClips.size()
                                   ? effectiveClips[completed.parentSourceIndex] : fullViewport;
        ScissorModeScope scissor(clip, width, height);
        DrawContainerForeground(definitions[completed.sourceIndex], completed, view, width, height,
                                logicalScale, pointer);
        foregroundStack.pop_back();
    }
}
} // namespace mfd::detail
