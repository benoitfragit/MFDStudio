#include "mfd/apilevel1/widgets/WidgetOcclusion.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace mfd::api1
{
namespace
{
struct Bounds
{
    Vec2 minimum {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec2 maximum {-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
    void Add(const Vec2 point) noexcept
    { minimum.x = std::min(minimum.x, point.x); minimum.y = std::min(minimum.y, point.y);
      maximum.x = std::max(maximum.x, point.x); maximum.y = std::max(maximum.y, point.y); }
    void Rectangle(const float width, const float height) noexcept
    { Add({-width * 0.5f, -height * 0.5f}); Add({width * 0.5f, height * 0.5f}); }
};
Bounds PrimitiveBounds(const Primitive& primitive) noexcept
{
    Bounds bounds;
    if (const auto* geometry = std::get_if<TextGeometry>(&primitive.geometry))
    {
        const float width = std::min(4096.0f, static_cast<float>(geometry->text.size())) * (geometry->fontSize + geometry->letterSpacing);
        const float left = geometry->align == Align::Left ? 0 : geometry->align == Align::Right ? -width : -width * 0.5f;
        bounds.Add({left, -geometry->fontSize}); bounds.Add({left + width, geometry->fontSize});
    }
    else if (const auto* geometry = std::get_if<TimeGeometry>(&primitive.geometry)) bounds.Rectangle(geometry->fontSize * 32, geometry->fontSize * 2);
    else if (const auto* geometry = std::get_if<LineGeometry>(&primitive.geometry)) { bounds.Add(geometry->start); bounds.Add(geometry->end); }
    else if (const auto* geometry = std::get_if<CircleGeometry>(&primitive.geometry)) bounds.Rectangle(2 * geometry->radius, 2 * geometry->radius);
    else if (const auto* geometry = std::get_if<RingGeometry>(&primitive.geometry)) bounds.Rectangle(2 * geometry->outerRadius, 2 * geometry->outerRadius);
    else if (const auto* geometry = std::get_if<RectangleGeometry>(&primitive.geometry)) bounds.Rectangle(geometry->width, geometry->height);
    else if (const auto* geometry = std::get_if<EllipseGeometry>(&primitive.geometry)) bounds.Rectangle(geometry->width, geometry->height);
    else if (const auto* geometry = std::get_if<SquareGeometry>(&primitive.geometry)) bounds.Rectangle(geometry->width, geometry->height);
    else if (const auto* geometry = std::get_if<DiamondGeometry>(&primitive.geometry)) bounds.Rectangle(geometry->width, geometry->height);
    else if (const auto* geometry = std::get_if<TriangleGeometry>(&primitive.geometry)) for (const auto point : geometry->points) bounds.Add(point);
    else if (const auto* geometry = std::get_if<PolylineGeometry>(&primitive.geometry))
    { for (std::size_t i = 0; i < geometry->points.size() && i < 4096; ++i) bounds.Add(geometry->points[i]); }
    else if (const auto* geometry = std::get_if<BezierGeometry>(&primitive.geometry))
    { for (std::size_t i = 0; i < geometry->controlPoints.size() && i < 4096; ++i) bounds.Add(geometry->controlPoints[i]); }
    else if (const auto* geometry = std::get_if<ArcGeometry>(&primitive.geometry)) bounds.Rectangle(2 * geometry->radius, 2 * geometry->radius);
    else if (const auto* geometry = std::get_if<ImageGeometry>(&primitive.geometry)) bounds.Rectangle(geometry->width, geometry->height);
    else if (const auto* geometry = std::get_if<ExternalStreamGeometry>(&primitive.geometry)) bounds.Rectangle(geometry->width, geometry->height);
    return bounds;
}
}
bool ReticleOccludesWidgetPointer(const ReticleGroup& reticle, const Vec2 pointer) noexcept
{
    if (!reticle.visible || !std::isfinite(pointer.x) || !std::isfinite(pointer.y)) return false;
    for (std::size_t index = 0; index < reticle.primitives.size() && index < 4096; ++index)
    {
        const auto& primitive = reticle.primitives[index];
        const auto color = reticle.overrides.color.value_or(primitive.style.color);
        const auto fill = reticle.overrides.fillColor.value_or(primitive.style.fillColor);
        if (!primitive.style.visible || (color.a == 0 && (!reticle.overrides.filled.value_or(primitive.style.filled) || fill.a == 0))) continue;
        const auto transform = ResolvePrimitiveWorldTransform(primitive, reticle);
        if (std::abs(transform.scale.x) < 1e-8f || std::abs(transform.scale.y) < 1e-8f) continue;
        const float angle = -transform.rotationDegrees * 0.017453292519943295f;
        const float x = pointer.x - transform.position.x, y = pointer.y - transform.position.y;
        const Vec2 local {(x * std::cos(angle) - y * std::sin(angle)) / transform.scale.x,
                          (x * std::sin(angle) + y * std::cos(angle)) / transform.scale.y};
        const auto bounds = PrimitiveBounds(primitive);
        const float stroke = reticle.overrides.thickness.value_or(primitive.style.thickness);
        if (local.x >= bounds.minimum.x - stroke && local.x <= bounds.maximum.x + stroke &&
            local.y >= bounds.minimum.y - stroke && local.y <= bounds.maximum.y + stroke) return true;
    }
    return false;
}
}
