/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "mfd/render/Canvas2D.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <string>
#include <utility>

namespace mfd
{
namespace
{
Color ToRayColor(const ColorRgba& color)
{
    return Color {color.r, color.g, color.b, color.a};
}

void FillConvexPolygon(const std::vector<Vector2>& points, const Color color)
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

void DrawPolylineStroke(const std::vector<Vector2>& points,
                        const bool closed,
                        const float thickness,
                        const Color color)
{
    if (points.size() < 2)
    {
        return;
    }

    for (std::size_t index = 0; index + 1 < points.size(); ++index)
    {
        DrawLineEx(points[index], points[index + 1], thickness, color);
    }

    if (closed)
    {
        DrawLineEx(points.back(), points.front(), thickness, color);
    }
}

void FillRing(const std::vector<Vector2>& outerPoints,
              const std::vector<Vector2>& innerPoints,
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

Vec2 Lerp(const Vec2& lhs, const Vec2& rhs, const float factor)
{
    return {
        lhs.x + (rhs.x - lhs.x) * factor,
        lhs.y + (rhs.y - lhs.y) * factor};
}

Vec2 EvaluateBezier(std::vector<Vec2> controlPoints, const float factor)
{
    while (controlPoints.size() > 1)
    {
        std::vector<Vec2> nextPoints;
        nextPoints.reserve(controlPoints.size() - 1);

        for (std::size_t index = 0; index + 1 < controlPoints.size(); ++index)
        {
            nextPoints.push_back(Lerp(controlPoints[index], controlPoints[index + 1], factor));
        }

        controlPoints = std::move(nextPoints);
    }

    return controlPoints.front();
}

std::vector<Vec2> SampleBezier(const BezierGeometry& geometry)
{
    const int segmentCount = std::max(2, geometry.segments);
    std::vector<Vec2> points;
    points.reserve(static_cast<std::size_t>(segmentCount) + 1);

    for (int index = 0; index <= segmentCount; ++index)
    {
        const float factor = static_cast<float>(index) / static_cast<float>(segmentCount);
        points.push_back(EvaluateBezier(geometry.controlPoints, factor));
    }

    return points;
}

std::vector<Vec2> SampleEllipse(const EllipseGeometry& geometry, const int segments = 64)
{
    const int segmentCount = std::max(12, segments);
    const float halfWidth = geometry.width * 0.5f;
    const float halfHeight = geometry.height * 0.5f;

    std::vector<Vec2> points;
    points.reserve(static_cast<std::size_t>(segmentCount));

    for (int index = 0; index < segmentCount; ++index)
    {
        const float angle = static_cast<float>(index) / static_cast<float>(segmentCount) * 2.0f * PI;
        points.push_back(Vec2 {
            std::cos(angle) * halfWidth,
            std::sin(angle) * halfHeight});
    }

    return points;
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

Canvas2D::Canvas2D(const int width, const int height, const PageViewState view, const Font* textFont)
    : width_(width)
    , height_(height)
    , view_(view)
    , textFont_(textFont)
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

void Canvas2D::DrawReticle(const ReticleGroup& reticle) const
{
    if (!reticle.visible)
    {
        return;
    }

    for (const auto& primitive : reticle.primitives)
    {
        if (!primitive.style.visible)
        {
            continue;
        }

        DrawPrimitive(primitive, reticle);
    }
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

std::vector<Vector2> Canvas2D::BuildScreenPoints(const std::vector<Vec2>& points,
                                                 const Primitive& primitive,
                                                 const ReticleGroup& group) const
{
    std::vector<Vector2> screenPoints;
    screenPoints.reserve(points.size());

    for (const auto& point : points)
    {
        screenPoints.push_back(ToScreen(TransformPoint(point, primitive, group)));
    }

    return screenPoints;
}

void Canvas2D::DrawPrimitive(const Primitive& primitive, const ReticleGroup& group) const
{
    const PrimitiveStyle style = MergeStyle(primitive.style, group.overrides);
    const Color strokeColor = ToRayColor(style.color);
    const Color fillColor = ToRayColor(style.fillColor);
    const float strokeThickness = std::max(1.0f, std::abs(ToPixels(style.thickness)));
    const Transform2D combinedTransform = CombineTransforms(group.transform, primitive.transform);

    switch (primitive.type)
    {
    case PrimitiveType::Text:
    {
        const auto& text = std::get<TextGeometry>(primitive.geometry);
        const Font font = TextFont();
        const float textScale = AverageScale(group.transform, primitive.transform);
        const float fontSize = std::max(1.0f,
                                        std::abs(ToPixels(text.fontSize * textScale)));
        const float letterSpacing = std::isfinite(text.letterSpacing)
                                        ? ToPixels(text.letterSpacing * textScale)
                                        : ToPixels(kDefaultTextLetterSpacing * textScale);
        const Vector2 screenPosition = ToScreen(TransformPoint({}, primitive, group));
        const Vector2 textSize = MeasureTextEx(font, text.text.c_str(), fontSize, letterSpacing);
        const Vector2 origin {textSize.x * 0.5f, textSize.y * 0.5f};

        DrawCenteredText(text.text,
                         font,
                         fontSize,
                         letterSpacing,
                         screenPosition,
                         origin,
                         -combinedTransform.rotationDegrees,
                         strokeColor);
        break;
    }
    case PrimitiveType::Time:
    {
        const auto& time = std::get<TimeGeometry>(primitive.geometry);
        const std::string text = FormatTimeText(time);
        const Font font = TextFont();
        const float textScale = AverageScale(group.transform, primitive.transform);
        const float fontSize = std::max(1.0f,
                                        std::abs(ToPixels(time.fontSize * textScale)));
        const float letterSpacing = std::isfinite(time.letterSpacing)
                                        ? ToPixels(time.letterSpacing * textScale)
                                        : ToPixels(kDefaultTextLetterSpacing * textScale);
        const Vector2 screenPosition = ToScreen(TransformPoint({}, primitive, group));
        const Vector2 textSize = MeasureTextEx(font, text.c_str(), fontSize, letterSpacing);
        const Vector2 origin {textSize.x * 0.5f, textSize.y * 0.5f};

        DrawCenteredText(text,
                         font,
                         fontSize,
                         letterSpacing,
                         screenPosition,
                         origin,
                         -combinedTransform.rotationDegrees,
                         strokeColor);
        break;
    }
    case PrimitiveType::Line:
    {
        const auto& line = std::get<LineGeometry>(primitive.geometry);
        const Vector2 start = ToScreen(TransformPoint(line.start, primitive, group));
        const Vector2 end = ToScreen(TransformPoint(line.end, primitive, group));
        DrawLineEx(start, end, strokeThickness, strokeColor);
        break;
    }
    case PrimitiveType::Circle:
    {
        const auto& circle = std::get<CircleGeometry>(primitive.geometry);
        const float radius = std::max(0.0f,
                                      std::abs(ToPixels(circle.radius * AverageScale(group.transform,
                                                                                     primitive.transform))));
        const Vector2 center = ToScreen(TransformPoint({}, primitive, group));

        if (style.filled)
        {
            DrawCircleV(center, radius, fillColor);
        }

        DrawRing(center,
                 std::max(0.0f, radius - strokeThickness * 0.5f),
                 radius + strokeThickness * 0.5f,
                 0.0f,
                 360.0f,
                 64,
                 strokeColor);
        break;
    }
    case PrimitiveType::Ring:
    {
        const auto& ring = std::get<RingGeometry>(primitive.geometry);
        const std::vector<Vec2> outerLogicalPoints = SampleEllipse(
            EllipseGeometry {ring.outerRadius * 2.0f, ring.outerRadius * 2.0f},
            ring.segments);
        const std::vector<Vec2> innerLogicalPoints = SampleEllipse(
            EllipseGeometry {ring.innerRadius * 2.0f, ring.innerRadius * 2.0f},
            ring.segments);
        const auto outerScreenPoints = BuildScreenPoints(outerLogicalPoints, primitive, group);
        const auto innerScreenPoints = BuildScreenPoints(innerLogicalPoints, primitive, group);

        if (style.filled)
        {
            FillRing(outerScreenPoints, innerScreenPoints, fillColor);
        }

        DrawPolylineStroke(outerScreenPoints, true, strokeThickness, strokeColor);
        DrawPolylineStroke(innerScreenPoints, true, strokeThickness, strokeColor);
        break;
    }
    case PrimitiveType::Rectangle:
    {
        const auto& rectangle = std::get<RectangleGeometry>(primitive.geometry);
        const std::vector<Vec2> logicalPoints {
            {-rectangle.width * 0.5f, -rectangle.height * 0.5f},
            {rectangle.width * 0.5f, -rectangle.height * 0.5f},
            {rectangle.width * 0.5f, rectangle.height * 0.5f},
            {-rectangle.width * 0.5f, rectangle.height * 0.5f}};
        const auto screenPoints = BuildScreenPoints(logicalPoints, primitive, group);

        if (style.filled)
        {
            FillConvexPolygon(screenPoints, fillColor);
        }

        DrawPolylineStroke(screenPoints, true, strokeThickness, strokeColor);
        break;
    }
    case PrimitiveType::Ellipse:
    {
        const auto& ellipse = std::get<EllipseGeometry>(primitive.geometry);
        const auto logicalPoints = SampleEllipse(ellipse);
        const auto screenPoints = BuildScreenPoints(logicalPoints, primitive, group);

        if (style.filled)
        {
            FillConvexPolygon(screenPoints, fillColor);
        }

        DrawPolylineStroke(screenPoints, true, strokeThickness, strokeColor);
        break;
    }
    case PrimitiveType::Square:
    {
        const auto& square = std::get<SquareGeometry>(primitive.geometry);
        const std::vector<Vec2> logicalPoints {
            {-square.width * 0.5f, -square.height * 0.5f},
            {square.width * 0.5f, -square.height * 0.5f},
            {square.width * 0.5f, square.height * 0.5f},
            {-square.width * 0.5f, square.height * 0.5f}};
        const auto screenPoints = BuildScreenPoints(logicalPoints, primitive, group);

        if (style.filled)
        {
            FillConvexPolygon(screenPoints, fillColor);
        }

        DrawPolylineStroke(screenPoints, true, strokeThickness, strokeColor);
        break;
    }
    case PrimitiveType::Diamond:
    {
        const auto& diamond = std::get<DiamondGeometry>(primitive.geometry);
        const std::vector<Vec2> logicalPoints {
            {0.0f, diamond.height * 0.5f},
            {diamond.width * 0.5f, 0.0f},
            {0.0f, -diamond.height * 0.5f},
            {-diamond.width * 0.5f, 0.0f}};
        const auto screenPoints = BuildScreenPoints(logicalPoints, primitive, group);

        if (style.filled)
        {
            FillConvexPolygon(screenPoints, fillColor);
        }

        DrawPolylineStroke(screenPoints, true, strokeThickness, strokeColor);
        break;
    }
    case PrimitiveType::Triangle:
    {
        const auto& triangle = std::get<TriangleGeometry>(primitive.geometry);
        const std::vector<Vec2> logicalPoints {
            triangle.points[0],
            triangle.points[1],
            triangle.points[2]};
        const auto screenPoints = BuildScreenPoints(logicalPoints, primitive, group);

        if (style.filled)
        {
            FillConvexPolygon(screenPoints, fillColor);
        }

        DrawPolylineStroke(screenPoints, true, strokeThickness, strokeColor);
        break;
    }
    case PrimitiveType::Polyline:
    {
        const auto& polyline = std::get<PolylineGeometry>(primitive.geometry);
        const auto screenPoints = BuildScreenPoints(polyline.points, primitive, group);

        if (style.filled && polyline.closed)
        {
            FillConvexPolygon(screenPoints, fillColor);
        }

        DrawPolylineStroke(screenPoints, polyline.closed, strokeThickness, strokeColor);
        break;
    }
    case PrimitiveType::Bezier:
    {
        const auto& bezier = std::get<BezierGeometry>(primitive.geometry);
        const auto sampledPoints = SampleBezier(bezier);
        const auto screenPoints = BuildScreenPoints(sampledPoints, primitive, group);
        DrawPolylineStroke(screenPoints, false, strokeThickness, strokeColor);
        break;
    }
    }
}
} // namespace mfd
