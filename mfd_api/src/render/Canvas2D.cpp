/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for Canvas2D.
 */

#include "Canvas2D.h"
#include "CanvasFastPath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include "mfd/core/ArrayView.h"
#include "mfd/model/RuntimeBudgets.h"
#include <string>
#include <utility>

#include <rlgl.h>

#include "BezierPolylineCache.h"
#include "ImageTextureCache.h"
#include "OpenGlCompat.h"
#include "PolygonTriangulation.h"
#include "TextAnchorSnap.h"
#include "TextLayoutCache.h"

namespace mfd
{
namespace
{
constexpr float kMinSegmentLength = 0.0001f;
constexpr float kTargetCirclePixelsPerSegment = 3.0f;
constexpr int kMaxPrimitiveSegments = 1024;
constexpr std::size_t kMaxStrokeFragmentsPerSegment = 4096U;
constexpr float kIsotropicScaleEpsilon = 1.0e-4f;

Color ToRayColor(const ColorRgba& color)
{
    return Color {color.r, color.g, color.b, color.a};
}

float Distance(const Vector2& lhs, const Vector2& rhs) noexcept
{
    const float dx = rhs.x - lhs.x;
    const float dy = rhs.y - lhs.y;
    return std::sqrt(dx * dx + dy * dy);
}

Vector2 LerpVector2(const Vector2& lhs, const Vector2& rhs, const float factor) noexcept
{
    return Vector2 {
        lhs.x + (rhs.x - lhs.x) * factor,
        lhs.y + (rhs.y - lhs.y) * factor};
}

bool IsFiniteVector(const Vector2& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

using runtime_validation::IsFiniteVec2;

int SanitizeSegmentCount(const int requested, const int minimum) noexcept
{
    return std::clamp(requested, minimum, kMaxPrimitiveSegments);
}

bool TryMeasureFiniteSegment(const Vector2& start, const Vector2& end, float& segmentLength) noexcept
{
    if (!IsFiniteVector(start) || !IsFiniteVector(end))
    {
        return false;
    }

    segmentLength = Distance(start, end);
    return std::isfinite(segmentLength) && segmentLength > kMinSegmentLength;
}

void FillConvexPolygon(const ArrayView<const Vector2> points, const Color color)
{
    if (points.size() < 3)
    {
        return;
    }

    for (std::size_t index = 1; index + 1 < points.size(); ++index)
    {
        DrawTriangle(points[0], points[index], points[index + 1], color);
    }
}

void FillIndexedTriangles(const ArrayView<const Vector2> points,
                          const ArrayView<const std::size_t> triangleIndices,
                          const Color color)
{
    for (std::size_t index = 0; index + 2 < triangleIndices.size(); index += 3U)
    {
        const std::size_t a = triangleIndices[index];
        const std::size_t b = triangleIndices[index + 1U];
        const std::size_t c = triangleIndices[index + 2U];
        if (a >= points.size() || b >= points.size() || c >= points.size())
        {
            continue;
        }

        DrawTriangle(points[a], points[b], points[c], color);
    }
}

void DrawDottedStrokeSegment(const Vector2 start,
                             const Vector2 end,
                             const float dotSpacing,
                             const float dotRadius,
                             const Color color,
                             float& distanceToNextDot)
{
    float segmentLength = 0.0f;
    if (!TryMeasureFiniteSegment(start, end, segmentLength))
    {
        return;
    }

    if (!std::isfinite(distanceToNextDot) || distanceToNextDot < 0.0f)
    {
        distanceToNextDot = 0.0f;
    }

    std::size_t fragmentCount = 0U;
    while (distanceToNextDot <= segmentLength + 0.0001f)
    {
        const float factor = std::clamp(distanceToNextDot / segmentLength, 0.0f, 1.0f);
        if (!std::isfinite(factor) || ++fragmentCount > kMaxStrokeFragmentsPerSegment)
        {
            break;
        }

        DrawCircleV(LerpVector2(start, end, factor), dotRadius, color);
        distanceToNextDot += dotSpacing;
        if (!std::isfinite(distanceToNextDot))
        {
            break;
        }
    }

    distanceToNextDot -= segmentLength;
    if (!std::isfinite(distanceToNextDot) || distanceToNextDot <= 0.0001f)
    {
        distanceToNextDot = dotSpacing;
    }
}

void DrawDashedStrokeSegment(const Vector2 start,
                             const Vector2 end,
                             const float dashLength,
                             const float gapLength,
                             const float thickness,
                             const Color color,
                             float& distanceToNextTransition,
                             bool& drawingDash)
{
    float segmentLength = 0.0f;
    if (!TryMeasureFiniteSegment(start, end, segmentLength))
    {
        return;
    }

    if (!std::isfinite(distanceToNextTransition) || distanceToNextTransition <= 0.0f)
    {
        distanceToNextTransition = drawingDash ? dashLength : gapLength;
    }

    float traversedLength = 0.0f;
    std::size_t fragmentCount = 0U;
    while (traversedLength < segmentLength)
    {
        if (++fragmentCount > kMaxStrokeFragmentsPerSegment)
        {
            break;
        }

        const float consumedLength = std::min(distanceToNextTransition, segmentLength - traversedLength);
        if (!std::isfinite(consumedLength) || consumedLength <= 0.0f)
        {
            break;
        }

        const Vector2 fragmentStart = LerpVector2(start, end, traversedLength / segmentLength);
        const Vector2 fragmentEnd = LerpVector2(start, end, (traversedLength + consumedLength) / segmentLength);
        if (!IsFiniteVector(fragmentStart) || !IsFiniteVector(fragmentEnd))
        {
            break;
        }

        if (drawingDash)
        {
            float dashFragmentLength = 0.0f;
            if (TryMeasureFiniteSegment(fragmentStart, fragmentEnd, dashFragmentLength))
            {
                DrawLineEx(fragmentStart, fragmentEnd, thickness, color);
            }
        }

        traversedLength += consumedLength;
        distanceToNextTransition -= consumedLength;
        if (!std::isfinite(distanceToNextTransition) || distanceToNextTransition <= 0.0001f)
        {
            drawingDash = !drawingDash;
            distanceToNextTransition = drawingDash ? dashLength : gapLength;
        }
    }
}

int EstimateCircleSegmentCount(const float radiusPixels, const int minimum) noexcept
{
    if (!std::isfinite(radiusPixels) || radiusPixels <= 0.0f)
    {
        return SanitizeSegmentCount(minimum, minimum);
    }

    const double circumferencePixels = 2.0 * static_cast<double>(PI) * static_cast<double>(radiusPixels);
    const double estimatedSegments = std::ceil(
        circumferencePixels / static_cast<double>(kTargetCirclePixelsPerSegment));
    if (!std::isfinite(estimatedSegments))
    {
        return kMaxPrimitiveSegments;
    }

    const double boundedEstimate = std::clamp(estimatedSegments,
                                              static_cast<double>(minimum),
                                              static_cast<double>(kMaxPrimitiveSegments));
    const int estimated = static_cast<int>(boundedEstimate);
    return SanitizeSegmentCount(std::max(minimum, estimated), minimum);
}

// Returns true when a primitive's world scale maps a circle to a true screen-space
// circle (i.e. both axes scale identically), so the cheap raylib ring fast path is
// geometrically exact rather than an approximation of a squashed ellipse.
bool IsIsotropicScale(const Vec2& scale) noexcept
{
    const float scaleX = std::abs(scale.x);
    const float scaleY = std::abs(scale.y);
    return std::abs(scaleX - scaleY) <= kIsotropicScaleEpsilon * std::max({1.0f, scaleX, scaleY});
}

// Fast path for a solid, isotropic circle outline: raylib tessellates the ring in a
// single batched call instead of re-sampling sin/cos and emitting one DrawLineEx per
// segment from C++ every frame. This is the dominant per-frame cost in Debug builds.
void DrawCircleOutlineFast(const Vector2 center,
                           const float radius,
                           const float thickness,
                           const Color color) noexcept
{
    // A degenerate circle drew nothing on the polyline path because every sampled segment had
    // zero length and was filtered out. Preserve that: without this guard DrawRing would still
    // paint a visible dot of outer radius thickness/2.
    if (radius <= 0.0f)
    {
        return;
    }

    const float halfThickness = thickness * 0.5f;
    const float innerRadius = std::max(0.0f, radius - halfThickness);
    const float outerRadius = radius + halfThickness;
    const int segments = EstimateCircleSegmentCount(radius, 64);
    DrawRing(center, innerRadius, outerRadius, 0.0f, 360.0f, segments, color);
}

class ScopedStencilStateReset
{
public:
    ScopedStencilStateReset() = default;

    ~ScopedStencilStateReset() noexcept
    {
        rlDrawRenderBatchActive();
        detail::OpenGlSetColorWriteMask(true, true, true, true);
        detail::OpenGlSetStencilMask(0xFF);
        detail::OpenGlSetStencilOperation(detail::GlStencilOperation::Keep,
                                          detail::GlStencilOperation::Keep,
                                          detail::GlStencilOperation::Keep);
        detail::OpenGlSetStencilFunction(detail::GlStencilCompare::Always, 0, 0xFF);
        detail::OpenGlSetStencilEnabled(false);
    }

    ScopedStencilStateReset(const ScopedStencilStateReset&) = delete;
    ScopedStencilStateReset& operator=(const ScopedStencilStateReset&) = delete;
};

void DrawSolidRingFast(const Vector2 center,
                       const float innerRadius,
                       const float outerRadius,
                       const bool filled,
                       const float strokeThickness,
                       const Color strokeColor,
                       const Color fillColor) noexcept
{
    if (filled && outerRadius > innerRadius)
    {
        const int segments = EstimateCircleSegmentCount(outerRadius, 64);
        DrawRing(center, innerRadius, outerRadius, 0.0f, 360.0f, segments, fillColor);
    }

    DrawCircleOutlineFast(center, outerRadius, strokeThickness, strokeColor);
    DrawCircleOutlineFast(center, innerRadius, strokeThickness, strokeColor);
}

void DrawPolylineStroke(const ArrayView<const Vector2> points,
                        const bool closed,
                        const float thickness,
                        const Color color,
                        const LineStyle lineStyle)
{
    if (points.size() < 2 || !std::isfinite(thickness) || thickness <= 0.0f)
    {
        return;
    }

    if (lineStyle == LineStyle::Solid)
    {
        for (std::size_t index = 0; index + 1 < points.size(); ++index)
        {
            float segmentLength = 0.0f;
            if (TryMeasureFiniteSegment(points[index], points[index + 1], segmentLength))
            {
                DrawLineEx(points[index], points[index + 1], thickness, color);
            }
        }

        if (closed)
        {
            float segmentLength = 0.0f;
            if (TryMeasureFiniteSegment(points.back(), points.front(), segmentLength))
            {
                DrawLineEx(points.back(), points.front(), thickness, color);
            }
        }

        return;
    }

    if (lineStyle == LineStyle::Dotted)
    {
        const float dotSpacing = std::max(4.0f, thickness * 2.25f);
        const float dotRadius = std::max(1.0f, thickness * 0.5f);
        float distanceToNextDot = 0.0f;

        for (std::size_t index = 0; index + 1 < points.size(); ++index)
        {
            DrawDottedStrokeSegment(points[index], points[index + 1], dotSpacing, dotRadius, color, distanceToNextDot);
        }

        if (closed)
        {
            DrawDottedStrokeSegment(points.back(), points.front(), dotSpacing, dotRadius, color, distanceToNextDot);
        }

        return;
    }

    const float dashLength = std::max(6.0f, thickness * 4.0f);
    const float gapLength = std::max(4.0f, thickness * 2.0f);
    float distanceToNextTransition = dashLength;
    bool drawingDash = true;

    for (std::size_t index = 0; index + 1 < points.size(); ++index)
    {
        DrawDashedStrokeSegment(
            points[index],
            points[index + 1],
            dashLength,
            gapLength,
            thickness,
            color,
            distanceToNextTransition,
            drawingDash);
    }

    if (closed)
    {
        DrawDashedStrokeSegment(
            points.back(),
            points.front(),
            dashLength,
            gapLength,
            thickness,
            color,
            distanceToNextTransition,
            drawingDash);
    }
}

void FillRing(const ArrayView<const Vector2> outerPoints,
              const ArrayView<const Vector2> innerPoints,
              const Color color)
{
    if (outerPoints.size() < 3 || outerPoints.size() != innerPoints.size())
    {
        return;
    }

    for (std::size_t index = 0; index < outerPoints.size(); ++index)
    {
        const std::size_t nextIndex = (index + 1U) % outerPoints.size();
        DrawTriangle(outerPoints[index], outerPoints[nextIndex], innerPoints[nextIndex], color);
        DrawTriangle(outerPoints[index], innerPoints[nextIndex], innerPoints[index], color);
    }
}

void SampleArcInto(const ArcGeometry& geometry, std::vector<Vec2>& destination)
{
    const int segmentCount = SanitizeSegmentCount(geometry.segments, 2);
    const float radius = std::max(0.0f, std::abs(geometry.radius));
    const float startAngleRadians = geometry.startAngleDegrees * PI / 180.0f;
    const float sweepRadians = (geometry.endAngleDegrees - geometry.startAngleDegrees) * PI / 180.0f;

    destination.clear();
    destination.reserve(static_cast<std::size_t>(segmentCount) + 1U);

    for (int index = 0; index <= segmentCount; ++index)
    {
        const float factor = static_cast<float>(index) / static_cast<float>(segmentCount);
        const float angle = startAngleRadians + sweepRadians * factor;
        destination.push_back(Vec2 {
            std::cos(angle) * radius,
            std::sin(angle) * radius});
    }
}

void SampleEllipseInto(const EllipseGeometry& geometry, const int segments, std::vector<Vec2>& destination)
{
    const int segmentCount = SanitizeSegmentCount(segments, 12);
    const float halfWidth = geometry.width * 0.5f;
    const float halfHeight = geometry.height * 0.5f;

    destination.clear();
    destination.reserve(static_cast<std::size_t>(segmentCount));

    for (int index = 0; index < segmentCount; ++index)
    {
        const float angle = static_cast<float>(index) / static_cast<float>(segmentCount) * 2.0f * PI;
        destination.push_back(Vec2 {
            std::cos(angle) * halfWidth,
            std::sin(angle) * halfHeight});
    }
}

void DrawAlignedText(const std::string& text,
                     const Font& font,
                     const float fontSize,
                     const float letterSpacing,
                     const Vector2 screenPosition,
                     const Vector2 origin,
                     const float rotationDegrees,
                     const Color color)
{
    const Vector2 snappedScreenPosition = detail::SnapTextAnchor(screenPosition);
    DrawTextPro(font,
                text.c_str(),
                snappedScreenPosition,
                origin,
                rotationDegrees,
                fontSize,
                letterSpacing,
                color);
}

void DrawStaticTextLayout(TextLayoutCache& layoutCache,
                          const std::string& text,
                          const Font& font,
                          const float fontSize,
                          const float letterSpacing,
                          const Align align,
                          const Vector2 screenPosition,
                          const float rotationDegrees,
                          const Color color)
{
    const CachedTextLayout& layout = layoutCache.ResolveStaticText(text, font, fontSize, letterSpacing, align);
    if (!IsFiniteVector(layout.size) || !IsFiniteVector(layout.origin))
    {
        return;
    }

    DrawAlignedText(layout.text, font, fontSize, letterSpacing, screenPosition, layout.origin, rotationDegrees, color);
}

void DrawTimeTextLayout(TextLayoutCache& layoutCache,
                        const TimeGeometry& time,
                        const Font& font,
                        const float fontSize,
                        const float letterSpacing,
                        const Vector2 screenPosition,
                        const float rotationDegrees,
                        const Color color)
{
    const CachedTextLayout& layout = layoutCache.ResolveTimeText(time, font, fontSize, letterSpacing);
    if (!IsFiniteVector(layout.size) || !IsFiniteVector(layout.origin))
    {
        return;
    }

    DrawAlignedText(layout.text, font, fontSize, letterSpacing, screenPosition, layout.origin, rotationDegrees, color);
}
} // namespace

Canvas2D::Canvas2D(const int width,
                   const int height,
                   const PageViewState view,
                   const Font* textFont,
                   const Color backgroundColor,
                   const bool clippingEnabled,
                   BezierPolylineCache* bezierCache,
                   ImageTextureCache* imageCache,
                   TextLayoutCache* textLayoutCache,
                   BackgroundRestoreCallback backgroundRestore)
    : width_(width)
    , height_(height)
    , view_(view)
    , textFont_(textFont)
    , backgroundColor_(backgroundColor)
    , clippingEnabled_(clippingEnabled)
    , backgroundRestore_(std::move(backgroundRestore))
    , bezierCache_(bezierCache)
    , imageCache_(imageCache)
    , textLayoutCache_(textLayoutCache)
{
}

Font Canvas2D::TextFont() const noexcept
{
    return textFont_ != nullptr ? *textFont_ : GetFontDefault();
}

float Canvas2D::LogicalScale() const noexcept
{
    return static_cast<float>(std::min(width_, height_)) * 0.5f;
}

float Canvas2D::ToPixels(const float logicalValue) const noexcept
{
    return logicalValue * LogicalScale();
}

float Canvas2D::ViewZoom() const noexcept
{
    return SanitizeZoom(view_.zoom);
}

float Canvas2D::ToViewPixels(const float logicalValue) const noexcept
{
    return ToPixels(logicalValue * ViewZoom());
}

void Canvas2D::DrawReticle(const ReticleGroup& reticle) const
{
    DrawReticle(reticle, reticle.visible);
}

void Canvas2D::DrawReticle(const ReticleGroup& reticle, const bool visible) const
{
    if (!visible)
    {
        return;
    }

    if (clippingEnabled_)
    {
        if (const Primitive* clipPrimitive = ResolveClipPrimitive(reticle);
            clipPrimitive != nullptr && detail::OpenGlStencilApiAvailable())
        {
            ApplyClipMask(*clipPrimitive, reticle);
        }
    }

    DrawReticlePrimitives(reticle);
}

void Canvas2D::DrawReticleWithoutClipping(const ReticleGroup& reticle, const bool visible) const
{
    if (!visible)
    {
        return;
    }

    DrawReticlePrimitives(reticle);
}

void Canvas2D::DrawReticlePrimitives(const ReticleGroup& reticle) const
{
    for (const auto& primitive : reticle.primitives)
    {
        if (!primitive.style.visible)
        {
            continue;
        }

        DrawPrimitive(primitive, reticle);
    }
}

void Canvas2D::RestoreClippedBackground() const
{
    if (backgroundRestore_)
    {
        backgroundRestore_();
        return;
    }

    DrawRectangle(0, 0, width_, height_, backgroundColor_);
}

void Canvas2D::ApplyClipMask(const Primitive& primitive, const ReticleGroup& group) const
{
    const ScopedStencilStateReset stencilStateReset;
    rlDrawRenderBatchActive();
    detail::OpenGlSetStencilEnabled(true);
    detail::OpenGlSetStencilMask(0xFF);
    detail::OpenGlClearStencilValue(0);
    detail::OpenGlClearStencilBuffer();

    detail::OpenGlSetColorWriteMask(false, false, false, false);
    detail::OpenGlSetStencilFunction(detail::GlStencilCompare::Always, 1, 0xFF);
    detail::OpenGlSetStencilOperation(detail::GlStencilOperation::Replace,
                                      detail::GlStencilOperation::Replace,
                                      detail::GlStencilOperation::Replace);
    if (!DrawClipMaskPrimitive(primitive, group))
    {
        rlDrawRenderBatchActive();
        detail::OpenGlSetColorWriteMask(true, true, true, true);
        detail::OpenGlSetStencilMask(0xFF);
        detail::OpenGlSetStencilOperation(detail::GlStencilOperation::Keep,
                                          detail::GlStencilOperation::Keep,
                                          detail::GlStencilOperation::Keep);
        detail::OpenGlSetStencilFunction(detail::GlStencilCompare::Always, 0, 0xFF);
        detail::OpenGlSetStencilEnabled(false);
        return;
    }

    rlDrawRenderBatchActive();
    detail::OpenGlSetColorWriteMask(true, true, true, true);
    detail::OpenGlSetStencilMask(0x00);
    detail::OpenGlSetStencilOperation(detail::GlStencilOperation::Keep,
                                      detail::GlStencilOperation::Keep,
                                      detail::GlStencilOperation::Keep);
    detail::OpenGlSetStencilFunction(group.clipping.mode == ReticleClipMode::Inner
                                         ? detail::GlStencilCompare::NotEqual
                                         : detail::GlStencilCompare::Equal,
                                      0,
                                      0xFF);
    RestoreClippedBackground();

    rlDrawRenderBatchActive();
    detail::OpenGlSetStencilMask(0xFF);
    detail::OpenGlSetStencilOperation(detail::GlStencilOperation::Keep,
                                      detail::GlStencilOperation::Keep,
                                      detail::GlStencilOperation::Keep);
    detail::OpenGlSetStencilFunction(detail::GlStencilCompare::Always, 0, 0xFF);
    detail::OpenGlSetStencilEnabled(false);
}

bool Canvas2D::DrawClipMaskPrimitive(const Primitive& primitive, const ReticleGroup& group) const
{
    constexpr Color kClipMaskColor {255, 255, 255, 255};

    switch (primitive.type)
    {
    case PrimitiveType::Circle:
    {
        const auto* circle = std::get_if<CircleGeometry>(&primitive.geometry);
        if (circle == nullptr)
        {
            break;
        }

        const float radius = std::max(0.0f,
                                      std::abs(ToViewPixels(circle->radius * PrimitiveAverageScale(primitive, group))));
        const Vector2 center = ToScreen(TransformPoint({}, primitive, group));
        if (!std::isfinite(radius) || !IsFiniteVector(center))
        {
            break;
        }

        DrawCircleV(center, radius, kClipMaskColor);
        return true;
    }
    case PrimitiveType::Rectangle:
    {
        const auto* rectangle = std::get_if<RectangleGeometry>(&primitive.geometry);
        if (rectangle == nullptr)
        {
            break;
        }

        const std::array<Vec2, 4> logicalPoints { {
            {-rectangle->width * 0.5f, -rectangle->height * 0.5f},
            {rectangle->width * 0.5f, -rectangle->height * 0.5f},
            {rectangle->width * 0.5f, rectangle->height * 0.5f},
            {-rectangle->width * 0.5f, rectangle->height * 0.5f}} };
        BuildScreenPointsInto(logicalPoints.data(), logicalPoints.size(), primitive, group, screenScratchA_);
        FillConvexPolygon(screenScratchA_, kClipMaskColor);
        return !screenScratchA_.empty();
    }
    case PrimitiveType::Ellipse:
    {
        const auto* ellipse = std::get_if<EllipseGeometry>(&primitive.geometry);
        if (ellipse == nullptr)
        {
            break;
        }

        SampleEllipseInto(*ellipse, 64, logicalScratchA_);
        BuildScreenPointsInto(logicalScratchA_.data(), logicalScratchA_.size(), primitive, group, screenScratchA_);
        FillConvexPolygon(screenScratchA_, kClipMaskColor);
        return !screenScratchA_.empty();
    }
    case PrimitiveType::Square:
    {
        const auto* square = std::get_if<SquareGeometry>(&primitive.geometry);
        if (square == nullptr)
        {
            break;
        }

        const std::array<Vec2, 4> logicalPoints { {
            {-square->width * 0.5f, -square->height * 0.5f},
            {square->width * 0.5f, -square->height * 0.5f},
            {square->width * 0.5f, square->height * 0.5f},
            {-square->width * 0.5f, square->height * 0.5f}} };
        BuildScreenPointsInto(logicalPoints.data(), logicalPoints.size(), primitive, group, screenScratchA_);
        FillConvexPolygon(screenScratchA_, kClipMaskColor);
        return !screenScratchA_.empty();
    }
    case PrimitiveType::Triangle:
    {
        const auto* triangle = std::get_if<TriangleGeometry>(&primitive.geometry);
        if (triangle == nullptr)
        {
            break;
        }

        BuildScreenPointsInto(triangle->points.data(), triangle->points.size(), primitive, group, screenScratchA_);
        FillConvexPolygon(screenScratchA_, kClipMaskColor);
        return !screenScratchA_.empty();
    }
    default:
        break;
    }

    return false;
}

Vector2 Canvas2D::ToScreen(const Vec2& logical) const noexcept
{
    const Vec2 viewPoint = ApplyPageView(logical, view_);
    return Vector2 {
        static_cast<float>(width_) * 0.5f + ToPixels(viewPoint.x),
        static_cast<float>(height_) * 0.5f - ToPixels(viewPoint.y)};
}

Vec2 Canvas2D::TransformPoint(const Vec2& point,
                              const Primitive& primitive,
                              const ReticleGroup& group) const noexcept
{
    return ApplyPrimitiveWorldTransform(point, primitive, group);
}

void Canvas2D::BuildScreenPointsInto(const Vec2* points,
                                     const std::size_t pointCount,
                                     const Primitive& primitive,
                                     const ReticleGroup& group,
                                     std::vector<Vector2>& destination) const
{
    destination.clear();
    destination.reserve(pointCount);

    for (std::size_t index = 0; index < pointCount; ++index)
    {
        if (!IsFiniteVec2(points[index]))
        {
            continue;
        }

        const Vector2 screenPoint = ToScreen(TransformPoint(points[index], primitive, group));
        if (IsFiniteVector(screenPoint))
        {
            destination.push_back(screenPoint);
        }
    }
}

bool mfd::detail::CanUseFastSolidRingPath(const LineStyle lineStyle,
                                          const Vec2& scale,
                                          const float innerRadiusPixels,
                                          const float outerRadiusPixels) noexcept
{
    return lineStyle == LineStyle::Solid &&
           IsIsotropicScale(scale) &&
           std::isfinite(innerRadiusPixels) &&
           std::isfinite(outerRadiusPixels) &&
           innerRadiusPixels >= 0.0f &&
           outerRadiusPixels > 0.0f &&
           innerRadiusPixels <= outerRadiusPixels;
}

void Canvas2D::DrawPrimitive(const Primitive& primitive, const ReticleGroup& group) const
{
    const PrimitiveStyle style = MergeStyle(primitive.style, group.overrides);
    const Color strokeColor = ToRayColor(style.color);
    const Color fillColor = ToRayColor(style.fillColor);
    const float strokeThickness = std::max(1.0f, std::abs(ToViewPixels(style.thickness)));
    const Transform2D combinedTransform = ResolvePrimitiveWorldTransform(primitive, group);
    if (!std::isfinite(strokeThickness) ||
        !IsFiniteVec2(combinedTransform.position) ||
        !std::isfinite(combinedTransform.rotationDegrees) ||
        !IsFiniteVec2(combinedTransform.scale))
    {
        return;
    }

    switch (primitive.type)
    {
    case PrimitiveType::Text:
    {
        const auto* text = std::get_if<TextGeometry>(&primitive.geometry);
        if (text == nullptr)
        {
            break;
        }

        const Font font = TextFont();
        const float textScale = PrimitiveAverageScale(primitive, group);
        const float fontSize = std::max(1.0f,
                                        std::abs(ToViewPixels(text->fontSize * textScale)));
        const float letterSpacing = std::isfinite(text->letterSpacing)
                                        ? ToViewPixels(text->letterSpacing * textScale)
                                        : ToViewPixels(kDefaultTextLetterSpacing * textScale);
        const Vector2 screenPosition = ToScreen(TransformPoint({}, primitive, group));
        if (!std::isfinite(fontSize) || !std::isfinite(letterSpacing) || !IsFiniteVector(screenPosition))
        {
            break;
        }

        if (textLayoutCache_ != nullptr)
        {
            DrawStaticTextLayout(*textLayoutCache_, text->text, font, fontSize, letterSpacing, text->align,
                                screenPosition, -combinedTransform.rotationDegrees, strokeColor);
        }
        else
        {
            TextLayoutCache localTextLayoutCache;
            DrawStaticTextLayout(localTextLayoutCache, text->text, font, fontSize, letterSpacing, text->align,
                                screenPosition, -combinedTransform.rotationDegrees, strokeColor);
        }
        break;
    }
    case PrimitiveType::Time:
    {
        const auto* time = std::get_if<TimeGeometry>(&primitive.geometry);
        if (time == nullptr)
        {
            break;
        }

        const Font font = TextFont();
        const float textScale = PrimitiveAverageScale(primitive, group);
        const float fontSize = std::max(1.0f,
                                        std::abs(ToViewPixels(time->fontSize * textScale)));
        const float letterSpacing = std::isfinite(time->letterSpacing)
                                        ? ToViewPixels(time->letterSpacing * textScale)
                                        : ToViewPixels(kDefaultTextLetterSpacing * textScale);
        const Vector2 screenPosition = ToScreen(TransformPoint({}, primitive, group));
        if (!std::isfinite(fontSize) || !std::isfinite(letterSpacing) || !IsFiniteVector(screenPosition))
        {
            break;
        }

        if (textLayoutCache_ != nullptr)
        {
            DrawTimeTextLayout(*textLayoutCache_, *time, font, fontSize, letterSpacing,
                              screenPosition, -combinedTransform.rotationDegrees, strokeColor);
        }
        else
        {
            TextLayoutCache localTextLayoutCache;
            DrawTimeTextLayout(localTextLayoutCache, *time, font, fontSize, letterSpacing,
                              screenPosition, -combinedTransform.rotationDegrees, strokeColor);
        }
        break;
    }
    case PrimitiveType::Line:
    {
        const auto* line = std::get_if<LineGeometry>(&primitive.geometry);
        if (line == nullptr)
        {
            break;
        }

        const Vector2 start = ToScreen(TransformPoint(line->start, primitive, group));
        const Vector2 end = ToScreen(TransformPoint(line->end, primitive, group));
        const std::array<Vector2, 2> linePoints {{start, end}};
        DrawPolylineStroke(linePoints, false, strokeThickness, strokeColor, style.lineStyle);
        break;
    }
    case PrimitiveType::Circle:
    {
        const auto* circle = std::get_if<CircleGeometry>(&primitive.geometry);
        if (circle == nullptr)
        {
            break;
        }

        const float radius = std::max(0.0f,
                                      std::abs(ToViewPixels(circle->radius * PrimitiveAverageScale(primitive, group))));
        const Vector2 center = ToScreen(TransformPoint({}, primitive, group));
        if (!std::isfinite(radius) || !IsFiniteVector(center))
        {
            break;
        }

        if (style.filled)
        {
            DrawCircleV(center, radius, fillColor);
        }

        if (style.lineStyle == LineStyle::Solid && IsIsotropicScale(combinedTransform.scale))
        {
            DrawCircleOutlineFast(center, radius, strokeThickness, strokeColor);
            break;
        }

        const int segmentCount = EstimateCircleSegmentCount(radius, 64);
        SampleEllipseInto(
            EllipseGeometry {circle->radius * 2.0f, circle->radius * 2.0f},
            segmentCount,
            logicalScratchA_);
        BuildScreenPointsInto(logicalScratchA_.data(), logicalScratchA_.size(), primitive, group, screenScratchA_);
        DrawPolylineStroke(screenScratchA_, true, strokeThickness, strokeColor, style.lineStyle);
        break;
    }
    case PrimitiveType::Ring:
    {
        const auto* ring = std::get_if<RingGeometry>(&primitive.geometry);
        if (ring == nullptr)
        {
            break;
        }

        const float innerRadiusPixels =
            std::max(0.0f,
                     std::abs(ToViewPixels(ring->innerRadius * PrimitiveAverageScale(primitive, group))));
        const float outerRadiusPixels =
            std::max(0.0f,
                     std::abs(ToViewPixels(ring->outerRadius * PrimitiveAverageScale(primitive, group))));
        const Vector2 center = ToScreen(TransformPoint({}, primitive, group));
        if (!std::isfinite(innerRadiusPixels) ||
            !std::isfinite(outerRadiusPixels) ||
            innerRadiusPixels > outerRadiusPixels ||
            !IsFiniteVector(center))
        {
            break;
        }

        if (detail::CanUseFastSolidRingPath(style.lineStyle, combinedTransform.scale, innerRadiusPixels, outerRadiusPixels))
        {
            DrawSolidRingFast(
                center,
                innerRadiusPixels,
                outerRadiusPixels,
                style.filled,
                strokeThickness,
                strokeColor,
                fillColor);
            break;
        }

        const int segmentCount =
            std::max(SanitizeSegmentCount(ring->segments, 12), EstimateCircleSegmentCount(outerRadiusPixels, 64));
        SampleEllipseInto(
            EllipseGeometry {ring->outerRadius * 2.0f, ring->outerRadius * 2.0f},
            segmentCount,
            logicalScratchA_);
        SampleEllipseInto(
            EllipseGeometry {ring->innerRadius * 2.0f, ring->innerRadius * 2.0f},
            segmentCount,
            logicalScratchB_);
        BuildScreenPointsInto(
            logicalScratchA_.data(),
            logicalScratchA_.size(),
            primitive,
            group,
            screenScratchA_);
        BuildScreenPointsInto(
            logicalScratchB_.data(),
            logicalScratchB_.size(),
            primitive,
            group,
            screenScratchB_);

        if (style.filled)
        {
            FillRing(screenScratchA_, screenScratchB_, fillColor);
        }

        DrawPolylineStroke(screenScratchA_, true, strokeThickness, strokeColor, style.lineStyle);
        DrawPolylineStroke(screenScratchB_, true, strokeThickness, strokeColor, style.lineStyle);
        break;
    }
    case PrimitiveType::Rectangle:
    {
        const auto* rectangle = std::get_if<RectangleGeometry>(&primitive.geometry);
        if (rectangle == nullptr)
        {
            break;
        }

        const std::array<Vec2, 4> logicalPoints {{
            {-rectangle->width * 0.5f, -rectangle->height * 0.5f},
            {rectangle->width * 0.5f, -rectangle->height * 0.5f},
            {rectangle->width * 0.5f, rectangle->height * 0.5f},
            {-rectangle->width * 0.5f, rectangle->height * 0.5f}}};
        BuildScreenPointsInto(logicalPoints.data(), logicalPoints.size(), primitive, group, screenScratchA_);

        if (style.filled)
        {
            FillConvexPolygon(screenScratchA_, fillColor);
        }

        DrawPolylineStroke(screenScratchA_, true, strokeThickness, strokeColor, style.lineStyle);
        break;
    }
    case PrimitiveType::Ellipse:
    {
        const auto* ellipse = std::get_if<EllipseGeometry>(&primitive.geometry);
        if (ellipse == nullptr)
        {
            break;
        }

        SampleEllipseInto(*ellipse, 64, logicalScratchA_);
        BuildScreenPointsInto(logicalScratchA_.data(), logicalScratchA_.size(), primitive, group, screenScratchA_);

        if (style.filled)
        {
            FillConvexPolygon(screenScratchA_, fillColor);
        }

        DrawPolylineStroke(screenScratchA_, true, strokeThickness, strokeColor, style.lineStyle);
        break;
    }
    case PrimitiveType::Square:
    {
        const auto* square = std::get_if<SquareGeometry>(&primitive.geometry);
        if (square == nullptr)
        {
            break;
        }

        const std::array<Vec2, 4> logicalPoints {{
            {-square->width * 0.5f, -square->height * 0.5f},
            {square->width * 0.5f, -square->height * 0.5f},
            {square->width * 0.5f, square->height * 0.5f},
            {-square->width * 0.5f, square->height * 0.5f}}};
        BuildScreenPointsInto(logicalPoints.data(), logicalPoints.size(), primitive, group, screenScratchA_);

        if (style.filled)
        {
            FillConvexPolygon(screenScratchA_, fillColor);
        }

        DrawPolylineStroke(screenScratchA_, true, strokeThickness, strokeColor, style.lineStyle);
        break;
    }
    case PrimitiveType::Diamond:
    {
        const auto* diamond = std::get_if<DiamondGeometry>(&primitive.geometry);
        if (diamond == nullptr)
        {
            break;
        }

        const std::array<Vec2, 4> logicalPoints {{
            {0.0f, diamond->height * 0.5f},
            {diamond->width * 0.5f, 0.0f},
            {0.0f, -diamond->height * 0.5f},
            {-diamond->width * 0.5f, 0.0f}}};
        BuildScreenPointsInto(logicalPoints.data(), logicalPoints.size(), primitive, group, screenScratchA_);

        if (style.filled)
        {
            FillConvexPolygon(screenScratchA_, fillColor);
        }

        DrawPolylineStroke(screenScratchA_, true, strokeThickness, strokeColor, style.lineStyle);
        break;
    }
    case PrimitiveType::Triangle:
    {
        const auto* triangle = std::get_if<TriangleGeometry>(&primitive.geometry);
        if (triangle == nullptr)
        {
            break;
        }

        BuildScreenPointsInto(triangle->points.data(), triangle->points.size(), primitive, group, screenScratchA_);

        if (style.filled)
        {
            FillConvexPolygon(screenScratchA_, fillColor);
        }

        DrawPolylineStroke(screenScratchA_, true, strokeThickness, strokeColor, style.lineStyle);
        break;
    }
    case PrimitiveType::Polyline:
    {
        const auto* polyline = std::get_if<PolylineGeometry>(&primitive.geometry);
        if (polyline == nullptr)
        {
            break;
        }

        BuildScreenPointsInto(polyline->points.data(), polyline->points.size(), primitive, group, screenScratchA_);

        if (style.filled && polyline->closed)
        {
            if (detail::PolygonIsConvex(screenScratchA_))
            {
                FillConvexPolygon(screenScratchA_, fillColor);
            }
            else
            {
                triangleIndexScratch_.clear();
                if (detail::TriangulateSimplePolygon(screenScratchA_, triangleIndexScratch_))
                {
                    FillIndexedTriangles(screenScratchA_, triangleIndexScratch_, fillColor);
                }
            }
        }

        DrawPolylineStroke(screenScratchA_, polyline->closed, strokeThickness, strokeColor, style.lineStyle);
        break;
    }
    case PrimitiveType::Bezier:
    {
        const auto* bezier = std::get_if<BezierGeometry>(&primitive.geometry);
        if (bezier == nullptr)
        {
            break;
        }

        const std::vector<Vec2>* sampledPoints = nullptr;
        if (bezierCache_ != nullptr)
        {
            sampledPoints = &bezierCache_->Resolve(*bezier);
        }
        else
        {
            BezierPolylineCache::BuildPolyline(*bezier, logicalScratchA_);
            sampledPoints = &logicalScratchA_;
        }

        if (sampledPoints == nullptr)
        {
            break;
        }

        BuildScreenPointsInto(
            sampledPoints->data(),
            sampledPoints->size(),
            primitive,
            group,
            screenScratchA_);
        DrawPolylineStroke(screenScratchA_, false, strokeThickness, strokeColor, style.lineStyle);
        break;
    }
    case PrimitiveType::Arc:
    {
        const auto* arc = std::get_if<ArcGeometry>(&primitive.geometry);
        if (arc == nullptr)
        {
            break;
        }

        SampleArcInto(*arc, logicalScratchA_);
        BuildScreenPointsInto(logicalScratchA_.data(), logicalScratchA_.size(), primitive, group, screenScratchA_);

        if (style.filled)
        {
            logicalScratchB_.clear();
            logicalScratchB_.reserve(logicalScratchA_.size() + 1U);
            logicalScratchB_.push_back(Vec2 {});
            logicalScratchB_.insert(logicalScratchB_.end(), logicalScratchA_.begin(), logicalScratchA_.end());
            BuildScreenPointsInto(logicalScratchB_.data(), logicalScratchB_.size(), primitive, group, screenScratchB_);
            FillConvexPolygon(screenScratchB_, fillColor);
            DrawPolylineStroke(screenScratchB_, true, strokeThickness, strokeColor, style.lineStyle);
        }
        else
        {
            DrawPolylineStroke(screenScratchA_, false, strokeThickness, strokeColor, style.lineStyle);
        }
        break;
    }
    case PrimitiveType::Image:
    {
        if (imageCache_ == nullptr)
        {
            break;
        }

        const auto* image = std::get_if<ImageGeometry>(&primitive.geometry);
        if (image == nullptr)
        {
            break;
        }

        const Texture2D* texture = imageCache_->Resolve(image->file);
        if (texture == nullptr || texture->id == 0)
        {
            break;
        }

        const Vector2 center = ToScreen(TransformPoint({}, primitive, group));
        const float width = std::max(1.0f, ToViewPixels(image->width * std::abs(combinedTransform.scale.x)));
        const float height = std::max(1.0f, ToViewPixels(image->height * std::abs(combinedTransform.scale.y)));
        if (!IsFiniteVector(center) || !std::isfinite(width) || !std::isfinite(height))
        {
            break;
        }

        const Rectangle source {
            0.0f,
            0.0f,
            static_cast<float>(texture->width) * (combinedTransform.scale.x < 0.0f ? -1.0f : 1.0f),
            static_cast<float>(texture->height) * (combinedTransform.scale.y < 0.0f ? -1.0f : 1.0f)};
        const Rectangle destination {center.x, center.y, width, height};
        const Vector2 origin {width * 0.5f, height * 0.5f};
        DrawTexturePro(*texture, source, destination, origin, -combinedTransform.rotationDegrees, WHITE);
        break;
    }
    }
}
} // namespace mfd
