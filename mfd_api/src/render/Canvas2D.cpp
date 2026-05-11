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

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include "mfd/core/ArrayView.h"
#include <ctime>
#include <string>
#include <utility>

#include <rlgl.h>

#include "BezierPolylineCache.h"
#include "ImageTextureCache.h"
#include "OpenGlCompat.h"
#include "PolygonTriangulation.h"
#include "TextLayoutCache.h"

namespace mfd
{
namespace
{
constexpr float kMinSegmentLength = 0.0001f;
constexpr int kMaxPrimitiveSegments = 1024;
constexpr std::size_t kMaxStrokeFragmentsPerSegment = 4096U;

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

bool IsFiniteVec2(const Vec2& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

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
                             const Color color)
{
    float segmentLength = 0.0f;
    if (!TryMeasureFiniteSegment(start, end, segmentLength))
    {
        return;
    }

    float dashStartOffset = 0.0f;
    std::size_t fragmentCount = 0U;
    while (dashStartOffset < segmentLength)
    {
        if (++fragmentCount > kMaxStrokeFragmentsPerSegment)
        {
            break;
        }

        const float dashEndOffset = std::min(dashStartOffset + dashLength, segmentLength);
        const Vector2 dashStart = LerpVector2(start, end, dashStartOffset / segmentLength);
        const Vector2 dashEnd = LerpVector2(start, end, dashEndOffset / segmentLength);
        if (!IsFiniteVector(dashStart) || !IsFiniteVector(dashEnd))
        {
            break;
        }

        DrawLineEx(dashStart, dashEnd, thickness, color);
        dashStartOffset = dashEndOffset + gapLength;
    }
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

    for (std::size_t index = 0; index + 1 < points.size(); ++index)
    {
        DrawDashedStrokeSegment(points[index], points[index + 1], dashLength, gapLength, thickness, color);
    }

    if (closed)
    {
        DrawDashedStrokeSegment(points.back(), points.front(), dashLength, gapLength, thickness, color);
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

std::tm ToCalendarTime(const std::time_t rawTime, const bool utc) noexcept
{
    std::tm calendarTime {};

#if defined(_WIN32)
    if (utc)
    {
        gmtime_s(&calendarTime, &rawTime);
    }
    else
    {
        localtime_s(&calendarTime, &rawTime);
    }
#else
    if (utc)
    {
        gmtime_r(&rawTime, &calendarTime);
    }
    else
    {
        localtime_r(&rawTime, &calendarTime);
    }
#endif

    return calendarTime;
}

std::string FormatTimeText(const TimeGeometry& geometry)
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t rawTime = std::chrono::system_clock::to_time_t(now);
    const std::tm calendarTime = ToCalendarTime(rawTime, geometry.utc);

    char buffer[128] {};
    if (std::strftime(buffer, sizeof(buffer), geometry.format.c_str(), &calendarTime) == 0U)
    {
        return geometry.utc ? "UTC" : "--:--:--";
    }

    return std::string(buffer);
}

void DrawCenteredText(const std::string& text,
                      const Font font,
                      const float fontSize,
                      const float letterSpacing,
                      const Vector2 screenPosition,
                      const Vector2 origin,
                      const float rotationDegrees,
                      const Color color)
{
    DrawTextPro(font,
                text.c_str(),
                screenPosition,
                origin,
                rotationDegrees,
                fontSize,
                letterSpacing,
                color);
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
                   TextLayoutCache* textLayoutCache)
    : width_(width)
    , height_(height)
    , view_(view)
    , textFont_(textFont)
    , backgroundColor_(backgroundColor)
    , clippingEnabled_(clippingEnabled)
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
            if (BeginClipMask(*clipPrimitive, reticle))
            {
                DrawReticlePrimitives(reticle);
                EndClipMask();
                return;
            }
        }
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

bool Canvas2D::BeginClipMask(const Primitive& primitive, const ReticleGroup& group) const
{
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
        return false;
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
                                      1,
                                      0xFF);
    return true;
}

void Canvas2D::EndClipMask() const noexcept
{
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
                                      std::abs(ToViewPixels(circle->radius * AverageScale(group.transform,
                                                                                          primitive.transform))));
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
    return ApplyTransform(ApplyTransform(point, primitive.transform), group.transform);
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

void Canvas2D::DrawPrimitive(const Primitive& primitive, const ReticleGroup& group) const
{
    const PrimitiveStyle style = MergeStyle(primitive.style, group.overrides);
    const Color strokeColor = ToRayColor(style.color);
    const Color fillColor = ToRayColor(style.fillColor);
    const float strokeThickness = std::max(1.0f, std::abs(ToViewPixels(style.thickness)));
    const Transform2D combinedTransform = CombineTransforms(group.transform, primitive.transform);
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
        const float textScale = AverageScale(group.transform, primitive.transform);
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

        CachedTextLayout fallbackLayout;
        const CachedTextLayout* layout = nullptr;
        if (textLayoutCache_ != nullptr)
        {
            layout = &textLayoutCache_->ResolveStaticText(text->text, font, fontSize, letterSpacing);
        }
        else
        {
            fallbackLayout.text = text->text;
            fallbackLayout.size = MeasureTextEx(font, fallbackLayout.text.c_str(), fontSize, letterSpacing);
            fallbackLayout.origin = Vector2 {fallbackLayout.size.x * 0.5f, fallbackLayout.size.y * 0.5f};
            layout = &fallbackLayout;
        }

        if (layout == nullptr || !IsFiniteVector(layout->size) || !IsFiniteVector(layout->origin))
        {
            break;
        }

        DrawCenteredText(layout->text,
                         font,
                         fontSize,
                         letterSpacing,
                         screenPosition,
                         layout->origin,
                         -combinedTransform.rotationDegrees,
                         strokeColor);
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
        const float textScale = AverageScale(group.transform, primitive.transform);
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

        CachedTextLayout fallbackLayout;
        const CachedTextLayout* layout = nullptr;
        if (textLayoutCache_ != nullptr)
        {
            layout = &textLayoutCache_->ResolveTimeText(*time, font, fontSize, letterSpacing);
        }
        else
        {
            fallbackLayout.text = FormatTimeText(*time);
            fallbackLayout.size = MeasureTextEx(font, fallbackLayout.text.c_str(), fontSize, letterSpacing);
            fallbackLayout.origin = Vector2 {fallbackLayout.size.x * 0.5f, fallbackLayout.size.y * 0.5f};
            layout = &fallbackLayout;
        }

        if (layout == nullptr || !IsFiniteVector(layout->size) || !IsFiniteVector(layout->origin))
        {
            break;
        }

        DrawCenteredText(layout->text,
                         font,
                         fontSize,
                         letterSpacing,
                         screenPosition,
                         layout->origin,
                         -combinedTransform.rotationDegrees,
                         strokeColor);
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
                                      std::abs(ToViewPixels(circle->radius * AverageScale(group.transform,
                                                                                          primitive.transform))));
        const Vector2 center = ToScreen(TransformPoint({}, primitive, group));
        if (!std::isfinite(radius) || !IsFiniteVector(center))
        {
            break;
        }

        if (style.filled)
        {
            DrawCircleV(center, radius, fillColor);
        }

        SampleEllipseInto(EllipseGeometry {circle->radius * 2.0f, circle->radius * 2.0f}, 64, logicalScratchA_);
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

        SampleEllipseInto(
            EllipseGeometry {ring->outerRadius * 2.0f, ring->outerRadius * 2.0f},
            ring->segments,
            logicalScratchA_);
        SampleEllipseInto(
            EllipseGeometry {ring->innerRadius * 2.0f, ring->innerRadius * 2.0f},
            ring->segments,
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
