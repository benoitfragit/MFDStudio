/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Page-preview hit-testing and click-selection implementation extracted from `EditorApplication`.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

#include "internal/application/EditorApplicationAuthoringSupport.h"
#include "internal/application/EditorApplicationInternal.h"

namespace
{
using editor::app::IsPageStrobeIndexVisibleInEditor;
using editor::app::IsReticleVisibleInEditor;
using editor::detail::ApproximateArcPoints;
using editor::detail::Distance;
using editor::detail::FallbackPreviewTextSizeLogical;
using editor::detail::PrimitiveTextOriginX;
using editor::detail::ResolvePreviewMeasurementFont;
using editor::detail::TransformPrimitiveWorldPoint;

bool HasSamePageTitleDisplay(const mfd::PageTitleDisplayDefinition& lhs,
                             const mfd::PageTitleDisplayDefinition& rhs) noexcept
{
    return lhs.visible == rhs.visible &&
           lhs.transform.position.x == rhs.transform.position.x &&
           lhs.transform.position.y == rhs.transform.position.y &&
           lhs.transform.rotationDegrees == rhs.transform.rotationDegrees &&
           lhs.transform.scale.x == rhs.transform.scale.x &&
           lhs.transform.scale.y == rhs.transform.scale.y &&
           lhs.color.r == rhs.color.r &&
           lhs.color.g == rhs.color.g &&
           lhs.color.b == rhs.color.b &&
           lhs.color.a == rhs.color.a &&
           lhs.lineWidth == rhs.lineWidth &&
           lhs.lineStyle == rhs.lineStyle &&
           lhs.decoration == rhs.decoration;
}

bool IsPointInsidePolygon(const std::vector<ImVec2>& polygon, const ImVec2 point) noexcept
{
    if (polygon.size() < 3)
    {
        return false;
    }

    bool inside = false;
    std::size_t previous = polygon.size() - 1U;
    for (std::size_t current = 0; current < polygon.size(); ++current)
    {
        const ImVec2& a = polygon[current];
        const ImVec2& b = polygon[previous];

        const bool intersects =
            ((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) / ((b.y - a.y) + 0.00001f) + a.x);
        if (intersects)
        {
            inside = !inside;
        }

        previous = current;
    }

    return inside;
}

std::vector<mfd::Vec2> ApproximateEllipsePoints(const float width, const float height, const int segments = 48)
{
    const int segmentCount = std::max(12, segments);
    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    std::vector<mfd::Vec2> points;
    points.reserve(static_cast<std::size_t>(segmentCount));

    for (int index = 0; index < segmentCount; ++index)
    {
        const float angle = static_cast<float>(index) / static_cast<float>(segmentCount) * 2.0f * PI;
        points.push_back({std::cos(angle) * halfWidth, std::sin(angle) * halfHeight});
    }

    return points;
}

template <typename TCallback>
void ForEachApproximateArcPoint(const float radius,
                                const float startAngleDegrees,
                                const float endAngleDegrees,
                                const int segments,
                                TCallback&& callback)
{
    editor::detail::ForEachPrimitiveArcPoint(radius, startAngleDegrees, endAngleDegrees, segments, callback);
}

struct PageClipPrimitiveHit
{
    int reticleIndex = -1;
    int primitiveIndex = -1;
    float primitiveDistance = std::numeric_limits<float>::max();
    float reticleDistance = std::numeric_limits<float>::max();
};

float EditorViewportZoom(const editor::app::ViewportState& viewport) noexcept
{
    return mfd::SanitizeZoom(viewport.view.zoom);
}

float ToEditorViewPixels(const editor::app::ViewportState& viewport, const float logicalValue) noexcept
{
    return logicalValue * viewport.LogicalScale() * EditorViewportZoom(viewport);
}

Vector2 RotateScreenOffset(const Vector2 offset, const float rotationDegrees) noexcept
{
    const float radians = rotationDegrees * PI / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    return Vector2 {
        offset.x * cosine - offset.y * sine,
        offset.x * sine + offset.y * cosine};
}

void IncludeScreenPoint(editor::app::ReticleScreenBounds& bounds, const ImVec2 point) noexcept
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y))
    {
        return;
    }

    if (!bounds.valid)
    {
        bounds.min = point;
        bounds.max = point;
        bounds.valid = true;
        return;
    }

    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
}

class PrimitiveScreenBoundsAccumulator
{
public:
    PrimitiveScreenBoundsAccumulator(editor::app::ReticleScreenBounds& bounds,
                                     const editor::app::ViewportState& viewport,
                                     const mfd::ReticleGroup& reticle,
                                     const mfd::Primitive& primitive) noexcept
        : bounds_(bounds)
        , viewport_(viewport)
        , reticle_(reticle)
        , primitive_(primitive)
    {
    }

    void operator()(const mfd::Vec2 localPoint) const
    {
        IncludeScreenPoint(bounds_, viewport_.ToScreen(TransformPrimitiveWorldPoint(reticle_, primitive_, localPoint)));
    }

private:
    editor::app::ReticleScreenBounds& bounds_;
    const editor::app::ViewportState& viewport_;
    const mfd::ReticleGroup& reticle_;
    const mfd::Primitive& primitive_;
};

void IncludeAlignedTextLogicalBounds(editor::app::ReticleScreenBounds& bounds,
                                     const editor::app::ViewportState& viewport,
                                     const mfd::ReticleGroup& reticle,
                                     const mfd::Primitive& primitive,
                                     const float width,
                                     const float halfHeight,
                                     const mfd::Align align)
{
    const float originX = PrimitiveTextOriginX(width, align);
    const float left = -originX;
    const float right = width - originX;

    IncludeScreenPoint(bounds, viewport.ToScreen(TransformPrimitiveWorldPoint(reticle, primitive, {left, -halfHeight})));
    IncludeScreenPoint(bounds, viewport.ToScreen(TransformPrimitiveWorldPoint(reticle, primitive, {right, -halfHeight})));
    IncludeScreenPoint(bounds, viewport.ToScreen(TransformPrimitiveWorldPoint(reticle, primitive, {right, halfHeight})));
    IncludeScreenPoint(bounds, viewport.ToScreen(TransformPrimitiveWorldPoint(reticle, primitive, {left, halfHeight})));
}

bool IncludeTextLayoutScreenBounds(editor::app::ReticleScreenBounds& bounds,
                                   const editor::app::ViewportState& viewport,
                                   const mfd::ReticleGroup& reticle,
                                   const mfd::Primitive& primitive,
                                   const mfd::CachedTextLayout& layout)
{
    const mfd::Transform2D combinedTransform = mfd::ResolvePrimitiveWorldTransform(primitive, reticle);
    if (!std::isfinite(layout.size.x) || !std::isfinite(layout.size.y) ||
        !std::isfinite(layout.origin.x) || !std::isfinite(layout.origin.y) ||
        !std::isfinite(combinedTransform.rotationDegrees) ||
        layout.size.x <= 0.0f || layout.size.y <= 0.0f)
    {
        return false;
    }

    const float left = -layout.origin.x;
    const float right = layout.size.x - layout.origin.x;
    const float top = -layout.origin.y;
    const float bottom = layout.size.y - layout.origin.y;
    const ImVec2 anchor = viewport.ToScreen(TransformPrimitiveWorldPoint(reticle, primitive, {}));
    const float screenRotationDegrees = -combinedTransform.rotationDegrees;

    const std::array<Vector2, 4> offsets {{
        {left, top},
        {right, top},
        {right, bottom},
        {left, bottom}}};
    for (const Vector2 offset : offsets)
    {
        const Vector2 rotatedOffset = RotateScreenOffset(offset, screenRotationDegrees);
        IncludeScreenPoint(bounds, ImVec2(anchor.x + rotatedOffset.x, anchor.y + rotatedOffset.y));
    }

    return true;
}

bool IncludeMeasuredTextScreenBounds(editor::app::ReticleScreenBounds& bounds,
                                     const editor::app::ViewportState& viewport,
                                     const mfd::ReticleGroup& reticle,
                                     const mfd::Primitive& primitive,
                                     mfd::TextLayoutCache& textLayoutCache,
                                     const mfd::TextGeometry& text,
                                     const Font* const previewFont)
{
    const Font font = ResolvePreviewMeasurementFont(previewFont);
    const float textScale = mfd::PrimitiveAverageScale(primitive, reticle);
    const float fontSizePixels = std::max(1.0f, std::abs(ToEditorViewPixels(viewport, text.fontSize * textScale)));
    const float letterSpacingPixels = std::isfinite(text.letterSpacing)
                                          ? ToEditorViewPixels(viewport, text.letterSpacing * textScale)
                                          : ToEditorViewPixels(viewport, mfd::kDefaultTextLetterSpacing * textScale);
    const mfd::CachedTextLayout& layout =
        textLayoutCache.ResolveStaticText(text.text, font, fontSizePixels, letterSpacingPixels, text.align);

    return IncludeTextLayoutScreenBounds(bounds, viewport, reticle, primitive, layout);
}

bool IncludeMeasuredTimeScreenBounds(editor::app::ReticleScreenBounds& bounds,
                                     const editor::app::ViewportState& viewport,
                                     const mfd::ReticleGroup& reticle,
                                     const mfd::Primitive& primitive,
                                     mfd::TextLayoutCache& textLayoutCache,
                                     const mfd::TimeGeometry& time,
                                     const Font* const previewFont)
{
    const Font font = ResolvePreviewMeasurementFont(previewFont);
    const float textScale = mfd::PrimitiveAverageScale(primitive, reticle);
    const float fontSizePixels = std::max(1.0f, std::abs(ToEditorViewPixels(viewport, time.fontSize * textScale)));
    const float letterSpacingPixels = std::isfinite(time.letterSpacing)
                                          ? ToEditorViewPixels(viewport, time.letterSpacing * textScale)
                                          : ToEditorViewPixels(viewport, mfd::kDefaultTextLetterSpacing * textScale);
    const mfd::CachedTextLayout& layout = textLayoutCache.ResolveTimeText(time, font, fontSizePixels, letterSpacingPixels);

    return IncludeTextLayoutScreenBounds(bounds, viewport, reticle, primitive, layout);
}
} // namespace

EditorApplication::ReticleScreenBounds EditorApplication::ComputePrimitiveScreenBounds(
    const mfd::ReticleGroup& reticle,
    const mfd::Primitive& primitive,
    const ViewportState& viewport) const
{
    ReticleScreenBounds bounds;
    if (!primitive.style.visible)
    {
        return bounds;
    }

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        mfd::TextLayoutCache textLayoutCache;
        if (!IncludeMeasuredTextScreenBounds(
                bounds,
                viewport,
                reticle,
                primitive,
                textLayoutCache,
                *text,
                PreviewTextFont()))
        {
            const mfd::Vec2 size = FallbackPreviewTextSizeLogical(*text);
            IncludeAlignedTextLogicalBounds(bounds, viewport, reticle, primitive, size.x, size.y * 0.5f, text->align);
        }
    }
    else if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        mfd::TextLayoutCache textLayoutCache;
        if (!IncludeMeasuredTimeScreenBounds(
                bounds,
                viewport,
                reticle,
                primitive,
                textLayoutCache,
                *time,
                PreviewTextFont()))
        {
            const mfd::Vec2 size = FallbackPreviewTextSizeLogical(*time);
            IncludeAlignedTextLogicalBounds(bounds, viewport, reticle, primitive, size.x, size.y * 0.5f, time->align);
        }
    }
    else
    {
        const PrimitiveScreenBoundsAccumulator accumulator(bounds, viewport, reticle, primitive);
        editor::detail::ForEachPrimitiveBoundsLocalPoint(primitive, accumulator);
    }

    if (bounds.valid)
    {
        bounds.center = ImVec2((bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f);
    }

    return bounds;
}

EditorApplication::ReticleScreenBounds EditorApplication::ComputeReticleScreenBounds(
    const mfd::ReticleGroup& reticle,
    const ViewportState& viewport) const
{
    ReticleScreenBounds bounds;

    for (const auto& primitive : reticle.primitives)
    {
        const ReticleScreenBounds primitiveBounds = ComputePrimitiveScreenBounds(reticle, primitive, viewport);
        if (!primitiveBounds.valid)
        {
            continue;
        }

        if (!bounds.valid)
        {
            bounds = primitiveBounds;
            continue;
        }

        bounds.min.x = std::min(bounds.min.x, primitiveBounds.min.x);
        bounds.min.y = std::min(bounds.min.y, primitiveBounds.min.y);
        bounds.max.x = std::max(bounds.max.x, primitiveBounds.max.x);
        bounds.max.y = std::max(bounds.max.y, primitiveBounds.max.y);
    }

    if (bounds.valid)
    {
        bounds.center = ImVec2((bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f);
    }

    return bounds;
}

float EditorApplication::PrimitiveHitDistancePixels(const mfd::ReticleGroup& reticle,
                                                    const mfd::Primitive& primitive,
                                                    const ViewportState& viewport,
                                                    const ImVec2 mousePosition) const
{
    const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(reticle, primitive, viewport);
    if (!bounds.valid)
    {
        return std::numeric_limits<float>::max();
    }

    const auto distanceToPoint = [mousePosition](const ImVec2 point)
    {
        const float dx = point.x - mousePosition.x;
        const float dy = point.y - mousePosition.y;
        return std::sqrt(dx * dx + dy * dy);
    };

    auto distanceToSegment = [mousePosition](const ImVec2 a, const ImVec2 b)
    {
        const float abx = b.x - a.x;
        const float aby = b.y - a.y;
        const float apx = mousePosition.x - a.x;
        const float apy = mousePosition.y - a.y;
        const float lengthSquared = abx * abx + aby * aby;
        if (lengthSquared <= 0.0001f)
        {
            const float dx = a.x - mousePosition.x;
            const float dy = a.y - mousePosition.y;
            return std::sqrt(dx * dx + dy * dy);
        }

        const float factor = std::clamp((apx * abx + apy * aby) / lengthSquared, 0.0f, 1.0f);
        const ImVec2 projection(a.x + abx * factor, a.y + aby * factor);
        const float dx = projection.x - mousePosition.x;
        const float dy = projection.y - mousePosition.y;
        return std::sqrt(dx * dx + dy * dy);
    };

    float bestDistance = distanceToPoint(bounds.center);

    auto toScreenPoint = [&viewport, &reticle, &primitive](const mfd::Vec2 localPoint)
    {
        return viewport.ToScreen(TransformPrimitiveWorldPoint(reticle, primitive, localPoint));
    };

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        if (mousePosition.x >= bounds.min.x - 6.0f && mousePosition.x <= bounds.max.x + 6.0f &&
            mousePosition.y >= bounds.min.y - 6.0f && mousePosition.y <= bounds.max.y + 6.0f)
        {
            bestDistance = std::min(bestDistance, 2.0f);
        }
        return bestDistance;
    }

    if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        if (mousePosition.x >= bounds.min.x - 6.0f && mousePosition.x <= bounds.max.x + 6.0f &&
            mousePosition.y >= bounds.min.y - 6.0f && mousePosition.y <= bounds.max.y + 6.0f)
        {
            bestDistance = std::min(bestDistance, 2.0f);
        }
        return bestDistance;
    }

    if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        return std::min(bestDistance, distanceToSegment(toScreenPoint(line->start), toScreenPoint(line->end)));
    }

    if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
    {
        const std::array<ImVec2, 4> imageCorners {
            toScreenPoint({-image->width * 0.5f, -image->height * 0.5f}),
            toScreenPoint({image->width * 0.5f, -image->height * 0.5f}),
            toScreenPoint({image->width * 0.5f, image->height * 0.5f}),
            toScreenPoint({-image->width * 0.5f, image->height * 0.5f})};
        std::vector<ImVec2> polygon(imageCorners.begin(), imageCorners.end());
        if (IsPointInsidePolygon(polygon, mousePosition))
        {
            return 1.0f;
        }

        for (std::size_t index = 0; index < imageCorners.size(); ++index)
        {
            const std::size_t nextIndex = (index + 1U) % imageCorners.size();
            bestDistance = std::min(bestDistance, distanceToSegment(imageCorners[index], imageCorners[nextIndex]));
        }

        return bestDistance;
    }

    if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        const ImVec2 center = toScreenPoint({});
        const ImVec2 edge = toScreenPoint({circle->radius, 0.0f});
        const float radiusPixels = std::max(1.0f, Distance(center, edge));
        const float centerDistance = distanceToPoint(center);
        bestDistance = std::min(bestDistance, std::abs(centerDistance - radiusPixels));
        if (centerDistance <= radiusPixels + 6.0f)
        {
            bestDistance = std::min(bestDistance, 3.0f);
        }
        return bestDistance;
    }

    if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        const ImVec2 center = toScreenPoint({});
        const ImVec2 innerEdge = toScreenPoint({ring->innerRadius, 0.0f});
        const ImVec2 outerEdge = toScreenPoint({ring->outerRadius, 0.0f});
        float innerRadiusPixels = Distance(center, innerEdge);
        float outerRadiusPixels = std::max(1.0f, Distance(center, outerEdge));
        if (innerRadiusPixels > outerRadiusPixels)
        {
            std::swap(innerRadiusPixels, outerRadiusPixels);
        }

        const float centerDistance = distanceToPoint(center);
        float ringDistance = 0.0f;
        if (centerDistance < innerRadiusPixels)
        {
            ringDistance = innerRadiusPixels - centerDistance;
        }
        else if (centerDistance > outerRadiusPixels)
        {
            ringDistance = centerDistance - outerRadiusPixels;
        }
        else
        {
            ringDistance = std::min(centerDistance - innerRadiusPixels, outerRadiusPixels - centerDistance);
        }

        if (centerDistance >= innerRadiusPixels - 6.0f && centerDistance <= outerRadiusPixels + 6.0f)
        {
            ringDistance = std::min(ringDistance, 2.0f);
        }

        return ringDistance;
    }

    if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
    {
        const std::vector<mfd::Vec2> logicalArcPoints =
            ApproximateArcPoints(arc->radius, arc->startAngleDegrees, arc->endAngleDegrees, arc->segments);
        std::vector<ImVec2> arcPoints;
        arcPoints.reserve(logicalArcPoints.size());
        for (const auto& point : logicalArcPoints)
        {
            arcPoints.push_back(toScreenPoint(point));
        }

        for (std::size_t index = 0; index + 1U < arcPoints.size(); ++index)
        {
            bestDistance = std::min(bestDistance, distanceToSegment(arcPoints[index], arcPoints[index + 1U]));
        }

        if (primitive.style.filled && arcPoints.size() >= 2U)
        {
            std::vector<ImVec2> sectorPoints;
            sectorPoints.reserve(arcPoints.size() + 1U);
            sectorPoints.push_back(toScreenPoint({}));
            sectorPoints.insert(sectorPoints.end(), arcPoints.begin(), arcPoints.end());
            for (std::size_t index = 0; index + 1U < sectorPoints.size(); ++index)
            {
                bestDistance = std::min(bestDistance, distanceToSegment(sectorPoints[index], sectorPoints[index + 1U]));
            }
            bestDistance = std::min(bestDistance, distanceToSegment(sectorPoints.back(), sectorPoints.front()));
            if (IsPointInsidePolygon(sectorPoints, mousePosition))
            {
                bestDistance = std::min(bestDistance, 2.0f);
            }
        }

        return bestDistance;
    }

    std::vector<ImVec2> points;

    if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint({-rectangle->width * 0.5f, -rectangle->height * 0.5f}),
            toScreenPoint({rectangle->width * 0.5f, -rectangle->height * 0.5f}),
            toScreenPoint({rectangle->width * 0.5f, rectangle->height * 0.5f}),
            toScreenPoint({-rectangle->width * 0.5f, rectangle->height * 0.5f})};
    }
    else if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        for (const auto& point : ApproximateEllipsePoints(ellipse->width, ellipse->height))
        {
            points.push_back(toScreenPoint(point));
        }
    }
    else if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint({-square->width * 0.5f, -square->height * 0.5f}),
            toScreenPoint({square->width * 0.5f, -square->height * 0.5f}),
            toScreenPoint({square->width * 0.5f, square->height * 0.5f}),
            toScreenPoint({-square->width * 0.5f, square->height * 0.5f})};
    }
    else if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint({0.0f, diamond->height * 0.5f}),
            toScreenPoint({diamond->width * 0.5f, 0.0f}),
            toScreenPoint({0.0f, -diamond->height * 0.5f}),
            toScreenPoint({-diamond->width * 0.5f, 0.0f})};
    }
    else if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint(triangle->points[0]),
            toScreenPoint(triangle->points[1]),
            toScreenPoint(triangle->points[2])};
    }
    else if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        for (const auto& point : polyline->points)
        {
            points.push_back(toScreenPoint(point));
        }

        for (std::size_t index = 0; index + 1 < points.size(); ++index)
        {
            bestDistance = std::min(bestDistance, distanceToSegment(points[index], points[index + 1]));
        }

        if (polyline->closed && points.size() > 2)
        {
            bestDistance = std::min(bestDistance, distanceToSegment(points.back(), points.front()));
        }

        return bestDistance;
    }
    else if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        for (const auto& point : bezier->controlPoints)
        {
            points.push_back(toScreenPoint(point));
        }
    }
    else if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint({-image->width * 0.5f, -image->height * 0.5f}),
            toScreenPoint({image->width * 0.5f, -image->height * 0.5f}),
            toScreenPoint({image->width * 0.5f, image->height * 0.5f}),
            toScreenPoint({-image->width * 0.5f, image->height * 0.5f})};
    }

    for (std::size_t index = 0; index < points.size(); ++index)
    {
        bestDistance = std::min(bestDistance, distanceToPoint(points[index]));
        if (index + 1 < points.size())
        {
            bestDistance = std::min(bestDistance, distanceToSegment(points[index], points[index + 1]));
        }
    }

    if (points.size() > 2)
    {
        bestDistance = std::min(bestDistance, distanceToSegment(points.back(), points.front()));
        if (IsPointInsidePolygon(points, mousePosition))
        {
            bestDistance = std::min(bestDistance, 2.0f);
        }
    }

    return bestDistance;
}

float EditorApplication::ReticleHitDistancePixels(const mfd::ReticleGroup& reticle,
                                                  const ViewportState& viewport,
                                                  const ImVec2 mousePosition) const
{
    const ReticleScreenBounds bounds = ComputeReticleScreenBounds(reticle, viewport);
    if (!bounds.valid)
    {
        return std::numeric_limits<float>::max();
    }

    float bestDistance = Distance(bounds.center, mousePosition);
    for (const auto& primitive : reticle.primitives)
    {
        bestDistance = std::min(bestDistance, PrimitiveHitDistancePixels(reticle, primitive, viewport, mousePosition));
    }

    return bestDistance;
}

std::optional<EditorApplication::PageReticleHit> EditorApplication::BuildPageReticleHit(
    const mfd::PageDefinition& page,
    const ViewportState& viewport,
    const ImVec2 mousePosition,
    const editor::PagePreviewHitTarget target,
    const mfd::ReticleGroup& reticle,
    const editor::PagePreviewDrawOrderKey drawOrder) const
{
    const bool isPageTitle = target.kind == editor::PagePreviewHitKind::PageTitle;
    const bool isPageStrobe = target.kind == editor::PagePreviewHitKind::PageStrobe;
    if ((!isPageTitle && !IsReticleVisibleInEditor(page, reticle)) ||
        (!isPageTitle && !isPageStrobe && !IsPageReticleSelectableInCurrentFocus(page, reticle)) ||
        (isPageTitle && !reticle.visible))
    {
        return std::nullopt;
    }

    const ReticleScreenBounds bounds = ComputeReticleScreenBounds(reticle, viewport);
    if (!bounds.valid)
    {
        return std::nullopt;
    }

    float distance = ReticleHitDistancePixels(reticle, viewport, mousePosition);
    const bool mouseInsideBounds =
        mousePosition.x >= bounds.min.x - 8.0f && mousePosition.x <= bounds.max.x + 8.0f &&
        mousePosition.y >= bounds.min.y - 8.0f && mousePosition.y <= bounds.max.y + 8.0f;
    const bool directHit = distance <= 6.0f;
    if (mouseInsideBounds)
    {
        distance = std::min(distance, 4.0f);
    }

    if (!mouseInsideBounds && !directHit && distance > 36.0f)
    {
        return std::nullopt;
    }

    const float area = std::max(1.0f, (bounds.max.x - bounds.min.x) * (bounds.max.y - bounds.min.y));
    return PageReticleHit {target, distance, area, directHit, mouseInsideBounds, drawOrder};
}

bool EditorApplication::PreferPageReticleHit(const PageReticleHit& lhs, const PageReticleHit& rhs) noexcept
{
    if (lhs.drawOrder < rhs.drawOrder)
    {
        return false;
    }

    if (rhs.drawOrder < lhs.drawOrder)
    {
        return true;
    }
    if (lhs.directHit != rhs.directHit)
    {
        return lhs.directHit && !rhs.directHit;
    }
    if (lhs.boundsHit != rhs.boundsHit)
    {
        return lhs.boundsHit && !rhs.boundsHit;
    }
    if (std::abs(lhs.distance - rhs.distance) > 0.25f)
    {
        return lhs.distance < rhs.distance;
    }
    if (std::abs(lhs.area - rhs.area) > 0.5f)
    {
        return lhs.area < rhs.area;
    }

    if (lhs.target.kind != rhs.target.kind)
    {
        return static_cast<int>(lhs.target.kind) > static_cast<int>(rhs.target.kind);
    }

    return lhs.target.index > rhs.target.index;
}

const mfd::ReticleGroup& EditorApplication::BuildPageTitlePreviewReticle(const mfd::PageDefinition& page) const
{
    if (!previewState_.pageTitlePreviewReticleCache.valid ||
        previewState_.pageTitlePreviewReticleCache.pageName != page.name ||
        previewState_.pageTitlePreviewReticleCache.pageTitle != page.title ||
        !HasSamePageTitleDisplay(previewState_.pageTitlePreviewReticleCache.display, page.titleDisplay))
    {
        previewState_.pageTitlePreviewReticleCache.pageName = page.name;
        previewState_.pageTitlePreviewReticleCache.pageTitle = page.title;
        previewState_.pageTitlePreviewReticleCache.display = page.titleDisplay;
        previewState_.pageTitlePreviewReticleCache.reticle =
            mfd::BuildPageTitleDisplayReticle(page.name, page.title, page.titleDisplay);
        previewState_.pageTitlePreviewReticleCache.valid = true;
    }

    return previewState_.pageTitlePreviewReticleCache.reticle;
}

void EditorApplication::UpdateReticleSelectionFromClick(const ViewportState& viewport, const bool additiveSelection)
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return;
    }

    const ImVec2 mousePosition = ImGui::GetMousePos();
    std::optional<PageReticleHit> bestHit;
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page->staticReticles.size()); ++reticleIndex)
    {
        const mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        std::optional<PageReticleHit> candidate = BuildPageReticleHit(
            *page,
            viewport,
            mousePosition,
            editor::PagePreviewHitTarget::StaticReticle(reticleIndex),
            reticle,
            services_.pagePreviewDrawOrder.BuildStaticReticleDrawOrderKey(*page, reticleIndex));
        if (candidate.has_value() && (!bestHit.has_value() || PreferPageReticleHit(*candidate, *bestHit)))
        {
            bestHit = std::move(candidate);
        }
    }

    for (int strobeIndex = 0; strobeIndex < static_cast<int>(page->strobes.size()); ++strobeIndex)
    {
        if (!IsPageStrobeIndexVisibleInEditor(*page, strobeIndex))
        {
            continue;
        }

        std::optional<PageReticleHit> candidate = BuildPageReticleHit(
            *page,
            viewport,
            mousePosition,
            editor::PagePreviewHitTarget::PageStrobe(strobeIndex),
            page->strobes[static_cast<std::size_t>(strobeIndex)].reticle,
            services_.pagePreviewDrawOrder.BuildStrobeDrawOrderKey(static_cast<std::size_t>(strobeIndex)));
        if (candidate.has_value() && (!bestHit.has_value() || PreferPageReticleHit(*candidate, *bestHit)))
        {
            bestHit = std::move(candidate);
        }
    }

    const mfd::ReticleGroup& titleReticle = BuildPageTitlePreviewReticle(*page);
    if (titleReticle.visible)
    {
        std::optional<PageReticleHit> titleHit = BuildPageReticleHit(
            *page,
            viewport,
            mousePosition,
            editor::PagePreviewHitTarget::PageTitle(),
            titleReticle,
            services_.pagePreviewDrawOrder.BuildTitleDrawOrderKey());
        if (titleHit.has_value() && (!bestHit.has_value() || PreferPageReticleHit(*titleHit, *bestHit)))
        {
            bestHit = std::move(titleHit);
        }
    }

    if (!bestHit.has_value())
    {
        if (!additiveSelection)
        {
            SelectPage(documentState_.selection.pageIndex, false);
        }
        return;
    }

    if (bestHit->target.kind == editor::PagePreviewHitKind::PageTitle)
    {
        SelectPageTitle(documentState_.selection.pageIndex);
    }
    else if (bestHit->target.kind == editor::PagePreviewHitKind::PageStrobe)
    {
        SelectPageStrobe(documentState_.selection.pageIndex, bestHit->target.index);
    }
    else
    {
        if (additiveSelection)
        {
            TogglePageReticleSelection(documentState_.selection.pageIndex, bestHit->target.index);
        }
        else
        {
            SelectPageReticle(documentState_.selection.pageIndex, bestHit->target.index);
        }
    }
}

std::vector<int> EditorApplication::CollectPageReticlesAt(const ViewportState& viewport, const ImVec2 mousePosition) const
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return {};
    }

    std::vector<PageReticleHit> hits;
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page->staticReticles.size()); ++reticleIndex)
    {
        const auto& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(*page, reticle) || !IsPageReticleSelectableInCurrentFocus(*page, reticle))
        {
            continue;
        }

        const ReticleScreenBounds bounds = ComputeReticleScreenBounds(reticle, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        float distance = ReticleHitDistancePixels(reticle, viewport, mousePosition);
        const bool mouseInsideBounds =
            mousePosition.x >= bounds.min.x - 8.0f && mousePosition.x <= bounds.max.x + 8.0f &&
            mousePosition.y >= bounds.min.y - 8.0f && mousePosition.y <= bounds.max.y + 8.0f;
        const bool directHit = distance <= 6.0f;
        if (mouseInsideBounds)
        {
            distance = std::min(distance, 4.0f);
        }

        if (!mouseInsideBounds && !directHit && distance > 36.0f)
        {
            continue;
        }

        const float area = std::max(1.0f, (bounds.max.x - bounds.min.x) * (bounds.max.y - bounds.min.y));
        hits.push_back(PageReticleHit {
            editor::PagePreviewHitTarget::StaticReticle(reticleIndex),
            distance,
            area,
            directHit,
            mouseInsideBounds,
            services_.pagePreviewDrawOrder.BuildStaticReticleDrawOrderKey(*page, reticleIndex)});
    }

    std::sort(hits.begin(),
              hits.end(),
              PreferPageReticleHit);

    std::vector<int> reticleIndices;
    reticleIndices.reserve(hits.size());
    for (const PageReticleHit& hit : hits)
    {
        reticleIndices.push_back(hit.target.index);
    }
    return reticleIndices;
}

std::optional<int> EditorApplication::FindNearestPageReticle(const ViewportState& viewport, const ImVec2 mousePosition) const
{
    const std::vector<int> hits = CollectPageReticlesAt(viewport, mousePosition);
    return hits.empty() ? std::nullopt : std::optional<int> {hits.front()};
}

std::vector<EditorApplication::PageClipTarget> EditorApplication::CollectPageClipTargetsAt(
    const ViewportState& viewport,
    const ImVec2 mousePosition) const
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return {};
    }

    std::vector<PageClipPrimitiveHit> hits;
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page->staticReticles.size()); ++reticleIndex)
    {
        const auto& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(*page, reticle))
        {
            continue;
        }

        const float reticleDistance = ReticleHitDistancePixels(reticle, viewport, mousePosition);
        for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle.primitives.size()); ++primitiveIndex)
        {
            const auto& primitive = reticle.primitives[static_cast<std::size_t>(primitiveIndex)];
            if (primitive.id.empty() || !mfd::SupportsReticleClipPrimitive(primitive) || !primitive.style.visible)
            {
                continue;
            }

            const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(reticle, primitive, viewport);
            if (!bounds.valid)
            {
                continue;
            }

            float primitiveDistance = PrimitiveHitDistancePixels(reticle, primitive, viewport, mousePosition);
            const bool mouseInsideBounds =
                mousePosition.x >= bounds.min.x - 8.0f && mousePosition.x <= bounds.max.x + 8.0f &&
                mousePosition.y >= bounds.min.y - 8.0f && mousePosition.y <= bounds.max.y + 8.0f;
            if (mouseInsideBounds)
            {
                primitiveDistance = std::min(primitiveDistance, 3.0f);
            }

            if (!mouseInsideBounds && primitiveDistance > 10.0f)
            {
                continue;
            }

            hits.push_back(PageClipPrimitiveHit {
                reticleIndex,
                primitiveIndex,
                primitiveDistance,
                reticleDistance});
        }
    }

    std::sort(hits.begin(),
              hits.end(),
              [](const PageClipPrimitiveHit& lhs, const PageClipPrimitiveHit& rhs)
              {
                  if (std::abs(lhs.primitiveDistance - rhs.primitiveDistance) > 0.25f)
                  {
                      return lhs.primitiveDistance < rhs.primitiveDistance;
                  }
                  if (std::abs(lhs.reticleDistance - rhs.reticleDistance) > 0.25f)
                  {
                      return lhs.reticleDistance < rhs.reticleDistance;
                  }
                  if (lhs.reticleIndex != rhs.reticleIndex)
                  {
                      return lhs.reticleIndex > rhs.reticleIndex;
                  }
                  return lhs.primitiveIndex > rhs.primitiveIndex;
              });

    std::vector<PageClipTarget> targets;
    targets.reserve(hits.size());
    for (const PageClipPrimitiveHit& hit : hits)
    {
        targets.push_back(PageClipTarget {hit.reticleIndex, hit.primitiveIndex});
    }
    return targets;
}

std::optional<EditorApplication::PageClipTarget> EditorApplication::FindNearestPageClipPrimitive(
    const ViewportState& viewport,
    const ImVec2 mousePosition) const
{
    const std::vector<PageClipTarget> targets = CollectPageClipTargetsAt(viewport, mousePosition);
    return targets.empty() ? std::nullopt : std::optional<PageClipTarget> {targets.front()};
}

std::optional<int> EditorApplication::FindNearestLibraryPrimitive(const ViewportState& viewport,
                                                                  const ImVec2 mousePosition) const
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return std::nullopt;
    }

    float bestDistance = 32.0f;
    float bestArea = std::numeric_limits<float>::max();
    std::optional<int> bestIndex;

    for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle->primitives.size()); ++primitiveIndex)
    {
        const mfd::Primitive& primitive = reticle->primitives[static_cast<std::size_t>(primitiveIndex)];
        const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(*reticle, primitive, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        float distance = PrimitiveHitDistancePixels(*reticle, primitive, viewport, mousePosition);
        if (mousePosition.x >= bounds.min.x - 8.0f && mousePosition.x <= bounds.max.x + 8.0f &&
            mousePosition.y >= bounds.min.y - 8.0f && mousePosition.y <= bounds.max.y + 8.0f)
        {
            distance = std::min(distance, 3.0f);
        }

        const float area = std::max(1.0f, (bounds.max.x - bounds.min.x) * (bounds.max.y - bounds.min.y));
        if (distance < bestDistance - 0.25f ||
            (std::abs(distance - bestDistance) <= 0.25f && area < bestArea))
        {
            bestDistance = distance;
            bestArea = area;
            bestIndex = primitiveIndex;
        }
    }

    return bestIndex;
}
