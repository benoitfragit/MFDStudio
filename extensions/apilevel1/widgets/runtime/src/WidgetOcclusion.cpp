#include "mfd/apilevel1/widgets/WidgetOcclusion.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace mfd::api1
{
namespace
{
bool ToLocalPointer(const Primitive& primitive, const ReticleGroup& reticle, const Vec2 pointer, Vec2& local) noexcept
{
    const auto transform = ResolvePrimitiveWorldTransform(primitive, reticle);
    if (std::abs(transform.scale.x) < 1e-8f || std::abs(transform.scale.y) < 1e-8f) return false;
    const float angle = -transform.rotationDegrees * 0.017453292519943295f;
    const float x = pointer.x - transform.position.x, y = pointer.y - transform.position.y;
    local = {(x * std::cos(angle) - y * std::sin(angle)) / transform.scale.x,
             (x * std::sin(angle) + y * std::cos(angle)) / transform.scale.y};
    return std::isfinite(local.x) && std::isfinite(local.y);
}
float TriangleSide(const Vec2 a, const Vec2 b, const Vec2 point) noexcept
{ return (b.x-a.x)*(point.y-a.y) - (b.y-a.y)*(point.x-a.x); }
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
bool ReticleOccludesWidgetPointer(const ReticleGroup& reticle, const Vec2 pointer,
    const WidgetImageOcclusionSampler imageSampler, void* const imageContext) noexcept
{
    if (!reticle.visible || !std::isfinite(pointer.x) || !std::isfinite(pointer.y)) return false;
    for (std::size_t index = 0; index < reticle.primitives.size() && index < 4096; ++index)
    {
        const auto& primitive = reticle.primitives[index];
        const auto color = reticle.overrides.color.value_or(primitive.style.color);
        const auto fill = reticle.overrides.fillColor.value_or(primitive.style.fillColor);
        if (!primitive.style.visible || (color.a == 0 && (!reticle.overrides.filled.value_or(primitive.style.filled) || fill.a == 0))) continue;
        Vec2 local {};
        if (!ToLocalPointer(primitive, reticle, pointer, local)) continue;
        const auto bounds = PrimitiveBounds(primitive);
        const float stroke = std::abs(reticle.overrides.thickness.value_or(primitive.style.thickness)) * 0.5f;
        const bool paintedFill = reticle.overrides.filled.value_or(primitive.style.filled) && fill.a != 0U;
        const bool paintedStroke = color.a != 0U && stroke > 0;
        if (const auto* geometry = std::get_if<ImageGeometry>(&primitive.geometry))
        {
            if (color.a == 0 || geometry->width <= 0 || geometry->height <= 0) continue;
            const Vec2 uv {local.x / geometry->width + 0.5f, 0.5f - local.y / geometry->height};
            if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1) continue;
            if (!imageSampler || imageSampler(imageContext, *geometry, uv)) return true;
            continue;
        }
        if (const auto* geometry = std::get_if<CircleGeometry>(&primitive.geometry))
        {
            const float radius = std::hypot(local.x, local.y);
            if ((paintedFill && radius <= geometry->radius) ||
                (paintedStroke && std::abs(radius - geometry->radius) <= stroke)) return true;
            continue;
        }
        if (const auto* geometry = std::get_if<RingGeometry>(&primitive.geometry))
        {
            const float radius = std::hypot(local.x, local.y);
            if ((paintedFill && radius >= geometry->innerRadius && radius <= geometry->outerRadius) ||
                (paintedStroke && (std::abs(radius - geometry->innerRadius) <= stroke ||
                                   std::abs(radius - geometry->outerRadius) <= stroke))) return true;
            continue;
        }
        if (std::holds_alternative<RectangleGeometry>(primitive.geometry) || std::holds_alternative<SquareGeometry>(primitive.geometry))
        {
            const float x = std::abs(local.x), y = std::abs(local.y);
            if (paintedFill && x <= bounds.maximum.x && y <= bounds.maximum.y) return true;
            if (paintedStroke && x <= bounds.maximum.x + stroke && y <= bounds.maximum.y + stroke &&
                (x >= bounds.maximum.x - stroke || y >= bounds.maximum.y - stroke)) return true;
            continue;
        }
        if (const auto* geometry = std::get_if<LineGeometry>(&primitive.geometry))
        {
            const double dx = geometry->end.x - geometry->start.x, dy = geometry->end.y - geometry->start.y;
            const double length = dx * dx + dy * dy;
            const double t = length > 0 ? std::clamp(((local.x - geometry->start.x) * dx +
                (local.y - geometry->start.y) * dy) / length, 0.0, 1.0) : 0;
            if (paintedStroke && std::hypot(local.x - geometry->start.x - t * dx,
                local.y - geometry->start.y - t * dy) <= stroke) return true;
            continue;
        }
        if (local.x >= bounds.minimum.x - stroke && local.x <= bounds.maximum.x + stroke &&
            local.y >= bounds.minimum.y - stroke && local.y <= bounds.maximum.y + stroke) return true;
    }
    return false;
}
bool ReticleErasesWidgetOcclusion(const ReticleGroup& reticle, const Vec2 pointer) noexcept
{
    if (!reticle.visible) return false;
    const auto* primitive = ResolveClipPrimitive(reticle);
    if (!primitive) return false;
    Vec2 local {};
    if (!ToLocalPointer(*primitive, reticle, pointer, local)) return false;
    bool inside = false;
    if (const auto* shape = std::get_if<CircleGeometry>(&primitive->geometry))
        inside = std::hypot(local.x,local.y) <= shape->radius;
    else if (const auto* shape = std::get_if<EllipseGeometry>(&primitive->geometry))
    {
        if (shape->width <= 0 || shape->height <= 0) return false;
        inside = std::hypot(local.x * 2 / shape->width, local.y * 2 / shape->height) <= 1;
    }
    else if (const auto* shape = std::get_if<TriangleGeometry>(&primitive->geometry))
    {
        const float a = TriangleSide(shape->points[0], shape->points[1], local);
        const float b = TriangleSide(shape->points[1], shape->points[2], local);
        const float c = TriangleSide(shape->points[2], shape->points[0], local);
        inside = (a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0);
    }
    else if (std::holds_alternative<RectangleGeometry>(primitive->geometry) || std::holds_alternative<SquareGeometry>(primitive->geometry))
    {
        const auto bounds = PrimitiveBounds(*primitive);
        inside = std::abs(local.x) <= bounds.maximum.x && std::abs(local.y) <= bounds.maximum.y;
    }
    else return false;
    return reticle.clipping.mode == ReticleClipMode::Inner ? inside : !inside;
}
}
