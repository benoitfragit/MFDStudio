/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for SceneRegistry.
 */

#include "mfd/runtime/SceneRegistry.h"

#include "mfd/control/CommandTypes.h"
#include "mfd/model/RuntimeBudgets.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>

namespace mfd
{
SceneRegistry::SceneRegistry() = default;
SceneRegistry::~SceneRegistry() = default;
SceneRegistry::SceneRegistry(SceneRegistry&&) noexcept = default;
SceneRegistry& SceneRegistry::operator=(SceneRegistry&&) noexcept = default;

namespace
{
using BlinkClock = std::chrono::steady_clock;
using runtime_validation::AreValidPoints;
using runtime_validation::IsFiniteAbsWithin;
using runtime_validation::IsPositiveFiniteWithin;
using runtime_validation::IsValidScale;
using runtime_validation::IsValidTextPayload;
using runtime_validation::IsValidTimeValue;
using runtime_validation::IsValidVec2;
using runtime_validation::HasVisibleTimeField;
using runtime_validation::kMaxAbsAngleDegrees;
using runtime_validation::kMaxBezierControlPoints;
using runtime_validation::kMaxFilledPolygonPoints;
using runtime_validation::kMaxLogicalSize;
using runtime_validation::kMaxPrimitiveSegments;
using runtime_validation::kMaxThickness;
using runtime_validation::kMaxZoom;

constexpr std::size_t kMaxPatchEntryCount = 2048U;

enum class ReticleDrawPass : std::uint8_t
{
    Normal = 0,
    DrawOnTop = 1,
    Strobe = 2
};

enum class ReticleDrawBand : std::uint8_t
{
    Static = 0,
    Dynamic = 1,
    Strobe = 2
};

struct ReticleDrawOrderKey
{
    ReticleDrawPass pass = ReticleDrawPass::Normal;
    std::size_t layerOrder = 0;
    ReticleDrawBand band = ReticleDrawBand::Static;
    std::size_t staticAuthoredOrder = 0;
    int dynamicTemplateOrder = 0;
    std::uint64_t dynamicCreationOrder = 0;
};

bool operator<(const ReticleDrawOrderKey& lhs, const ReticleDrawOrderKey& rhs) noexcept
{
    if (lhs.pass != rhs.pass)
    {
        return lhs.pass < rhs.pass;
    }

    if (lhs.pass != ReticleDrawPass::Strobe && lhs.layerOrder != rhs.layerOrder)
    {
        return lhs.layerOrder < rhs.layerOrder;
    }

    if (lhs.band != rhs.band)
    {
        return lhs.band < rhs.band;
    }

    if (lhs.band == ReticleDrawBand::Static && lhs.staticAuthoredOrder != rhs.staticAuthoredOrder)
    {
        return lhs.staticAuthoredOrder < rhs.staticAuthoredOrder;
    }

    if (lhs.band == ReticleDrawBand::Dynamic)
    {
        if (lhs.dynamicTemplateOrder != rhs.dynamicTemplateOrder)
        {
            return lhs.dynamicTemplateOrder < rhs.dynamicTemplateOrder;
        }

        if (lhs.dynamicCreationOrder != rhs.dynamicCreationOrder)
        {
            return lhs.dynamicCreationOrder < rhs.dynamicCreationOrder;
        }
    }

    return false;
}

std::string NormalizeReticleId(const std::string_view value)
{
    return NormalizePageName(value);
}

bool IsValidPrimitivePatch(const PrimitivePatch& patch) noexcept
{
    if (patch.position.has_value() && !IsValidVec2(*patch.position))
    {
        return false;
    }

    if (patch.rotationDegrees.has_value() &&
        !IsFiniteAbsWithin(*patch.rotationDegrees, kMaxAbsAngleDegrees))
    {
        return false;
    }

    if (patch.scale.has_value() && !IsValidScale(*patch.scale))
    {
        return false;
    }

    if (patch.thickness.has_value() &&
        !IsPositiveFiniteWithin(*patch.thickness, kMaxThickness))
    {
        return false;
    }

    if (patch.text.has_value() && !IsValidTextPayload(*patch.text))
    {
        return false;
    }

    if (patch.letterSpacing.has_value() &&
        !IsFiniteAbsWithin(*patch.letterSpacing, kMaxLogicalSize))
    {
        return false;
    }

    if (patch.lineStart.has_value() && !IsValidVec2(*patch.lineStart))
    {
        return false;
    }

    if (patch.lineEnd.has_value() && !IsValidVec2(*patch.lineEnd))
    {
        return false;
    }

    if ((patch.radius.has_value() && !IsFiniteAbsWithin(*patch.radius, kMaxLogicalSize)) ||
        (patch.innerRadius.has_value() && !IsFiniteAbsWithin(*patch.innerRadius, kMaxLogicalSize)) ||
        (patch.outerRadius.has_value() && !IsFiniteAbsWithin(*patch.outerRadius, kMaxLogicalSize)) ||
        (patch.width.has_value() && !IsFiniteAbsWithin(*patch.width, kMaxLogicalSize)) ||
        (patch.height.has_value() && !IsFiniteAbsWithin(*patch.height, kMaxLogicalSize)) ||
        (patch.startAngleDegrees.has_value() &&
         !IsFiniteAbsWithin(*patch.startAngleDegrees, kMaxAbsAngleDegrees)) ||
        (patch.endAngleDegrees.has_value() &&
         !IsFiniteAbsWithin(*patch.endAngleDegrees, kMaxAbsAngleDegrees)))
    {
        return false;
    }

    if (patch.size.has_value() && !IsValidVec2(*patch.size, kMaxLogicalSize))
    {
        return false;
    }

    if (patch.points.has_value())
    {
        if (!AreValidPoints(*patch.points, 2U))
        {
            return false;
        }

        if (patch.closed.value_or(false) && patch.points->size() > kMaxFilledPolygonPoints)
        {
            return false;
        }
    }

    if (patch.segments.has_value() &&
        (*patch.segments < 2 || *patch.segments > kMaxPrimitiveSegments))
    {
        return false;
    }

    if (patch.clearTimeValue && patch.timeValue.has_value())
    {
        return false;
    }

    if (patch.timeValue.has_value() && !IsValidTimeValue(*patch.timeValue))
    {
        return false;
    }

    if (patch.timeFields.has_value() && !HasVisibleTimeField(*patch.timeFields))
    {
        return false;
    }

    return true;
}

bool IsValidReticlePatch(const ReticlePatch& patch) noexcept
{
    if (patch.position.has_value() && !IsValidVec2(*patch.position))
    {
        return false;
    }

    if (patch.rotationDegrees.has_value() &&
        !IsFiniteAbsWithin(*patch.rotationDegrees, kMaxAbsAngleDegrees))
    {
        return false;
    }

    if (patch.scale.has_value() && !IsValidScale(*patch.scale))
    {
        return false;
    }

    if (patch.thickness.has_value() &&
        !IsPositiveFiniteWithin(*patch.thickness, kMaxThickness))
    {
        return false;
    }

    if (patch.text.has_value() && !IsValidTextPayload(*patch.text))
    {
        return false;
    }

    if (patch.letterSpacing.has_value() &&
        !IsFiniteAbsWithin(*patch.letterSpacing, kMaxLogicalSize))
    {
        return false;
    }

    if (patch.texts.size() > kMaxPatchEntryCount ||
        patch.textsById.size() > kMaxPatchEntryCount ||
        patch.letterSpacings.size() > kMaxPatchEntryCount ||
        patch.letterSpacingsById.size() > kMaxPatchEntryCount ||
        patch.primitivePatches.size() > kMaxPatchEntryCount ||
        patch.primitivePatchesById.size() > kMaxPatchEntryCount)
    {
        return false;
    }

    for (const auto& textEntry : patch.texts)
    {
        if (!IsValidTextPayload(textEntry.second))
        {
            return false;
        }
    }

    for (const auto& textEntry : patch.textsById)
    {
        if (!IsValidTextPayload(textEntry.second))
        {
            return false;
        }
    }

    for (const auto& spacingEntry : patch.letterSpacings)
    {
        if (!IsFiniteAbsWithin(spacingEntry.second, kMaxLogicalSize))
        {
            return false;
        }
    }

    for (const auto& spacingEntry : patch.letterSpacingsById)
    {
        if (!IsFiniteAbsWithin(spacingEntry.second, kMaxLogicalSize))
        {
            return false;
        }
    }

    for (const auto& primitivePatchEntry : patch.primitivePatches)
    {
        if (!IsValidPrimitivePatch(primitivePatchEntry.second))
        {
            return false;
        }
    }

    for (const auto& primitivePatchEntry : patch.primitivePatchesById)
    {
        if (!IsValidPrimitivePatch(primitivePatchEntry.second))
        {
            return false;
        }
    }

    return true;
}

bool IsValidPageView(const PageViewState& view) noexcept
{
    return IsValidVec2(view.center) &&
           IsPositiveFiniteWithin(view.zoom, kMaxZoom);
}

float DistanceSquared(const Vec2& lhs, const Vec2& rhs) noexcept
{
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    return dx * dx + dy * dy;
}

bool IsInsideCaptureArea(const Vec2& strobePosition,
                         const Vec2& candidatePosition,
                         const StrobeCaptureConfig& capture) noexcept
{
    const Vec2 offset = candidatePosition - strobePosition;

    switch (capture.shape)
    {
    case StrobeCaptureShape::Circle:
        return DistanceSquared(strobePosition, candidatePosition) <= capture.radius * capture.radius;

    case StrobeCaptureShape::Rectangle:
        return std::abs(offset.x) <= capture.size.x * 0.5f &&
               std::abs(offset.y) <= capture.size.y * 0.5f;
    }

    return false;
}

// Returns true when the capture area can still match at least one candidate, matching the exact
// per-candidate semantics of IsInsideCaptureArea so the early-out never changes behavior.
//
// A circle compares distanceSquared <= radius*radius, which still holds at the exact center
// (0 <= radius*radius) for every radius, including 0 and negatives, so a circle capture is never
// degenerate. A rectangle compares against half extents and matches a centered candidate only
// when both extents are non-negative; strictly negative extents can never match and are skipped.
bool CaptureAreaCanMatch(const StrobeCaptureConfig& capture) noexcept
{
    switch (capture.shape)
    {
    case StrobeCaptureShape::Circle:
        return true;

    case StrobeCaptureShape::Rectangle:
        return capture.size.x >= 0.0f && capture.size.y >= 0.0f;
    }

    return false;
}

PrimitiveStyle ResolveStrobeMagnetVisualStyle(const ReticleGroup& reticle) noexcept
{
    if (reticle.primitives.empty())
    {
        return {};
    }

    return reticle.primitives.front().style;
}

std::vector<Primitive> MakeStrobeMagnetVisualPrimitives(const StrobeMagnetConfig& magnet,
                                                        const PrimitiveStyle& style)
{
    Primitive primitive;
    primitive.id = "magnet_visual_shape";
    primitive.style = style;

    const float size = std::max(0.001f, magnet.visualShapeSize);
    if (magnet.visualShape == StrobeMagnetVisualShape::Square)
    {
        primitive.type = PrimitiveType::Square;
        primitive.geometry = SquareGeometry {size, size};
    }
    else
    {
        primitive.type = PrimitiveType::Circle;
        primitive.geometry = CircleGeometry {size * 0.5f};
    }

    return {std::move(primitive)};
}

bool IsEmptyPatch(const ReticlePatch& patch) noexcept
{
    return !patch.visible.has_value() &&
           !patch.blinkEnabled.has_value() &&
           !patch.blinkType.has_value() &&
           !patch.blinkTypeId.has_value() &&
           !patch.position.has_value() &&
           !patch.rotationDegrees.has_value() &&
           !patch.scale.has_value() &&
           !patch.color.has_value() &&
           !patch.thickness.has_value() &&
           !patch.text.has_value() &&
           patch.texts.empty() &&
           patch.textsById.empty() &&
           !patch.letterSpacing.has_value() &&
           patch.letterSpacings.empty() &&
           patch.letterSpacingsById.empty() &&
           patch.primitivePatches.empty() &&
           patch.primitivePatchesById.empty();
}

template <typename Geometry>
bool ApplyBoxPrimitivePatch(Geometry& geometry, const PrimitivePatch& patch)
{
    bool applied = false;

    if (patch.width.has_value())
    {
        geometry.width = *patch.width;
        applied = true;
    }

    if (patch.height.has_value())
    {
        geometry.height = *patch.height;
        applied = true;
    }

    if (patch.size.has_value())
    {
        geometry.width = patch.size->x;
        geometry.height = patch.size->y;
        applied = true;
    }

    return applied;
}

bool ApplyTrianglePrimitivePatch(TriangleGeometry& geometry, const PrimitivePatch& patch)
{
    if (!patch.points.has_value() || patch.points->size() != geometry.points.size())
    {
        return false;
    }

    std::copy(patch.points->begin(), patch.points->end(), geometry.points.begin());
    return true;
}

bool ApplyPolylinePrimitivePatch(PolylineGeometry& geometry, const PrimitivePatch& patch)
{
    bool applied = false;

    if (patch.points.has_value() && patch.points->size() >= 2U)
    {
        geometry.points = *patch.points;
        applied = true;
    }

    if (patch.closed.has_value())
    {
        geometry.closed = *patch.closed;
        applied = true;
    }

    return applied;
}

bool ApplyBezierPrimitivePatch(BezierGeometry& geometry, const PrimitivePatch& patch)
{
    bool applied = false;

    if (patch.points.has_value() &&
        patch.points->size() >= 2U &&
        patch.points->size() <= kMaxBezierControlPoints)
    {
        // Reject oversized bezier control-point sets: De Casteljau sampling is O(n^2 * segments) and a
        // large set could stall a frame. Other patch fields still apply so the rest of the update lands.
        geometry.controlPoints = *patch.points;
        applied = true;
    }

    if (patch.segments.has_value())
    {
        geometry.segments = *patch.segments;
        applied = true;
    }

    return applied;
}

bool ApplyArcPrimitivePatch(ArcGeometry& geometry, const PrimitivePatch& patch)
{
    bool applied = false;

    if (patch.radius.has_value())
    {
        geometry.radius = *patch.radius;
        applied = true;
    }

    if (patch.startAngleDegrees.has_value())
    {
        geometry.startAngleDegrees = *patch.startAngleDegrees;
        applied = true;
    }

    if (patch.endAngleDegrees.has_value())
    {
        geometry.endAngleDegrees = *patch.endAngleDegrees;
        applied = true;
    }

    if (patch.segments.has_value())
    {
        geometry.segments = *patch.segments;
        applied = true;
    }

    return applied;
}

bool ApplyPatchToPrimitive(Primitive& primitive, const PrimitivePatch& patch)
{
    bool applied = false;

    if (patch.visible.has_value())
    {
        primitive.style.visible = *patch.visible;
        applied = true;
    }

    if (patch.position.has_value())
    {
        primitive.transform.position = *patch.position;
        applied = true;
    }

    if (patch.rotationDegrees.has_value())
    {
        primitive.transform.rotationDegrees = *patch.rotationDegrees;
        applied = true;
    }

    if (patch.scale.has_value())
    {
        primitive.transform.scale = *patch.scale;
        applied = true;
    }

    if (patch.color.has_value())
    {
        primitive.style.color = *patch.color;
        applied = true;
    }

    if (patch.fillColor.has_value())
    {
        primitive.style.fillColor = *patch.fillColor;
        applied = true;
    }

    if (patch.filled.has_value())
    {
        primitive.style.filled = *patch.filled;
        applied = true;
    }

    if (patch.thickness.has_value())
    {
        primitive.style.thickness = *patch.thickness;
        applied = true;
    }

    if (patch.lineStyle.has_value())
    {
        primitive.style.lineStyle = *patch.lineStyle;
        applied = true;
    }

    if (patch.text.has_value())
    {
        if (TextGeometry* geometry = std::get_if<TextGeometry>(&primitive.geometry))
        {
            geometry->text = *patch.text;
            applied = true;
        }
    }

    if (patch.letterSpacing.has_value())
    {
        if (TextGeometry* textGeometry = std::get_if<TextGeometry>(&primitive.geometry))
        {
            textGeometry->letterSpacing = *patch.letterSpacing;
            applied = true;
        }
        else if (TimeGeometry* timeGeometry = std::get_if<TimeGeometry>(&primitive.geometry))
        {
            timeGeometry->letterSpacing = *patch.letterSpacing;
            applied = true;
        }
    }

    if (TimeGeometry* timeGeometry = std::get_if<TimeGeometry>(&primitive.geometry))
    {
        if (patch.timeValue.has_value())
        {
            timeGeometry->runtimeValueOverride = *patch.timeValue;
            applied = true;
        }

        if (patch.clearTimeValue)
        {
            timeGeometry->runtimeValueOverride.reset();
            applied = true;
        }

        if (patch.timeUtc.has_value())
        {
            timeGeometry->runtimeUtc = *patch.timeUtc;
            applied = true;
        }

        if (patch.timeFields.has_value())
        {
            timeGeometry->runtimeFields = *patch.timeFields;
            applied = true;
        }
    }

    if (patch.lineStart.has_value())
    {
        if (LineGeometry* geometry = std::get_if<LineGeometry>(&primitive.geometry))
        {
            geometry->start = *patch.lineStart;
            applied = true;
        }
    }

    if (patch.lineEnd.has_value())
    {
        if (LineGeometry* geometry = std::get_if<LineGeometry>(&primitive.geometry))
        {
            geometry->end = *patch.lineEnd;
            applied = true;
        }
    }

    if (patch.radius.has_value())
    {
        if (CircleGeometry* circleGeometry = std::get_if<CircleGeometry>(&primitive.geometry))
        {
            circleGeometry->radius = *patch.radius;
            applied = true;
        }
        else if (ArcGeometry* arcGeometry = std::get_if<ArcGeometry>(&primitive.geometry))
        {
            arcGeometry->radius = *patch.radius;
            applied = true;
        }
    }

    if (patch.innerRadius.has_value() || patch.outerRadius.has_value())
    {
        if (RingGeometry* geometry = std::get_if<RingGeometry>(&primitive.geometry))
        {
            if (patch.innerRadius.has_value())
            {
                geometry->innerRadius = *patch.innerRadius;
                applied = true;
            }

            if (patch.outerRadius.has_value())
            {
                geometry->outerRadius = *patch.outerRadius;
                applied = true;
            }
        }
    }

    if (patch.segments.has_value())
    {
        if (RingGeometry* geometry = std::get_if<RingGeometry>(&primitive.geometry))
        {
            geometry->segments = *patch.segments;
            applied = true;
        }
    }

    if (RectangleGeometry* rectangleGeometry = std::get_if<RectangleGeometry>(&primitive.geometry))
    {
        applied = ApplyBoxPrimitivePatch(*rectangleGeometry, patch) || applied;
    }
    else if (EllipseGeometry* ellipseGeometry = std::get_if<EllipseGeometry>(&primitive.geometry))
    {
        applied = ApplyBoxPrimitivePatch(*ellipseGeometry, patch) || applied;
    }
    else if (SquareGeometry* squareGeometry = std::get_if<SquareGeometry>(&primitive.geometry))
    {
        applied = ApplyBoxPrimitivePatch(*squareGeometry, patch) || applied;
    }
    else if (DiamondGeometry* diamondGeometry = std::get_if<DiamondGeometry>(&primitive.geometry))
    {
        applied = ApplyBoxPrimitivePatch(*diamondGeometry, patch) || applied;
    }
    else if (TriangleGeometry* triangleGeometry = std::get_if<TriangleGeometry>(&primitive.geometry))
    {
        applied = ApplyTrianglePrimitivePatch(*triangleGeometry, patch) || applied;
    }
    else if (PolylineGeometry* polylineGeometry = std::get_if<PolylineGeometry>(&primitive.geometry))
    {
        applied = ApplyPolylinePrimitivePatch(*polylineGeometry, patch) || applied;
    }
    else if (BezierGeometry* bezierGeometry = std::get_if<BezierGeometry>(&primitive.geometry))
    {
        applied = ApplyBezierPrimitivePatch(*bezierGeometry, patch) || applied;
    }
    else if (ArcGeometry* arcGeometry = std::get_if<ArcGeometry>(&primitive.geometry))
    {
        applied = ApplyArcPrimitivePatch(*arcGeometry, patch) || applied;
    }

    return applied;
}

bool IsEmptyPatch(const WindowDisplayPatch& patch) noexcept
{
    return !patch.invertColors.has_value() &&
           !patch.brightness.has_value() &&
           !patch.disabled.has_value();
}

float* FindTextLikeLetterSpacing(Primitive& primitive) noexcept
{
    if (TextGeometry* geometry = std::get_if<TextGeometry>(&primitive.geometry))
    {
        return &geometry->letterSpacing;
    }

    if (TimeGeometry* geometry = std::get_if<TimeGeometry>(&primitive.geometry))
    {
        return &geometry->letterSpacing;
    }

    return nullptr;
}

bool ApplyPatchToReticleGroup(ReticleGroup& reticle, const ReticlePatch& patch)
{
    bool applied = false;

    if (patch.visible.has_value())
    {
        reticle.visible = *patch.visible;
        applied = true;
    }

    if (patch.position.has_value())
    {
        reticle.transform.position = *patch.position;
        applied = true;
    }

    if (patch.rotationDegrees.has_value())
    {
        reticle.transform.rotationDegrees = *patch.rotationDegrees;
        applied = true;
    }

    if (patch.scale.has_value())
    {
        reticle.transform.scale = *patch.scale;
        applied = true;
    }

    if (patch.color.has_value())
    {
        reticle.overrides.color = *patch.color;
        applied = true;
    }

    if (patch.thickness.has_value())
    {
        reticle.overrides.thickness = *patch.thickness;
        applied = true;
    }

    if (patch.text.has_value())
    {
        for (auto& primitive : reticle.primitives)
        {
            if (TextGeometry* geometry = std::get_if<TextGeometry>(&primitive.geometry))
            {
                geometry->text = *patch.text;
                applied = true;
                break;
            }
        }
    }

    for (const auto& [primitiveId, text] : patch.texts)
    {
        applied = SetTextPrimitive(reticle, primitiveId, text) || applied;
    }

    if (patch.letterSpacing.has_value())
    {
        for (auto& primitive : reticle.primitives)
        {
            if (float* letterSpacing = FindTextLikeLetterSpacing(primitive))
            {
                *letterSpacing = *patch.letterSpacing;
                applied = true;
                break;
            }
        }
    }

    for (const auto& [primitiveId, letterSpacing] : patch.letterSpacings)
    {
        applied = SetTextPrimitiveLetterSpacing(reticle, primitiveId, letterSpacing) || applied;
    }

    for (const auto& [primitiveId, primitivePatch] : patch.primitivePatches)
    {
        Primitive* primitive = FindPrimitive(reticle, primitiveId);
        if (primitive != nullptr)
        {
            applied = ApplyPatchToPrimitive(*primitive, primitivePatch) || applied;
        }
    }

    return applied;
}

struct BlinkPatchResult
{
    enum class Status
    {
        Unchanged,
        Applied,
        Invalid
    };

    Status status = Status::Unchanged;
    ReticleBlinkState blink {};
};

bool IsBlinkVisible(std::uint32_t durationMs,
                    BlinkClock::time_point blinkEpoch,
                    BlinkClock::time_point now) noexcept;
} // namespace

struct SceneRegistry::PageComponent
{
    std::string name;
    std::string normalizedName;
    std::string title;
    PageTitleDisplayDefinition titleDisplay {};
    TransportId transportId = 0;
    ColorRgba backgroundColor {6, 14, 20, 255};
    PageViewState view {};
    std::unordered_map<std::string, std::size_t, TransparentStringHash, TransparentStringEqual> layerOrdersById {};
    std::unordered_map<std::string, DynamicReticleLayerBinding, TransparentStringHash, TransparentStringEqual>
        dynamicBindingsByTemplate {};
    std::unordered_map<std::string, std::uint32_t, TransparentStringHash, TransparentStringEqual> blinkDurationsByType {};
    std::string defaultBlinkTypeName;
    std::string normalizedDefaultBlinkTypeName;
    std::string activeStrobeName;
    std::string normalizedActiveStrobeName;
    std::uint32_t defaultBlinkDurationMs = 0;
    BlinkClock::time_point blinkEpoch = BlinkClock::now();
    std::vector<entt::entity> orderedReticleEntities;
    bool hasStrobe = false;
    bool active = false;

    [[nodiscard]] std::size_t ResolveLayerOrder(const std::string_view layerId) const noexcept
    {
        const std::string normalizedLayerId = NormalizePageName(layerId);
        const auto iterator = layerOrdersById.find(normalizedLayerId);
        return iterator == layerOrdersById.end() ? layerOrdersById.size() : iterator->second;
    }

    [[nodiscard]] const DynamicReticleLayerBinding* ResolveDynamicBinding(
        const std::string_view templateId) const noexcept
    {
        const std::string normalizedTemplateId = NormalizePageName(templateId);
        if (normalizedTemplateId.empty())
        {
            return nullptr;
        }

        const auto iterator = dynamicBindingsByTemplate.find(normalizedTemplateId);
        return iterator == dynamicBindingsByTemplate.end() ? nullptr : &iterator->second;
    }

    [[nodiscard]] ReticleDrawOrderKey BuildStaticReticleDrawOrderKey(const ReticleGroup& reticle,
                                                                     const std::size_t authoredOrder) const noexcept
    {
        ReticleDrawOrderKey key;
        key.pass = reticle.drawOnTop ? ReticleDrawPass::DrawOnTop : ReticleDrawPass::Normal;
        key.layerOrder = ResolveLayerOrder(reticle.layerId);
        key.band = ReticleDrawBand::Static;
        key.staticAuthoredOrder = authoredOrder;
        return key;
    }

    [[nodiscard]] ReticleDrawOrderKey BuildDynamicReticleDrawOrderKey(const ReticleGroup& reticle,
                                                                      const std::uint64_t creationSequence) const noexcept
    {
        ReticleDrawOrderKey key;
        key.pass = reticle.drawOnTop ? ReticleDrawPass::DrawOnTop : ReticleDrawPass::Normal;
        key.layerOrder = ResolveLayerOrder(reticle.layerId);
        key.band = ReticleDrawBand::Dynamic;
        key.dynamicCreationOrder = creationSequence;

        if (const DynamicReticleLayerBinding* binding = ResolveDynamicBinding(reticle.sourceTemplateId);
            binding != nullptr)
        {
            key.dynamicTemplateOrder = binding->orderInLayer;
        }

        return key;
    }

    [[nodiscard]] bool TryResolveBlinkSelection(const ReticleBlinkState& candidate,
                                                std::uint32_t& durationMs) const noexcept
    {
        if (!candidate.typeName.empty())
        {
            const std::string normalizedTypeName = candidate.normalizedTypeName.empty()
                                                       ? NormalizePageName(candidate.typeName)
                                                       : candidate.normalizedTypeName;
            const auto iterator = blinkDurationsByType.find(normalizedTypeName);
            if (iterator == blinkDurationsByType.end())
            {
                return false;
            }

            durationMs = iterator->second;
            return true;
        }

        if (!candidate.enabled)
        {
            durationMs = 0;
            return true;
        }

        if (defaultBlinkDurationMs == 0)
        {
            return false;
        }

        durationMs = defaultBlinkDurationMs;
        return true;
    }

    [[nodiscard]] BlinkPatchResult EvaluateBlinkPatch(const ReticleBlinkState& currentBlink,
                                                      const ReticlePatch& patch) const noexcept
    {
        if (!patch.blinkEnabled.has_value() && !patch.blinkType.has_value())
        {
            return {};
        }

        BlinkPatchResult result;
        result.status = BlinkPatchResult::Status::Applied;
        result.blink = currentBlink;

        if (patch.blinkType.has_value())
        {
            result.blink.typeName = *patch.blinkType;
            result.blink.normalizedTypeName = NormalizePageName(result.blink.typeName);
            if (!patch.blinkType->empty())
            {
                result.blink.enabled = true;
            }
        }

        if (patch.blinkEnabled.has_value())
        {
            result.blink.enabled = *patch.blinkEnabled;
        }

        std::uint32_t durationMs = result.blink.durationMs;
        if (!TryResolveBlinkSelection(result.blink, durationMs))
        {
            result.status = BlinkPatchResult::Status::Invalid;
            return result;
        }

        result.blink.durationMs = durationMs;
        return result;
    }

    bool ResolveBlinkForReticle(ReticleGroup& reticle) const noexcept
    {
        std::uint32_t durationMs = reticle.blink.durationMs;
        if (!TryResolveBlinkSelection(reticle.blink, durationMs))
        {
            reticle.blink = {};
            return false;
        }

        reticle.blink.durationMs = durationMs;
        return true;
    }

    [[nodiscard]] bool IsReticleVisibleNow(const ReticleGroup& reticle, const BlinkClock::time_point now) const noexcept
    {
        return reticle.visible &&
               (!reticle.blink.enabled || IsBlinkVisible(reticle.blink.durationMs, blinkEpoch, now));
    }
};

struct SceneRegistry::PageMembership
{
    std::string pageName;
};

struct SceneRegistry::ReticleComponent
{
    ReticleGroup group;
    ReticleDrawOrderKey drawOrder {};
    RuntimeDynamicId runtimeReticleId = 0;
    TransportId sourceTemplateTransportId = 0;
    std::uint64_t dynamicCreationSequence = 0;
    // Layer order resolved once at creation/update time, never per frame. layerOrdersById
    // (PageComponent) is only populated at document-load time (no runtime layer-reordering API);
    // the only runtime mutation of layerId is in UpsertDynamicReticle, which already recomputes
    // this alongside drawOrder.
    std::size_t cachedLayerOrder = 0;
    // Normalized once at creation/update time; source of truth to avoid
    // NormalizePageName(sourceTemplateId) per reticle per frame in the render/strobe-scan path.
    // sourceTemplateId itself is only mutated inside UpsertDynamicReticle.
    std::string normalizedSourceTemplateId;
};

struct SceneRegistry::DynamicTag
{
};

struct SceneRegistry::StaticTag
{
};

struct SceneRegistry::StrobeTag
{
};

struct SceneRegistry::StrobeBehaviorComponent
{
    std::string name;
    std::string normalizedName;
    TransportId transportId = 0;
    StrobeCaptureConfig capture;
    StrobeMagnetConfig magnet;
    std::vector<Primitive> authoredPrimitives;
    ReticleStyleOverride authoredOverrides;
    ReticleClipState authoredClipping;
    bool visualShapeApplied = false;
    std::string lockedReticleId;
};

namespace
{
ReticleDrawOrderKey MakeStrobeDrawOrderKey() noexcept
{
    ReticleDrawOrderKey key;
    key.pass = ReticleDrawPass::Strobe;
    key.band = ReticleDrawBand::Strobe;
    return key;
}

bool IsBlinkVisible(const std::uint32_t durationMs,
                    const BlinkClock::time_point blinkEpoch,
                    const BlinkClock::time_point now) noexcept
{
    if (durationMs == 0)
    {
        return true;
    }

    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - blinkEpoch).count();
    const std::uint64_t periodMs = static_cast<std::uint64_t>(durationMs);
    const std::uint64_t phaseMs =
        static_cast<std::uint64_t>(elapsedMs < 0 ? 0 : elapsedMs) % periodMs;
    const std::uint64_t visibleWindowMs = std::max<std::uint64_t>(1U, (periodMs + 1U) / 2U);
    return phaseMs < visibleWindowMs;
}

} // namespace

SceneRegistry::SceneRegistry(MfdDocument document)
{
    LoadDocument(std::move(document));
}

SceneRegistry::SceneRegistry(MfdDocument document, std::optional<GeneratedTransportMap> transportMap)
{
    LoadDocument(std::move(document), std::move(transportMap));
}

void SceneRegistry::LoadDocument(MfdDocument document)
{
    LoadDocument(std::move(document), std::nullopt);
}

void SceneRegistry::LoadDocument(MfdDocument document, std::optional<GeneratedTransportMap> transportMap)
{
    document_ = std::move(document);
    transportMap_ = std::move(transportMap);
    registry_.clear();
    pageEntities_.clear();
    strobeEntities_.clear();
    reticleEntities_.clear();
    dynamicTemplateVisibility_.clear();
    dynamicTemplateStrobeMagnetEnabled_.clear();
    transportPageNames_.clear();
    transportPageEntities_.clear();
    transportReticleEntities_.clear();
    transportReticles_.clear();
    transportTemplates_.clear();
    templateTransportIds_.clear();
    transportPrimitives_.clear();
    transportBlinks_.clear();
    transportStrobes_.clear();
    nextDynamicCreationSequence_ = 1;
    activePage_.clear();
    windowDisplay_ = {};

    for (const auto& page : document_.pages)
    {
        const entt::entity pageEntity = registry_.create();
        PageComponent pageComponent;
        pageComponent.name = page.name;
        pageComponent.normalizedName = page.normalizedName;
        pageComponent.title = page.title;
        pageComponent.titleDisplay = page.titleDisplay;
        pageComponent.backgroundColor = page.backgroundColor;
        pageComponent.view = page.view;
        pageComponent.defaultBlinkTypeName = page.defaultBlinkTypeName;
        pageComponent.normalizedDefaultBlinkTypeName = page.normalizedDefaultBlinkTypeName;
        pageComponent.hasStrobe = !page.strobes.empty();
        if (const PageStrobeDefinition* activeStrobe = FindActivePageStrobeDefinition(page); activeStrobe != nullptr)
        {
            pageComponent.activeStrobeName = activeStrobe->name;
            pageComponent.normalizedActiveStrobeName = activeStrobe->normalizedName;
        }
        pageComponent.blinkEpoch = BlinkClock::now();

        for (std::size_t layerIndex = 0; layerIndex < page.layers.size(); ++layerIndex)
        {
            pageComponent.layerOrdersById.emplace(NormalizePageName(page.layers[layerIndex].id), layerIndex);
        }

        for (const auto& binding : page.dynamicReticleBindings)
        {
            pageComponent.dynamicBindingsByTemplate.emplace(
                NormalizePageName(binding.templateId),
                binding);
        }

        for (const auto& blinkType : page.blinkTypes)
        {
            pageComponent.blinkDurationsByType.emplace(blinkType.normalizedName, blinkType.durationMs);
        }

        if (!pageComponent.normalizedDefaultBlinkTypeName.empty())
        {
            const auto iterator = pageComponent.blinkDurationsByType.find(pageComponent.normalizedDefaultBlinkTypeName);
            if (iterator != pageComponent.blinkDurationsByType.end())
            {
                pageComponent.defaultBlinkDurationMs = iterator->second;
            }
        }
        else if (!page.blinkTypes.empty())
        {
            pageComponent.defaultBlinkTypeName = page.blinkTypes.front().name;
            pageComponent.normalizedDefaultBlinkTypeName = page.blinkTypes.front().normalizedName;
            pageComponent.defaultBlinkDurationMs = page.blinkTypes.front().durationMs;
        }

        registry_.emplace<PageComponent>(pageEntity, std::move(pageComponent));
        const PageComponent& storedPageComponent = registry_.get<PageComponent>(pageEntity);
        pageEntities_.emplace(page.normalizedName, pageEntity);

        for (std::size_t index = 0; index < page.staticReticles.size(); ++index)
        {
            const entt::entity reticleEntity = registry_.create();
            ReticleComponent component {page.staticReticles[index],
                                        storedPageComponent.BuildStaticReticleDrawOrderKey(
                                            page.staticReticles[index],
                                            index)};
            component.cachedLayerOrder = component.drawOrder.layerOrder;
            component.normalizedSourceTemplateId = NormalizePageName(component.group.sourceTemplateId);
            registry_.emplace<ReticleComponent>(reticleEntity, std::move(component));
            registry_.emplace<PageMembership>(reticleEntity, PageMembership {page.normalizedName});
            registry_.emplace<StaticTag>(reticleEntity);
            IndexReticle(page.normalizedName, page.staticReticles[index], reticleEntity);
            InsertReticleIntoPageDrawList(page.normalizedName, reticleEntity);
        }

        for (const PageStrobeDefinition& strobeDefinition : page.strobes)
        {
            const entt::entity strobeEntity = registry_.create();
            ReticleComponent component {strobeDefinition.reticle, MakeStrobeDrawOrderKey()};
            component.cachedLayerOrder = storedPageComponent.ResolveLayerOrder(component.group.layerId);
            component.normalizedSourceTemplateId = NormalizePageName(component.group.sourceTemplateId);
            registry_.emplace<ReticleComponent>(strobeEntity, std::move(component));
            registry_.emplace<PageMembership>(strobeEntity, PageMembership {page.normalizedName});
            StrobeBehaviorComponent behavior;
            behavior.name = strobeDefinition.name;
            behavior.normalizedName = strobeDefinition.normalizedName;
            behavior.capture = strobeDefinition.capture;
            behavior.magnet = strobeDefinition.magnet;
            behavior.authoredPrimitives = strobeDefinition.reticle.primitives;
            behavior.authoredOverrides = strobeDefinition.reticle.overrides;
            behavior.authoredClipping = strobeDefinition.reticle.clipping;
            registry_.emplace<StrobeBehaviorComponent>(strobeEntity, std::move(behavior));
            registry_.emplace<StrobeTag>(strobeEntity);
            strobeEntities_.emplace(MakeStrobeLookupKey(page.normalizedName, strobeDefinition.normalizedName), strobeEntity);
            IndexReticle(page.normalizedName, strobeDefinition.reticle, strobeEntity);
            if (storedPageComponent.normalizedActiveStrobeName == strobeDefinition.normalizedName)
            {
                InsertReticleIntoPageDrawList(page.normalizedName, strobeEntity);
            }
        }
    }

    if (!document_.pages.empty())
    {
        const auto defaultPageIterator = std::find_if(
            document_.pages.begin(),
            document_.pages.end(),
            [](const PageDefinition& page)
            {
                return page.defaultPage;
            });

        activePage_ =
            defaultPageIterator != document_.pages.end()
                ? defaultPageIterator->normalizedName
                : document_.pages.front().normalizedName;
        SetActiveFlag(activePage_, true);
    }

    RebuildTransportIndexes();
}

const MfdDocument& SceneRegistry::Document() const noexcept
{
    return document_;
}

const std::optional<GeneratedTransportMap>& SceneRegistry::TransportMap() const noexcept
{
    return transportMap_;
}

bool SceneRegistry::HasTransportMap() const noexcept
{
    return transportMap_.has_value();
}

void SceneRegistry::RebuildTransportIndexes()
{
    transportPageNames_.clear();
    transportPageEntities_.clear();
    transportReticleEntities_.clear();
    transportReticles_.clear();
    transportTemplates_.clear();
    templateTransportIds_.clear();
    transportPrimitives_.clear();
    transportBlinks_.clear();
    transportStrobes_.clear();

    for (const auto& [normalizedPageName, entity] : pageEntities_)
    {
        if (PageComponent* page = registry_.try_get<PageComponent>(entity); page != nullptr)
        {
            page->transportId = 0;
        }
    }

    auto strobeView = registry_.view<StrobeBehaviorComponent>();
    for (const entt::entity entity : strobeView)
    {
        strobeView.get<StrobeBehaviorComponent>(entity).transportId = 0;
    }

    if (!transportMap_.has_value())
    {
        return;
    }

    for (const auto& page : transportMap_->pages)
    {
        transportPageNames_.emplace(page.id, page.name);
        if (PageComponent* pageComponent = FindPage(page.normalizedName); pageComponent != nullptr)
        {
            pageComponent->transportId = page.id;
        }

        const auto pageEntityIt = pageEntities_.find(page.normalizedName);
        if (pageEntityIt != pageEntities_.end())
        {
            transportPageEntities_.emplace(page.id, pageEntityIt->second);
        }
    }

    for (const auto& reticle : transportMap_->reticles)
    {
        std::string pageName;
        if (reticle.pageId != 0)
        {
            const auto pageNameIt = transportPageNames_.find(reticle.pageId);
            if (pageNameIt == transportPageNames_.end())
            {
                continue;
            }

            pageName = pageNameIt->second;
        }

        const std::string normalizedPageName = NormalizePageName(pageName);
        transportReticles_.emplace(
            reticle.id,
            TransportReticleLookup {reticle.pageId, std::move(pageName), reticle.reticleId});

        const entt::entity reticleEntity = FindReticleEntity(normalizedPageName, reticle.reticleId);
        if (reticleEntity != entt::null)
        {
            transportReticleEntities_.emplace(reticle.id, reticleEntity);
        }
    }

    for (const auto& templ : transportMap_->templates)
    {
        transportTemplates_.emplace(templ.id, templ.templateId);
        templateTransportIds_.emplace(templ.normalizedTemplateId, templ.id);
    }

    for (const auto& primitive : transportMap_->primitives)
    {
        transportPrimitives_.emplace(
            primitive.id,
            TransportPrimitiveLookup {primitive.ownerKind, primitive.ownerId, primitive.primitiveId});
    }

    for (const auto& blink : transportMap_->blinkTypes)
    {
        transportBlinks_.emplace(blink.id, TransportBlinkLookup {blink.pageId, blink.blinkType});
    }

    for (const auto& strobe : transportMap_->strobes)
    {
        transportStrobes_.emplace(strobe.id, TransportStrobeLookup {strobe.pageId, strobe.strobeName});

        const std::string* pageName = ResolvePageName(strobe.pageId);
        if (pageName == nullptr)
        {
            continue;
        }

        const entt::entity entity = FindStrobeEntity(NormalizePageName(*pageName), strobe.normalizedStrobeName);
        if (entity == entt::null)
        {
            continue;
        }

        if (StrobeBehaviorComponent* behavior = registry_.try_get<StrobeBehaviorComponent>(entity); behavior != nullptr)
        {
            behavior->transportId = strobe.id;
        }
    }
}

bool SceneRegistry::HasMatchingTransportMap(const std::string_view mappingHash) const noexcept
{
    return !mappingHash.empty() &&
           transportMap_.has_value() &&
           transportMap_->mappingHash == mappingHash;
}

TransportId SceneRegistry::ResolvePageTransportId(const std::string_view pageName) const noexcept
{
    const PageComponent* page = FindPage(NormalizePageName(pageName));
    return page == nullptr ? 0 : page->transportId;
}

const std::string* SceneRegistry::ResolvePageName(const TransportId pageId) const noexcept
{
    const auto iterator = transportPageNames_.find(pageId);
    return iterator == transportPageNames_.end() ? nullptr : &iterator->second;
}

const SceneRegistry::TransportReticleLookup* SceneRegistry::ResolveStaticReticle(const TransportId reticleId) const noexcept
{
    const auto iterator = transportReticles_.find(reticleId);
    return iterator == transportReticles_.end() ? nullptr : &iterator->second;
}

const std::string* SceneRegistry::ResolveTemplateId(const TransportId templateId) const noexcept
{
    const auto iterator = transportTemplates_.find(templateId);
    return iterator == transportTemplates_.end() ? nullptr : &iterator->second;
}

TransportId SceneRegistry::ResolveTemplateTransportId(const std::string_view templateId) const noexcept
{
    const auto iterator = templateTransportIds_.find(NormalizePageName(templateId));
    return iterator == templateTransportIds_.end() ? 0 : iterator->second;
}

const std::string* SceneRegistry::ResolveBlinkType(const TransportId pageId, const TransportId blinkTypeId) const noexcept
{
    const auto iterator = transportBlinks_.find(blinkTypeId);
    if (iterator == transportBlinks_.end() || iterator->second.pageId != pageId)
    {
        return nullptr;
    }

    return &iterator->second.blinkType;
}

const std::string* SceneRegistry::ResolveStrobeName(const TransportId pageId, const TransportId strobeId) const noexcept
{
    const auto iterator = transportStrobes_.find(strobeId);
    if (iterator == transportStrobes_.end() || iterator->second.pageId != pageId)
    {
        return nullptr;
    }

    return &iterator->second.strobeName;
}

const std::string* SceneRegistry::ResolvePrimitiveIdForReticle(const TransportId reticleId,
                                                               const TransportId primitiveId) const noexcept
{
    const auto iterator = transportPrimitives_.find(primitiveId);
    if (iterator == transportPrimitives_.end() ||
        iterator->second.ownerKind != TransportPrimitiveOwnerKind::Reticle ||
        iterator->second.ownerId != reticleId)
    {
        return nullptr;
    }

    return &iterator->second.primitiveId;
}

const std::string* SceneRegistry::ResolvePrimitiveIdForTemplate(const TransportId templateId,
                                                                const TransportId primitiveId) const noexcept
{
    const auto iterator = transportPrimitives_.find(primitiveId);
    if (iterator == transportPrimitives_.end() ||
        iterator->second.ownerKind != TransportPrimitiveOwnerKind::Template ||
        iterator->second.ownerId != templateId)
    {
        return nullptr;
    }

    return &iterator->second.primitiveId;
}

const ReticleLibrary& SceneRegistry::Library() const noexcept
{
    return document_.reticleLibrary;
}

std::vector<PageSummary> SceneRegistry::Pages() const
{
    std::vector<PageSummary> pages;
    pages.reserve(document_.pages.size());

    for (const auto& page : document_.pages)
    {
        pages.push_back(PageSummary {
            page.name,
            page.title,
            page.titleDisplay,
            page.backgroundColor,
            !page.strobes.empty(),
            page.normalizedName == activePage_});
    }

    return pages;
}

bool SceneRegistry::HasPage(const std::string_view pageName) const noexcept
{
    return HasNormalizedPage(NormalizePageName(pageName));
}

bool SceneRegistry::HasStrobe(const std::string_view pageName) const noexcept
{
    return HasNormalizedStrobe(NormalizePageName(pageName));
}

bool SceneRegistry::ActivePageHasStrobe() const noexcept
{
    return HasNormalizedStrobe(activePage_);
}

bool SceneRegistry::HasNormalizedPage(const std::string_view pageName) const noexcept
{
    return pageEntities_.find(std::string(pageName)) != pageEntities_.end();
}

bool SceneRegistry::HasNormalizedStrobe(const std::string_view pageName) const noexcept
{
    const PageComponent* page = FindPage(pageName);
    return page != nullptr && page->hasStrobe;
}

bool SceneRegistry::IsActivePageKey(const std::string_view pageName) const noexcept
{
    return !activePage_.empty() && pageName == activePage_;
}

void SceneRegistry::SetActivePage(const std::string_view pageName) noexcept
{
    SetActivePageByKey(NormalizePageName(pageName));
}

void SceneRegistry::SetActivePageByKey(const std::string_view normalizedPageName) noexcept
{
    if (!HasNormalizedPage(normalizedPageName))
    {
        return;
    }

    SetActiveFlag(activePage_, false);
    activePage_ = std::string(normalizedPageName);
    SetActiveFlag(activePage_, true);
    RefreshStickyStrobePosition(activePage_);
}

std::string SceneRegistry::ActivePageName() const
{
    const PageComponent* page = FindPage(activePage_);
    return page == nullptr ? std::string {} : page->name;
}

std::optional<PageSummary> SceneRegistry::ActivePageSummary() const
{
    const PageComponent* page = FindPage(activePage_);
    if (page == nullptr)
    {
        return std::nullopt;
    }

    return PageSummary {
        page->name,
        page->title,
        page->titleDisplay,
        page->backgroundColor,
        page->hasStrobe,
        true};
}

std::optional<PageViewState> SceneRegistry::ViewForPage(const std::string_view pageName) const noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    const PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return std::nullopt;
    }

    return page->view;
}

PageViewState SceneRegistry::ActivePageView() const noexcept
{
    const PageComponent* page = FindPage(activePage_);
    return page == nullptr ? PageViewState {} : page->view;
}

std::optional<StrobeSummary> SceneRegistry::StrobeForPage(const std::string_view pageName) const
{
    return StrobeForPageKey(NormalizePageName(pageName));
}

std::optional<StrobeSummary> SceneRegistry::ActiveStrobeSummary() const
{
    return StrobeForPageKey(activePage_);
}

std::optional<StrobeSummary> SceneRegistry::StrobeForPageKey(const std::string_view pageName) const
{
    if (!IsActivePageKey(pageName))
    {
        return std::nullopt;
    }

    const entt::entity strobeEntity = FindActiveStrobeEntity(pageName);
    if (strobeEntity == entt::null)
    {
        return std::nullopt;
    }

    const auto* reticle = registry_.try_get<ReticleComponent>(strobeEntity);
    const auto* behavior = registry_.try_get<StrobeBehaviorComponent>(strobeEntity);
    const PageComponent* page = FindPage(pageName);
    if (reticle == nullptr || behavior == nullptr || page == nullptr)
    {
        return std::nullopt;
    }

    const BlinkClock::time_point now = BlinkClock::now();
    Vec2 strobePosition = reticle->group.transform.position;
    if (const ReticleComponent* lockedTarget =
            FindLockedStrobeMagnetTarget(pageName, behavior->lockedReticleId);
        lockedTarget != nullptr)
    {
        strobePosition = lockedTarget->group.transform.position;
    }

    return StrobeSummary {
        page->name,
        page->transportId,
        behavior->name,
        behavior->transportId,
        reticle->group.id,
        strobePosition,
        behavior->capture,
        behavior->magnet,
        page->IsReticleVisibleNow(reticle->group, now)};
}

std::optional<StrobeMagnetSummary> SceneRegistry::StrobeMagnetForPage(const std::string_view pageName) const
{
    return StrobeMagnetForPageKey(NormalizePageName(pageName));
}

std::optional<StrobeMagnetSummary> SceneRegistry::ActiveStrobeMagnetSummary() const
{
    return StrobeMagnetForPageKey(activePage_);
}

std::optional<StrobeMagnetSummary> SceneRegistry::StrobeMagnetForPageKey(const std::string_view pageName) const
{
    return ScanStrobeByKey(pageName).magnet;
}

ColorRgba SceneRegistry::ActiveBackgroundColor() const noexcept
{
    const PageComponent* page = FindPage(activePage_);
    return page == nullptr ? ColorRgba {6, 14, 20, 255} : page->backgroundColor;
}

WindowDisplayState SceneRegistry::WindowDisplay() const noexcept
{
    return windowDisplay_;
}

void SceneRegistry::CapturePageStateInto(RuntimeSnapshot& snapshot, const PageComponent& page) const
{
    snapshot.pages.push_back(RuntimeSnapshot::PageState {
        page.normalizedName,
        page.activeStrobeName,
        page.view,
        page.blinkEpoch});

    for (const auto& bindingEntry : page.dynamicBindingsByTemplate)
    {
        const std::string& normalizedTemplateId = bindingEntry.first;

        if (!IsDynamicTemplateVisible(page.normalizedName, normalizedTemplateId))
        {
            snapshot.dynamicTemplateVisibility.push_back(
                RuntimeSnapshot::DynamicTemplateVisibilityState {
                    page.normalizedName,
                    normalizedTemplateId,
                    false});
        }

        if (IsDynamicTemplateStrobeMagnetEnabled(page.normalizedName, normalizedTemplateId))
        {
            snapshot.dynamicTemplateStrobeMagnet.push_back(
                RuntimeSnapshot::DynamicTemplateStrobeMagnetState {
                    page.normalizedName,
                    normalizedTemplateId,
                    true});
        }
    }
}

void SceneRegistry::CaptureReticleEntityInto(RuntimeSnapshot& snapshot,
                                             const entt::entity entity,
                                             const PageMembership& membership,
                                             const ReticleComponent& reticle) const
{
    if (registry_.all_of<StrobeTag>(entity))
    {
        if (const auto* behavior = registry_.try_get<StrobeBehaviorComponent>(entity))
        {
            snapshot.strobes.push_back(RuntimeSnapshot::StrobeState {
                membership.pageName,
                behavior->normalizedName,
                reticle.group,
                behavior->capture,
                behavior->magnet,
                behavior->authoredPrimitives,
                behavior->authoredOverrides,
                behavior->authoredClipping,
                behavior->visualShapeApplied,
                behavior->lockedReticleId});
        }

        return;
    }

    if (registry_.all_of<DynamicTag>(entity))
    {
        snapshot.dynamicReticles.push_back(RuntimeSnapshot::DynamicReticleState {
            membership.pageName,
            reticle.group,
            reticle.runtimeReticleId,
            reticle.sourceTemplateTransportId,
            reticle.dynamicCreationSequence});
        return;
    }

    snapshot.staticReticles.push_back(RuntimeSnapshot::ReticleState {
        membership.pageName,
        reticle.group});
}

void SceneRegistry::SortRuntimeSnapshot(RuntimeSnapshot& snapshot)
{
    auto byPageAndId = [](const auto& lhs, const auto& rhs)
    {
        if (lhs.normalizedPageName != rhs.normalizedPageName)
        {
            return lhs.normalizedPageName < rhs.normalizedPageName;
        }

        return lhs.group.id < rhs.group.id;
    };

    std::sort(snapshot.pages.begin(),
              snapshot.pages.end(),
              [](const RuntimeSnapshot::PageState& lhs, const RuntimeSnapshot::PageState& rhs)
              {
                  return lhs.normalizedPageName < rhs.normalizedPageName;
              });
    std::sort(snapshot.staticReticles.begin(), snapshot.staticReticles.end(), byPageAndId);
    std::sort(snapshot.strobes.begin(), snapshot.strobes.end(), byPageAndId);
    std::sort(snapshot.dynamicReticles.begin(),
              snapshot.dynamicReticles.end(),
              [](const RuntimeSnapshot::DynamicReticleState& lhs, const RuntimeSnapshot::DynamicReticleState& rhs)
              {
                  if (lhs.dynamicCreationSequence != rhs.dynamicCreationSequence)
                  {
                      return lhs.dynamicCreationSequence < rhs.dynamicCreationSequence;
                  }

                  if (lhs.normalizedPageName != rhs.normalizedPageName)
                  {
                      return lhs.normalizedPageName < rhs.normalizedPageName;
                  }

                  return lhs.group.id < rhs.group.id;
              });
    std::sort(snapshot.dynamicTemplateVisibility.begin(),
              snapshot.dynamicTemplateVisibility.end(),
              [](const RuntimeSnapshot::DynamicTemplateVisibilityState& lhs,
                 const RuntimeSnapshot::DynamicTemplateVisibilityState& rhs)
              {
                  if (lhs.normalizedPageName != rhs.normalizedPageName)
                  {
                      return lhs.normalizedPageName < rhs.normalizedPageName;
                  }

                  return lhs.normalizedTemplateId < rhs.normalizedTemplateId;
              });
}

SceneRegistry::RuntimeSnapshot SceneRegistry::CaptureRuntimeSnapshot() const
{
    RuntimeSnapshot snapshot;
    snapshot.activePage = activePage_;
    snapshot.windowDisplay = windowDisplay_;
    snapshot.nextDynamicCreationSequence = nextDynamicCreationSequence_;

    snapshot.pages.reserve(pageEntities_.size());
    for (const auto& pageEntry : pageEntities_)
    {
        const auto entity = pageEntry.second;
        if (const PageComponent* page = registry_.try_get<PageComponent>(entity))
        {
            CapturePageStateInto(snapshot, *page);
        }
    }

    auto reticleView = registry_.view<PageMembership, ReticleComponent>();
    for (const entt::entity entity : reticleView)
    {
        CaptureReticleEntityInto(snapshot,
                                 entity,
                                 reticleView.get<PageMembership>(entity),
                                 reticleView.get<ReticleComponent>(entity));
    }

    SortRuntimeSnapshot(snapshot);
    return snapshot;
}

SceneRegistry::RuntimeSnapshot SceneRegistry::CaptureRuntimeSnapshotForPages(
    std::vector<std::string> normalizedPageKeys) const
{
    std::sort(normalizedPageKeys.begin(), normalizedPageKeys.end());
    normalizedPageKeys.erase(std::unique(normalizedPageKeys.begin(), normalizedPageKeys.end()),
                             normalizedPageKeys.end());

    RuntimeSnapshot snapshot;
    snapshot.scoped = true;
    snapshot.activePage = activePage_;
    snapshot.windowDisplay = windowDisplay_;
    snapshot.nextDynamicCreationSequence = nextDynamicCreationSequence_;

    snapshot.pages.reserve(normalizedPageKeys.size());
    for (const std::string& pageKey : normalizedPageKeys)
    {
        if (const PageComponent* page = FindPage(pageKey))
        {
            CapturePageStateInto(snapshot, *page);
        }
    }

    auto reticleView = registry_.view<PageMembership, ReticleComponent>();
    for (const entt::entity entity : reticleView)
    {
        const auto& membership = reticleView.get<PageMembership>(entity);
        if (!std::binary_search(normalizedPageKeys.begin(), normalizedPageKeys.end(), membership.pageName))
        {
            continue;
        }

        CaptureReticleEntityInto(snapshot, entity, membership, reticleView.get<ReticleComponent>(entity));
    }

    SortRuntimeSnapshot(snapshot);
    snapshot.scopedPageKeys = std::move(normalizedPageKeys);
    return snapshot;
}

void SceneRegistry::EraseDynamicTemplateOverridesForPage(const std::string_view normalizedPageName)
{
    const std::string keyPrefix = std::string(normalizedPageName) + '\x1F';
    const auto matchesPage = [&keyPrefix](const std::string& key)
    {
        return key.compare(0, keyPrefix.size(), keyPrefix) == 0;
    };

    for (auto iterator = dynamicTemplateVisibility_.begin(); iterator != dynamicTemplateVisibility_.end();)
    {
        iterator = matchesPage(iterator->first) ? dynamicTemplateVisibility_.erase(iterator) : std::next(iterator);
    }

    for (auto iterator = dynamicTemplateStrobeMagnetEnabled_.begin();
         iterator != dynamicTemplateStrobeMagnetEnabled_.end();)
    {
        iterator =
            matchesPage(iterator->first) ? dynamicTemplateStrobeMagnetEnabled_.erase(iterator) : std::next(iterator);
    }
}

void SceneRegistry::RestoreRuntimeSnapshot(const RuntimeSnapshot& snapshot)
{
    if (snapshot.scoped)
    {
        RestoreScopedRuntimeSnapshot(snapshot);
        return;
    }

    LoadDocument(document_, transportMap_);
    ApplySnapshotState(snapshot);
}

void SceneRegistry::RestoreScopedRuntimeSnapshot(const RuntimeSnapshot& snapshot)
{
    // Reset only the pages the failed batch touched to their neutral state (no dynamic
    // reticles, no template overrides), then re-apply the captured page rows. Static
    // reticles, strobes and page fields are overwritten by ApplySnapshotState, so they
    // do not need a reset pass.
    for (const std::string& pageKey : snapshot.scopedPageKeys)
    {
        ClearDynamicReticles(pageKey);
        EraseDynamicTemplateOverridesForPage(pageKey);
    }

    ApplySnapshotState(snapshot);
}

void SceneRegistry::ApplySnapshotState(const RuntimeSnapshot& snapshot)
{
    windowDisplay_ = snapshot.windowDisplay;

    for (const RuntimeSnapshot::PageState& pageState : snapshot.pages)
    {
        if (PageComponent* page = FindPage(pageState.normalizedPageName))
        {
            page->view = pageState.view;
            page->blinkEpoch = pageState.blinkEpoch;
        }
    }

    for (const RuntimeSnapshot::ReticleState& reticleState : snapshot.staticReticles)
    {
        const entt::entity entity = FindReticleEntity(reticleState.normalizedPageName, reticleState.group.id);
        if (ReticleComponent* component = registry_.try_get<ReticleComponent>(entity))
        {
            component->group = reticleState.group;
        }
    }

    for (const RuntimeSnapshot::DynamicTemplateVisibilityState& visibilityState : snapshot.dynamicTemplateVisibility)
    {
        if (!visibilityState.visible)
        {
            dynamicTemplateVisibility_[MakeDynamicTemplateLookupKey(visibilityState.normalizedPageName,
                                                                    visibilityState.normalizedTemplateId)] = false;
        }
    }

    for (const RuntimeSnapshot::DynamicTemplateStrobeMagnetState& magnetState :
         snapshot.dynamicTemplateStrobeMagnet)
    {
        if (magnetState.enabled)
        {
            dynamicTemplateStrobeMagnetEnabled_[MakeDynamicTemplateLookupKey(magnetState.normalizedPageName,
                                                                             magnetState.normalizedTemplateId)] = true;
        }
    }

    for (const RuntimeSnapshot::DynamicReticleState& reticleState : snapshot.dynamicReticles)
    {
        UpsertDynamicReticle(reticleState.normalizedPageName, reticleState.group);

        const entt::entity entity = FindReticleEntity(reticleState.normalizedPageName, reticleState.group.id);
        if (entity == entt::null)
        {
            continue;
        }

        PageComponent* page = FindPage(reticleState.normalizedPageName);
        ReticleComponent* component = registry_.try_get<ReticleComponent>(entity);
        if (page == nullptr || component == nullptr)
        {
            continue;
        }

        component->runtimeReticleId = reticleState.runtimeReticleId;
        component->sourceTemplateTransportId = reticleState.sourceTemplateTransportId;
        component->dynamicCreationSequence = reticleState.dynamicCreationSequence;
        component->drawOrder = page->BuildDynamicReticleDrawOrderKey(component->group,
                                                                     reticleState.dynamicCreationSequence);
        RemoveReticleFromPageDrawList(reticleState.normalizedPageName, entity);
        InsertReticleIntoPageDrawList(reticleState.normalizedPageName, entity);
    }

    for (const RuntimeSnapshot::StrobeState& strobeState : snapshot.strobes)
    {
        const entt::entity entity = FindStrobeEntity(strobeState.normalizedPageName, strobeState.normalizedStrobeName);
        if (entity == entt::null)
        {
            continue;
        }

        if (ReticleComponent* component = registry_.try_get<ReticleComponent>(entity))
        {
            component->group = strobeState.group;
        }

        if (StrobeBehaviorComponent* behavior = registry_.try_get<StrobeBehaviorComponent>(entity))
        {
            behavior->capture = strobeState.capture;
            behavior->magnet = strobeState.magnet;
            behavior->authoredPrimitives = strobeState.authoredPrimitives;
            behavior->authoredOverrides = strobeState.authoredOverrides;
            behavior->authoredClipping = strobeState.authoredClipping;
            behavior->visualShapeApplied = strobeState.visualShapeApplied;
            behavior->lockedReticleId = strobeState.lockedReticleId;
        }
    }

    for (const RuntimeSnapshot::PageState& pageState : snapshot.pages)
    {
        if (!pageState.activeStrobeName.empty())
        {
            SelectStrobe(pageState.normalizedPageName, pageState.activeStrobeName);
        }
    }

    if (!snapshot.activePage.empty())
    {
        SetActivePage(snapshot.activePage);
    }

    nextDynamicCreationSequence_ = std::max<std::uint64_t>(1U, snapshot.nextDynamicCreationSequence);
}

std::vector<ReticleGroup> SceneRegistry::CollectPageReticles(const std::string_view pageName) const
{
    return CollectPageReticlesByKey(NormalizePageName(pageName));
}

std::vector<ReticleGroup> SceneRegistry::CollectPageReticlesByKey(const std::string_view pageName) const
{
    const PageComponent* page = FindPage(pageName);
    if (page == nullptr)
    {
        return {};
    }

    std::vector<ReticleGroup> result;
    result.reserve(page->orderedReticleEntities.size());
    const BlinkClock::time_point now = BlinkClock::now();

    for (const entt::entity entity : page->orderedReticleEntities)
    {
        const ReticleComponent* reticle = registry_.try_get<ReticleComponent>(entity);
        if (reticle == nullptr)
        {
            continue;
        }

        bool visible = page->IsReticleVisibleNow(reticle->group, now);
        if (visible && registry_.all_of<DynamicTag>(entity))
        {
            visible = IsDynamicTemplateVisible(pageName, reticle->group.sourceTemplateId);
        }

        ReticleGroup copy = reticle->group;
        copy.visible = visible;
        result.push_back(std::move(copy));
    }

    return result;
}

std::vector<ReticleRenderView> SceneRegistry::CollectPageReticleViews(const std::string_view pageName) const
{
    return CollectPageReticleViewsByKey(NormalizePageName(pageName));
}

void SceneRegistry::CollectPageReticleViews(const std::string_view pageName,
                                            std::vector<ReticleRenderView>& destination) const
{
    CollectPageReticleViewsByKey(NormalizePageName(pageName), destination);
}

std::vector<ReticleRenderView> SceneRegistry::CollectPageReticleViewsByKey(const std::string_view pageName) const
{
    std::vector<ReticleRenderView> result;
    CollectPageReticleViewsByKey(pageName, result);
    return result;
}

void SceneRegistry::CollectPageReticleViewsByKey(const std::string_view pageName,
                                                 std::vector<ReticleRenderView>& destination) const
{
    destination.clear();

    const PageComponent* page = FindPage(pageName);
    if (page == nullptr)
    {
        return;
    }

    destination.reserve(page->orderedReticleEntities.size());
    const BlinkClock::time_point now = BlinkClock::now();

    for (const entt::entity entity : page->orderedReticleEntities)
    {
        const ReticleComponent* reticle = registry_.try_get<ReticleComponent>(entity);
        if (reticle == nullptr)
        {
            continue;
        }

        bool visible = page->IsReticleVisibleNow(reticle->group, now);
        if (visible && registry_.all_of<DynamicTag>(entity))
        {
            visible = IsDynamicTemplateVisibleByNormalizedId(pageName, reticle->normalizedSourceTemplateId);
        }

        destination.push_back(ReticleRenderView {
            &reticle->group,
            visible,
            reticle->cachedLayerOrder});
    }
}

std::vector<const ReticleGroup*> SceneRegistry::CollectPageReticlePointers(const std::string_view pageName) const
{
    return CollectPageReticlePointersByKey(NormalizePageName(pageName));
}

std::vector<const ReticleGroup*> SceneRegistry::CollectPageReticlePointersByKey(const std::string_view pageName) const
{
    const PageComponent* page = FindPage(pageName);
    if (page == nullptr)
    {
        return {};
    }

    std::vector<const ReticleGroup*> result;
    result.reserve(page->orderedReticleEntities.size());

    for (const entt::entity entity : page->orderedReticleEntities)
    {
        if (const ReticleComponent* reticle = registry_.try_get<ReticleComponent>(entity); reticle != nullptr)
        {
            result.push_back(&reticle->group);
        }
    }

    return result;
}

std::vector<ReticleGroup> SceneRegistry::CollectActiveReticles() const
{
    return CollectPageReticlesByKey(activePage_);
}

std::vector<ReticleRenderView> SceneRegistry::CollectActiveReticleViews() const
{
    return CollectPageReticleViewsByKey(activePage_);
}

void SceneRegistry::CollectActiveReticleViews(std::vector<ReticleRenderView>& destination) const
{
    CollectPageReticleViewsByKey(activePage_, destination);
}

std::vector<const ReticleGroup*> SceneRegistry::CollectActiveReticlePointers() const
{
    return CollectPageReticlePointersByKey(activePage_);
}

bool SceneRegistry::SetPageView(const std::string_view pageName, const PageViewState& view) noexcept
{
    return SetPageViewByKey(NormalizePageName(pageName), view);
}

bool SceneRegistry::SetPageViewByKey(const std::string_view normalizedPageName, const PageViewState& view) noexcept
{
    if (!IsValidPageView(view))
    {
        return false;
    }

    PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return false;
    }

    page->view.center = view.center;
    page->view.zoom = SanitizeZoom(view.zoom);
    return true;
}

bool SceneRegistry::SetPageViewCenter(const std::string_view pageName, const Vec2 center) noexcept
{
    if (!IsValidVec2(center))
    {
        return false;
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return false;
    }

    page->view.center = center;
    return true;
}

bool SceneRegistry::SetPageZoom(const std::string_view pageName, const float zoom) noexcept
{
    if (!IsPositiveFiniteWithin(zoom, kMaxZoom))
    {
        return false;
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return false;
    }

    page->view.zoom = SanitizeZoom(zoom);
    return true;
}

bool SceneRegistry::SetWindowColorInverted(const bool invertColors) noexcept
{
    windowDisplay_.invertColors = invertColors;
    return true;
}

bool SceneRegistry::SetWindowBrightness(const float brightness) noexcept
{
    if (!std::isfinite(brightness))
    {
        return false;
    }

    windowDisplay_.brightness = std::clamp(brightness, 0.0f, 1.0f);
    return true;
}

bool SceneRegistry::SetWindowDisabled(const bool disabled) noexcept
{
    windowDisplay_.disabled = disabled;
    return true;
}

bool SceneRegistry::ApplyWindowDisplayPatch(const WindowDisplayPatch& patch) noexcept
{
    if (patch.brightness.has_value() && !std::isfinite(*patch.brightness))
    {
        return false;
    }

    bool applied = false;

    if (patch.invertColors.has_value())
    {
        windowDisplay_.invertColors = *patch.invertColors;
        applied = true;
    }

    if (patch.disabled.has_value())
    {
        windowDisplay_.disabled = *patch.disabled;
        applied = true;
    }

    if (patch.brightness.has_value())
    {
        windowDisplay_.brightness = std::clamp(*patch.brightness, 0.0f, 1.0f);
        applied = true;
    }

    return applied || IsEmptyPatch(patch);
}

bool SceneRegistry::SetReticleVisible(const std::string_view pageName,
                                      const std::string_view reticleId,
                                      const bool visible) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    if (ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId))
    {
        reticle->group.visible = visible;
        return true;
    }

    return false;
}

bool SceneRegistry::SetReticleBlinkEnabled(const std::string_view pageName,
                                           const std::string_view reticleId,
                                           const bool enabled) noexcept
{
    ReticlePatch patch;
    patch.blinkEnabled = enabled;
    return ApplyReticlePatch(pageName, reticleId, patch);
}

bool SceneRegistry::SetReticleBlinkType(const std::string_view pageName,
                                        const std::string_view reticleId,
                                        const std::string_view blinkType) noexcept
{
    ReticlePatch patch;
    patch.blinkType = std::string(blinkType);
    return ApplyReticlePatch(pageName, reticleId, patch);
}

bool SceneRegistry::ClearReticleBlinkType(const std::string_view pageName,
                                          const std::string_view reticleId) noexcept
{
    ReticlePatch patch;
    patch.blinkType = std::string {};
    return ApplyReticlePatch(pageName, reticleId, patch);
}

bool SceneRegistry::SetReticlePosition(const std::string_view pageName,
                                       const std::string_view reticleId,
                                       const Vec2 position) noexcept
{
    if (!IsValidVec2(position))
    {
        return false;
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    if (ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId))
    {
        reticle->group.transform.position = position;
        return true;
    }

    return false;
}

bool SceneRegistry::SetReticleRotation(const std::string_view pageName,
                                       const std::string_view reticleId,
                                       const float rotationDegrees) noexcept
{
    if (!IsFiniteAbsWithin(rotationDegrees, kMaxAbsAngleDegrees))
    {
        return false;
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    if (ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId))
    {
        reticle->group.transform.rotationDegrees = rotationDegrees;
        return true;
    }

    return false;
}

bool SceneRegistry::SetReticleColor(const std::string_view pageName,
                                    const std::string_view reticleId,
                                    const ColorRgba color) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    if (ReticleComponent* reticle = entity == entt::null ? nullptr : registry_.try_get<ReticleComponent>(entity))
    {
        reticle->group.overrides.color = color;
        if (StrobeBehaviorComponent* behavior = registry_.try_get<StrobeBehaviorComponent>(entity);
            behavior != nullptr && behavior->visualShapeApplied)
        {
            behavior->authoredOverrides.color = color;
        }
        return true;
    }

    return false;
}

bool SceneRegistry::SetReticleThickness(const std::string_view pageName,
                                        const std::string_view reticleId,
                                        const float thickness) noexcept
{
    if (!IsPositiveFiniteWithin(thickness, kMaxThickness))
    {
        return false;
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    if (ReticleComponent* reticle = entity == entt::null ? nullptr : registry_.try_get<ReticleComponent>(entity))
    {
        reticle->group.overrides.thickness = thickness;
        if (StrobeBehaviorComponent* behavior = registry_.try_get<StrobeBehaviorComponent>(entity);
            behavior != nullptr && behavior->visualShapeApplied)
        {
            behavior->authoredOverrides.thickness = thickness;
        }
        return true;
    }

    return false;
}

bool SceneRegistry::SetReticleText(const std::string_view pageName,
                                   const std::string_view reticleId,
                                   std::string value) noexcept
{
    if (!IsValidTextPayload(value))
    {
        return false;
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    ReticleComponent* reticle = entity == entt::null ? nullptr : registry_.try_get<ReticleComponent>(entity);
    if (reticle == nullptr)
    {
        return false;
    }

    for (auto& primitive : reticle->group.primitives)
    {
        if (TextGeometry* geometry = std::get_if<TextGeometry>(&primitive.geometry))
        {
            geometry->text = std::move(value);
            return true;
        }
    }

    StrobeBehaviorComponent* behavior = registry_.try_get<StrobeBehaviorComponent>(entity);
    if (behavior != nullptr && behavior->visualShapeApplied)
    {
        for (auto& primitive : behavior->authoredPrimitives)
        {
            if (TextGeometry* geometry = std::get_if<TextGeometry>(&primitive.geometry))
            {
                geometry->text = std::move(value);
                return true;
            }
        }
    }

    return false;
}

bool SceneRegistry::SetReticleText(const std::string_view pageName,
                                   const std::string_view reticleId,
                                   const std::string_view primitiveId,
                                   std::string value) noexcept
{
    if (!IsValidTextPayload(value))
    {
        return false;
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    ReticleComponent* reticle = entity == entt::null ? nullptr : registry_.try_get<ReticleComponent>(entity);
    if (reticle == nullptr)
    {
        return false;
    }

    if (SetTextPrimitive(reticle->group, primitiveId, value))
    {
        return true;
    }

    StrobeBehaviorComponent* behavior = registry_.try_get<StrobeBehaviorComponent>(entity);
    if (behavior == nullptr || !behavior->visualShapeApplied)
    {
        return false;
    }

    ReticleGroup authoredTarget;
    authoredTarget.primitives = behavior->authoredPrimitives;
    if (!SetTextPrimitive(authoredTarget, primitiveId, std::move(value)))
    {
        return false;
    }

    behavior->authoredPrimitives = std::move(authoredTarget.primitives);
    return true;
}

bool SceneRegistry::SetReticleLetterSpacing(const std::string_view pageName,
                                            const std::string_view reticleId,
                                            const float letterSpacing) noexcept
{
    if (!IsFiniteAbsWithin(letterSpacing, kMaxLogicalSize))
    {
        return false;
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    ReticleComponent* reticle = entity == entt::null ? nullptr : registry_.try_get<ReticleComponent>(entity);
    if (reticle == nullptr)
    {
        return false;
    }

    for (auto& primitive : reticle->group.primitives)
    {
        if (float* primitiveLetterSpacing = FindTextLikeLetterSpacing(primitive))
        {
            *primitiveLetterSpacing = letterSpacing;
            return true;
        }
    }

    StrobeBehaviorComponent* behavior = registry_.try_get<StrobeBehaviorComponent>(entity);
    if (behavior != nullptr && behavior->visualShapeApplied)
    {
        for (auto& primitive : behavior->authoredPrimitives)
        {
            if (float* primitiveLetterSpacing = FindTextLikeLetterSpacing(primitive))
            {
                *primitiveLetterSpacing = letterSpacing;
                return true;
            }
        }
    }

    return false;
}

bool SceneRegistry::SetReticleLetterSpacing(const std::string_view pageName,
                                            const std::string_view reticleId,
                                            const std::string_view primitiveId,
                                            const float letterSpacing) noexcept
{
    if (!IsFiniteAbsWithin(letterSpacing, kMaxLogicalSize))
    {
        return false;
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    ReticleComponent* reticle = entity == entt::null ? nullptr : registry_.try_get<ReticleComponent>(entity);
    if (reticle == nullptr)
    {
        return false;
    }

    if (SetTextPrimitiveLetterSpacing(reticle->group, primitiveId, letterSpacing))
    {
        return true;
    }

    StrobeBehaviorComponent* behavior = registry_.try_get<StrobeBehaviorComponent>(entity);
    if (behavior == nullptr || !behavior->visualShapeApplied)
    {
        return false;
    }

    ReticleGroup authoredTarget;
    authoredTarget.primitives = behavior->authoredPrimitives;
    if (!SetTextPrimitiveLetterSpacing(authoredTarget, primitiveId, letterSpacing))
    {
        return false;
    }

    behavior->authoredPrimitives = std::move(authoredTarget.primitives);
    return true;
}

bool SceneRegistry::ApplyReticlePatch(const std::string_view pageName,
                                      const std::string_view reticleId,
                                      const ReticlePatch& patch) noexcept
{
    const auto reticleRef = ResolveStaticReticleRef(0, pageName, reticleId);
    return reticleRef.has_value() && ApplyReticlePatchByRef(*reticleRef, patch);
}

bool SceneRegistry::ApplyReticlePatchByRef(const ResolvedStaticReticleRef& reticleRef,
                                           const ReticlePatch& patch) noexcept
{
    if (!IsValidReticlePatch(patch))
    {
        return false;
    }

    PageComponent* page = reticleRef.page;
    const entt::entity entity = reticleRef.entity;
    ReticleComponent* reticle = reticleRef.reticle;
    if (page == nullptr || reticle == nullptr)
    {
        return false;
    }

    const BlinkPatchResult blinkResult = page->EvaluateBlinkPatch(reticle->group.blink, patch);
    if (blinkResult.status == BlinkPatchResult::Status::Invalid)
    {
        return false;
    }

    bool applied = ApplyPatchToReticleGroup(reticle->group, patch);
    StrobeBehaviorComponent* behavior = registry_.try_get<StrobeBehaviorComponent>(entity);
    if (behavior != nullptr && behavior->visualShapeApplied)
    {
        ReticleGroup authoredTarget;
        authoredTarget.primitives = behavior->authoredPrimitives;
        authoredTarget.overrides = behavior->authoredOverrides;
        authoredTarget.clipping = behavior->authoredClipping;

        if (ApplyPatchToReticleGroup(authoredTarget, patch))
        {
            behavior->authoredPrimitives = std::move(authoredTarget.primitives);
            behavior->authoredOverrides = authoredTarget.overrides;
            behavior->authoredClipping = authoredTarget.clipping;
            applied = true;
        }
    }

    if (blinkResult.status == BlinkPatchResult::Status::Applied)
    {
        reticle->group.blink = blinkResult.blink;
        applied = true;
    }

    return applied || IsEmptyPatch(patch);
}

bool SceneRegistry::HasDynamicReticle(const std::string_view pageName, const std::string_view reticleId) const noexcept
{
    return HasDynamicReticleByKey(NormalizePageName(pageName), reticleId);
}

bool SceneRegistry::HasDynamicReticleByKey(const std::string_view normalizedPageName,
                                           const std::string_view reticleId) const noexcept
{
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    return entity != entt::null && registry_.all_of<DynamicTag>(entity);
}

bool SceneRegistry::DynamicReticleUsesTemplate(const std::string_view pageName,
                                               const std::string_view reticleId,
                                               const std::string_view templateId) const noexcept
{
    return DynamicReticleUsesTemplateByKey(NormalizePageName(pageName), reticleId, templateId);
}

bool SceneRegistry::DynamicReticleUsesTemplateByKey(const std::string_view normalizedPageName,
                                                    const std::string_view reticleId,
                                                    const std::string_view templateId) const noexcept
{
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    const ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId);
    return entity != entt::null &&
           registry_.all_of<DynamicTag>(entity) &&
           reticle != nullptr &&
           PageNamesEqual(reticle->group.sourceTemplateId, templateId);
}

std::string SceneRegistry::NormalizedPageKeyFor(const TransportId pageId, const std::string_view pageName) const
{
    if (pageId != 0)
    {
        const auto iterator = transportPageEntities_.find(pageId);
        if (iterator == transportPageEntities_.end())
        {
            return {};
        }

        const PageComponent* page = registry_.try_get<PageComponent>(iterator->second);
        return page == nullptr ? std::string {} : page->normalizedName;
    }

    return NormalizePageName(pageName);
}

std::optional<SceneRegistry::ResolvedPageRef> SceneRegistry::ResolvePageRef(const TransportId pageId,
                                                                             const std::string_view pageName) noexcept
{
    entt::entity entity = entt::null;
    if (pageId != 0)
    {
        const auto iterator = transportPageEntities_.find(pageId);
        if (iterator == transportPageEntities_.end())
        {
            return std::nullopt;
        }

        entity = iterator->second;
    }
    else
    {
        const auto iterator = pageEntities_.find(NormalizePageName(pageName));
        if (iterator == pageEntities_.end())
        {
            return std::nullopt;
        }

        entity = iterator->second;
    }

    PageComponent* page = registry_.try_get<PageComponent>(entity);
    if (page == nullptr)
    {
        return std::nullopt;
    }

    return ResolvedPageRef {entity, page, page->normalizedName, &page->name, page->transportId};
}

std::optional<SceneRegistry::ResolvedTemplateRef> SceneRegistry::ResolveTemplateRef(
    const TransportId templateTransportId,
    const std::string_view templateId) const
{
    const std::string* authoredTemplateId = nullptr;
    if (templateTransportId != 0)
    {
        const auto iterator = transportTemplates_.find(templateTransportId);
        if (iterator == transportTemplates_.end())
        {
            return std::nullopt;
        }

        authoredTemplateId = &iterator->second;
    }

    const auto libraryIterator = authoredTemplateId != nullptr
                                     ? document_.reticleLibrary.find(*authoredTemplateId)
                                     : document_.reticleLibrary.find(std::string(templateId));
    if (libraryIterator == document_.reticleLibrary.end())
    {
        return std::nullopt;
    }

    return ResolvedTemplateRef {&libraryIterator->second, &libraryIterator->first, templateTransportId};
}

std::optional<SceneRegistry::ResolvedStaticReticleRef> SceneRegistry::ResolveStaticReticleRef(
    const TransportId reticleTransportId,
    const std::string_view pageName,
    const std::string_view reticleId) noexcept
{
    entt::entity entity = entt::null;
    if (reticleTransportId != 0)
    {
        const auto iterator = transportReticleEntities_.find(reticleTransportId);
        if (iterator == transportReticleEntities_.end())
        {
            return std::nullopt;
        }

        entity = iterator->second;
    }
    else
    {
        entity = FindReticleEntity(NormalizePageName(pageName), reticleId);
        if (entity == entt::null)
        {
            return std::nullopt;
        }
    }

    ReticleComponent* reticle = registry_.try_get<ReticleComponent>(entity);
    const PageMembership* membership = registry_.try_get<PageMembership>(entity);
    if (reticle == nullptr || membership == nullptr)
    {
        return std::nullopt;
    }

    PageComponent* page = FindPage(membership->pageName);
    if (page == nullptr)
    {
        return std::nullopt;
    }

    return ResolvedStaticReticleRef {entity, reticle, page, membership->pageName, reticleTransportId};
}

std::string SceneRegistry::DescribeDynamicReticleUpsertCheck(const DynamicReticleUpsertCheck check,
                                                              const std::string_view reticleId,
                                                              const std::string_view pageName)
{
    switch (check)
    {
    case DynamicReticleUpsertCheck::UnknownPage:
        return "Unknown page: " + std::string(pageName);
    case DynamicReticleUpsertCheck::BlankReticleId:
        return "Dynamic reticle updates require a non-blank reticle id on page '" + std::string(pageName) + "'";
    case DynamicReticleUpsertCheck::IdCollidesWithNonDynamicReticle:
        return "Dynamic reticle '" + std::string(reticleId) + "' on page '" + std::string(pageName) +
               "' collides with an authored reticle id";
    case DynamicReticleUpsertCheck::TemplateConflict:
        return "Dynamic reticle '" + std::string(reticleId) + "' on page '" + std::string(pageName) +
               "' already belongs to another template";
    case DynamicReticleUpsertCheck::InvalidPatch:
    case DynamicReticleUpsertCheck::UnresolvableBlink:
        return "Unable to update dynamic reticle '" + std::string(reticleId) + "'";
    case DynamicReticleUpsertCheck::Applicable:
        break;
    }

    return {};
}

SceneRegistry::DynamicReticleUpsertCheck SceneRegistry::CheckDynamicReticleUpsert(
    const std::string_view normalizedPageName,
    const std::string_view reticleId,
    const std::string_view templateId,
    const ReticleGroup& templateReticle,
    const ReticlePatch& patch) const
{
    const PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return DynamicReticleUpsertCheck::UnknownPage;
    }

    if (NormalizeReticleId(reticleId).empty())
    {
        return DynamicReticleUpsertCheck::BlankReticleId;
    }

    if (!IsValidReticlePatch(patch))
    {
        return DynamicReticleUpsertCheck::InvalidPatch;
    }

    ReticleBlinkState blinkBeforePatch {};
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    if (entity != entt::null)
    {
        const ReticleComponent* existing = registry_.try_get<ReticleComponent>(entity);
        if (existing == nullptr || !registry_.all_of<DynamicTag>(entity))
        {
            return DynamicReticleUpsertCheck::IdCollidesWithNonDynamicReticle;
        }

        if (!PageNamesEqual(existing->group.sourceTemplateId, templateId))
        {
            return DynamicReticleUpsertCheck::TemplateConflict;
        }

        blinkBeforePatch = existing->group.blink;
    }
    else
    {
        // A new reticle starts from the template blink, which UpsertDynamicReticleByKey
        // resolves through ResolveBlinkForReticle (clearing it when unresolvable) before
        // the patch is evaluated on top.
        blinkBeforePatch = templateReticle.blink;
        std::uint32_t durationMs = blinkBeforePatch.durationMs;
        if (page->TryResolveBlinkSelection(blinkBeforePatch, durationMs))
        {
            blinkBeforePatch.durationMs = durationMs;
        }
        else
        {
            blinkBeforePatch = {};
        }
    }

    if (page->EvaluateBlinkPatch(blinkBeforePatch, patch).status == BlinkPatchResult::Status::Invalid)
    {
        return DynamicReticleUpsertCheck::UnresolvableBlink;
    }

    return DynamicReticleUpsertCheck::Applicable;
}

bool SceneRegistry::ApplyDynamicReticlePatch(const std::string_view pageName,
                                             const std::string_view reticleId,
                                             const ReticlePatch& patch) noexcept
{
    return ApplyDynamicReticlePatchByKey(NormalizePageName(pageName), reticleId, patch);
}

bool SceneRegistry::ApplyDynamicReticlePatchByKey(const std::string_view normalizedPageName,
                                                  const std::string_view reticleId,
                                                  const ReticlePatch& patch) noexcept
{
    if (!IsValidReticlePatch(patch))
    {
        return false;
    }

    PageComponent* page = FindPage(normalizedPageName);
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    if (page == nullptr || entity == entt::null || !registry_.all_of<DynamicTag>(entity))
    {
        return false;
    }

    ReticleComponent* reticle = registry_.try_get<ReticleComponent>(entity);
    if (reticle == nullptr)
    {
        return false;
    }

    const BlinkPatchResult blinkResult = page->EvaluateBlinkPatch(reticle->group.blink, patch);
    if (blinkResult.status == BlinkPatchResult::Status::Invalid)
    {
        return false;
    }

    bool applied = ApplyPatchToReticleGroup(reticle->group, patch);
    if (blinkResult.status == BlinkPatchResult::Status::Applied)
    {
        reticle->group.blink = blinkResult.blink;
        applied = true;
    }

    if (applied)
    {
        RefreshStickyStrobePosition(normalizedPageName);
    }

    return applied || IsEmptyPatch(patch);
}

bool SceneRegistry::SetDynamicReticleSetVisible(const std::string_view pageName,
                                                const std::string_view templateId,
                                                const bool visible) noexcept
{
    return SetDynamicReticleSetVisibleByKey(NormalizePageName(pageName), templateId, visible);
}

bool SceneRegistry::SetDynamicReticleSetVisibleByKey(const std::string_view normalizedPageName,
                                                     const std::string_view templateId,
                                                     const bool visible) noexcept
{
    const std::string normalizedTemplateId = NormalizePageName(templateId);
    if (!HasNormalizedPage(normalizedPageName) || normalizedTemplateId.empty() ||
        FindDynamicReticleLayerBinding(normalizedPageName, normalizedTemplateId) == nullptr)
    {
        return false;
    }

    const std::string key = MakeDynamicTemplateLookupKey(normalizedPageName, normalizedTemplateId);
    if (visible)
    {
        dynamicTemplateVisibility_.erase(key);
    }
    else
    {
        dynamicTemplateVisibility_[key] = false;
    }

    RefreshStickyStrobePosition(normalizedPageName);
    return true;
}

bool SceneRegistry::SetDynamicReticleSetStrobeMagnetEnabled(const std::string_view pageName,
                                                            const std::string_view templateId,
                                                            const bool enabled) noexcept
{
    return SetDynamicReticleSetStrobeMagnetEnabledByKey(NormalizePageName(pageName), templateId, enabled);
}

bool SceneRegistry::SetDynamicReticleSetStrobeMagnetEnabledByKey(const std::string_view normalizedPageName,
                                                                 const std::string_view templateId,
                                                                 const bool enabled) noexcept
{
    const std::string normalizedTemplateId = NormalizePageName(templateId);
    if (!HasNormalizedPage(normalizedPageName) || normalizedTemplateId.empty() ||
        FindDynamicReticleLayerBinding(normalizedPageName, normalizedTemplateId) == nullptr)
    {
        return false;
    }

    const std::string key = MakeDynamicTemplateLookupKey(normalizedPageName, normalizedTemplateId);
    if (enabled)
    {
        dynamicTemplateStrobeMagnetEnabled_[key] = true;
    }
    else
    {
        dynamicTemplateStrobeMagnetEnabled_.erase(key);
    }

    RefreshStickyStrobePosition(normalizedPageName);
    return true;
}

const SceneRegistry::ReticleComponent* SceneRegistry::FindLockedStrobeMagnetTarget(
    const std::string_view normalizedPageName,
    const std::string_view reticleId) const noexcept
{
    if (!IsActivePageKey(normalizedPageName))
    {
        return nullptr;
    }

    if (reticleId.empty())
    {
        return nullptr;
    }

    const PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return nullptr;
    }

    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    if (entity == entt::null || !registry_.all_of<DynamicTag>(entity))
    {
        return nullptr;
    }

    const auto* membership = registry_.try_get<PageMembership>(entity);
    const auto* reticle = registry_.try_get<ReticleComponent>(entity);
    if (membership == nullptr || reticle == nullptr || membership->pageName != normalizedPageName)
    {
        return nullptr;
    }

    const BlinkClock::time_point now = BlinkClock::now();
    if (!page->IsReticleVisibleNow(reticle->group, now) ||
        !IsDynamicTemplateVisible(normalizedPageName, reticle->group.sourceTemplateId) ||
        !IsDynamicTemplateStrobeMagnetEnabled(normalizedPageName, reticle->group.sourceTemplateId))
    {
        return nullptr;
    }

    return reticle;
}

const SceneRegistry::ReticleComponent* SceneRegistry::FindNearestStrobeMagnetTarget(
    const std::string_view normalizedPageName,
    const Vec2 position,
    const float radius) const noexcept
{
    if (!IsActivePageKey(normalizedPageName))
    {
        return nullptr;
    }

    const PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr || radius <= 0.0f)
    {
        return nullptr;
    }

    const float radiusSquared = radius * radius;
    float bestDistanceSquared = radiusSquared;
    const ReticleComponent* bestTarget = nullptr;
    const BlinkClock::time_point now = BlinkClock::now();

    auto dynamicView = registry_.view<ReticleComponent, PageMembership, DynamicTag>();
    for (const entt::entity entity : dynamicView)
    {
        const auto& membership = dynamicView.get<PageMembership>(entity);
        if (membership.pageName != normalizedPageName)
        {
            continue;
        }

        const auto& dynamicReticle = dynamicView.get<ReticleComponent>(entity);
        if (!page->IsReticleVisibleNow(dynamicReticle.group, now) ||
            !IsDynamicTemplateVisible(normalizedPageName, dynamicReticle.group.sourceTemplateId) ||
            !IsDynamicTemplateStrobeMagnetEnabled(normalizedPageName, dynamicReticle.group.sourceTemplateId))
        {
            continue;
        }

        const float distanceSquared = DistanceSquared(position, dynamicReticle.group.transform.position);
        if (!std::isfinite(distanceSquared) ||
            distanceSquared > radiusSquared ||
            distanceSquared >= bestDistanceSquared)
        {
            continue;
        }

        bestDistanceSquared = distanceSquared;
        bestTarget = &dynamicReticle;
    }

    return bestTarget;
}

void SceneRegistry::ApplyStrobeMagnetVisualShape(ReticleComponent& strobe,
                                                 StrobeBehaviorComponent& behavior,
                                                 const bool magnetized)
{
    const bool shouldApply = magnetized && behavior.magnet.enabled && behavior.magnet.visualShapeEnabled;
    if (shouldApply)
    {
        if (!behavior.visualShapeApplied)
        {
            behavior.authoredPrimitives = strobe.group.primitives;
            behavior.authoredOverrides = strobe.group.overrides;
            behavior.authoredClipping = strobe.group.clipping;
        }

        const PrimitiveStyle visualStyle = behavior.authoredPrimitives.empty()
                                               ? ResolveStrobeMagnetVisualStyle(strobe.group)
                                               : behavior.authoredPrimitives.front().style;
        strobe.group.primitives = MakeStrobeMagnetVisualPrimitives(behavior.magnet, visualStyle);
        strobe.group.overrides = behavior.authoredOverrides;
        strobe.group.clipping = {};
        behavior.visualShapeApplied = true;
        return;
    }

    if (behavior.visualShapeApplied)
    {
        strobe.group.primitives = behavior.authoredPrimitives;
        strobe.group.overrides = behavior.authoredOverrides;
        strobe.group.clipping = behavior.authoredClipping;
        behavior.visualShapeApplied = false;
    }
}

void SceneRegistry::RefreshStickyStrobePosition(const std::string_view normalizedPageName) noexcept
{
    if (!IsActivePageKey(normalizedPageName))
    {
        return;
    }

    const entt::entity strobeEntity = FindActiveStrobeEntity(normalizedPageName);
    if (strobeEntity == entt::null)
    {
        return;
    }

    auto* strobe = registry_.try_get<ReticleComponent>(strobeEntity);
    auto* behavior = registry_.try_get<StrobeBehaviorComponent>(strobeEntity);
    if (strobe == nullptr || behavior == nullptr || behavior->lockedReticleId.empty())
    {
        return;
    }

    const ReticleComponent* target = FindLockedStrobeMagnetTarget(normalizedPageName, behavior->lockedReticleId);
    if (target == nullptr)
    {
        behavior->lockedReticleId.clear();
        ApplyStrobeMagnetVisualShape(*strobe, *behavior, false);
        return;
    }

    strobe->group.transform.position = target->group.transform.position;
    ApplyStrobeMagnetVisualShape(*strobe, *behavior, true);
}

bool SceneRegistry::SetStrobeActive(const std::string_view pageName, const bool active) noexcept
{
    return SetStrobeActiveByKey(NormalizePageName(pageName), active);
}

bool SceneRegistry::SetStrobeActiveByKey(const std::string_view normalizedPageName, const bool active) noexcept
{
    const entt::entity strobeEntity = FindActiveStrobeEntity(normalizedPageName);
    if (strobeEntity == entt::null)
    {
        return false;
    }

    if (auto* reticle = registry_.try_get<ReticleComponent>(strobeEntity))
    {
        reticle->group.visible = active;
        return true;
    }

    return false;
}

bool SceneRegistry::SelectStrobe(const std::string_view pageName, const std::string_view strobeName) noexcept
{
    return SelectStrobeByKey(NormalizePageName(pageName), strobeName);
}

bool SceneRegistry::SelectStrobeByKey(const std::string_view normalizedPageName,
                                      const std::string_view strobeName) noexcept
{
    PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr || !page->hasStrobe)
    {
        return false;
    }

    const std::string normalizedStrobeName = NormalizePageName(strobeName);
    const entt::entity nextEntity = FindStrobeEntity(normalizedPageName, normalizedStrobeName);
    if (nextEntity == entt::null)
    {
        return false;
    }

    if (page->normalizedActiveStrobeName == normalizedStrobeName)
    {
        return true;
    }

    const entt::entity previousEntity = FindActiveStrobeEntity(normalizedPageName);
    if (previousEntity != entt::null)
    {
        RemoveReticleFromPageDrawList(normalizedPageName, previousEntity);
    }

    const auto* behavior = registry_.try_get<StrobeBehaviorComponent>(nextEntity);
    if (behavior == nullptr)
    {
        return false;
    }

    page->activeStrobeName = behavior->name;
    page->normalizedActiveStrobeName = behavior->normalizedName;
    InsertReticleIntoPageDrawList(normalizedPageName, nextEntity);
    RefreshStickyStrobePosition(normalizedPageName);
    return true;
}

bool SceneRegistry::SetStrobePosition(const std::string_view pageName, const Vec2 position) noexcept
{
    return SetStrobePositionByKey(NormalizePageName(pageName), position);
}

bool SceneRegistry::SetStrobePositionByKey(const std::string_view normalizedPageName, const Vec2 position) noexcept
{
    if (!IsValidVec2(position))
    {
        return false;
    }

    const entt::entity strobeEntity = FindActiveStrobeEntity(normalizedPageName);
    if (strobeEntity == entt::null)
    {
        return false;
    }

    auto* reticle = registry_.try_get<ReticleComponent>(strobeEntity);
    auto* behavior = registry_.try_get<StrobeBehaviorComponent>(strobeEntity);
    if (reticle != nullptr)
    {
        if (behavior != nullptr)
        {
            behavior->lockedReticleId.clear();
            ApplyStrobeMagnetVisualShape(*reticle, *behavior, false);

            if (behavior->magnet.enabled && behavior->magnet.radius > 0.0f)
            {
                if (const ReticleComponent* target =
                        FindNearestStrobeMagnetTarget(normalizedPageName, position, behavior->magnet.radius);
                    target != nullptr)
                {
                    behavior->lockedReticleId = target->group.id;
                    reticle->group.transform.position = target->group.transform.position;
                    ApplyStrobeMagnetVisualShape(*reticle, *behavior, true);
                    return true;
                }
            }
        }

        reticle->group.transform.position = position;
        if (behavior != nullptr)
        {
            ApplyStrobeMagnetVisualShape(*reticle, *behavior, false);
        }
        return true;
    }

    return false;
}

bool SceneRegistry::OffsetStrobe(const std::string_view pageName, const Vec2 delta) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    const entt::entity strobeEntity = FindActiveStrobeEntity(normalizedPageName);
    if (strobeEntity == entt::null)
    {
        return false;
    }

    if (const auto* reticle = registry_.try_get<ReticleComponent>(strobeEntity))
    {
        return SetStrobePosition(pageName, reticle->group.transform.position + delta);
    }

    return false;
}

bool SceneRegistry::ApplyStrobeUpdate(const std::string_view pageName,
                                      const std::string_view strobeName,
                                      const std::optional<bool> active,
                                      const std::optional<Vec2> position) noexcept
{
    return ApplyStrobeUpdateByKey(NormalizePageName(pageName), strobeName, active, position);
}

bool SceneRegistry::ApplyStrobeUpdateByKey(const std::string_view normalizedPageName,
                                           const std::string_view strobeName,
                                           const std::optional<bool> active,
                                           const std::optional<Vec2> position) noexcept
{
    const PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr || !page->hasStrobe)
    {
        return false;
    }

    // Validation phase: prove every requested change will succeed before mutating anything.
    // The strobe that activation/position act on is the selected variant when one is named,
    // otherwise the currently active strobe.
    entt::entity targetStrobe = entt::null;
    if (!strobeName.empty())
    {
        targetStrobe = FindStrobeEntity(normalizedPageName, NormalizePageName(strobeName));
        if (targetStrobe == entt::null || !registry_.all_of<StrobeBehaviorComponent>(targetStrobe))
        {
            return false;
        }
    }
    else
    {
        targetStrobe = FindActiveStrobeEntity(normalizedPageName);
    }

    if (position.has_value() && !IsValidVec2(*position))
    {
        return false;
    }

    if ((active.has_value() || position.has_value()) &&
        (targetStrobe == entt::null || !registry_.all_of<ReticleComponent>(targetStrobe)))
    {
        return false;
    }

    // Mutation phase: preconditions hold, so none of these can fail and leave a partial state.
    if (!strobeName.empty() && !SelectStrobeByKey(normalizedPageName, strobeName))
    {
        return false;
    }

    if (active.has_value() && !SetStrobeActiveByKey(normalizedPageName, *active))
    {
        return false;
    }

    if (position.has_value() && !SetStrobePositionByKey(normalizedPageName, *position))
    {
        return false;
    }

    return true;
}

std::optional<StrobeCaptureResult> SceneRegistry::CaptureWithStrobe(const std::string_view pageName) const
{
    return CaptureWithStrobeKey(NormalizePageName(pageName));
}

std::optional<StrobeCaptureResult> SceneRegistry::CaptureActivePageStrobe() const
{
    return CaptureWithStrobeKey(activePage_);
}

std::optional<StrobeCaptureResult> SceneRegistry::CaptureWithStrobeKey(const std::string_view pageName) const
{
    return ScanStrobeByKey(pageName).capture;
}

StrobeScanResult SceneRegistry::ScanStrobeForPage(const std::string_view pageName) const
{
    return ScanStrobeByKey(NormalizePageName(pageName));
}

StrobeScanResult SceneRegistry::ScanStrobeByKey(const std::string_view pageName) const
{
    StrobeScanResult result;

    if (!IsActivePageKey(pageName))
    {
        return result;
    }

    const entt::entity strobeEntity = FindActiveStrobeEntity(pageName);
    if (strobeEntity == entt::null)
    {
        return result;
    }

    const auto* strobe = registry_.try_get<ReticleComponent>(strobeEntity);
    const auto* behavior = registry_.try_get<StrobeBehaviorComponent>(strobeEntity);
    const PageComponent* page = FindPage(pageName);
    if (strobe == nullptr || behavior == nullptr || page == nullptr)
    {
        return result;
    }

    StrobeMagnetSummary magnetSummary;
    magnetSummary.enabled = behavior->magnet.enabled;
    magnetSummary.radius = behavior->magnet.radius;
    magnetSummary.strength = behavior->magnet.strength;
    result.magnet = magnetSummary;

    const BlinkClock::time_point now = BlinkClock::now();
    if (!page->IsReticleVisibleNow(strobe->group, now))
    {
        // A hidden strobe is neither magnetized nor able to capture: leave the magnet summary
        // in its non-magnetized state and the capture result empty.
        return result;
    }

    // The locked magnet target, when still valid, both freezes magnetization and shifts the
    // capture reference position. Resolve it once and share it across magnet and capture.
    const ReticleComponent* lockedTarget = FindLockedStrobeMagnetTarget(pageName, behavior->lockedReticleId);

    Vec2 strobePosition = strobe->group.transform.position;
    if (lockedTarget != nullptr)
    {
        strobePosition = lockedTarget->group.transform.position;
    }

    const bool magnetActive = behavior->magnet.enabled && behavior->magnet.radius > 0.0f;
    if (magnetActive && lockedTarget != nullptr)
    {
        result.magnet->magnetized = true;
        result.magnet->runtimeReticleId = lockedTarget->runtimeReticleId;
        result.magnet->reticleId = lockedTarget->group.id;
        result.magnet->targetPosition = lockedTarget->group.transform.position;
        result.magnet->distance = 0.0f;
    }

    // Real early-out: skip the O(N) dynamic-reticle pass when neither a magnet search (only
    // needed when magnetization is active and no target is locked yet) nor a capture test can
    // ever match. A locked strobe still scans for capture because the lock only fixes the
    // magnet target, not the capture outcome.
    const bool runMagnetScan = magnetActive && lockedTarget == nullptr;
    const bool runCaptureScan = CaptureAreaCanMatch(behavior->capture);
    if (!runMagnetScan && !runCaptureScan)
    {
        return result;
    }

    const float magnetRadiusSquared = behavior->magnet.radius * behavior->magnet.radius;
    float bestMagnetDistanceSquared = magnetRadiusSquared;
    const ReticleComponent* bestMagnetTarget = nullptr;
    float bestCaptureDistanceSquared = std::numeric_limits<float>::max();

    auto dynamicView = registry_.view<ReticleComponent, PageMembership, DynamicTag>();
    for (const entt::entity entity : dynamicView)
    {
        const auto& membership = dynamicView.get<PageMembership>(entity);
        if (membership.pageName != pageName)
        {
            continue;
        }

        const auto& reticleComponent = dynamicView.get<ReticleComponent>(entity);
        const auto& reticle = reticleComponent.group;
        if (!page->IsReticleVisibleNow(reticle, now) ||
            !IsDynamicTemplateVisibleByNormalizedId(pageName, reticleComponent.normalizedSourceTemplateId))
        {
            continue;
        }

        const Vec2 candidatePosition = reticle.transform.position;
        const float distanceSquared = DistanceSquared(strobePosition, candidatePosition);

        if (runCaptureScan &&
            distanceSquared < bestCaptureDistanceSquared &&
            IsInsideCaptureArea(strobePosition, candidatePosition, behavior->capture))
        {
            bestCaptureDistanceSquared = distanceSquared;
            result.capture = StrobeCaptureResult {
                page->name,
                page->transportId,
                strobe->group.id,
                reticleComponent.runtimeReticleId,
                reticle.id,
                reticleComponent.sourceTemplateTransportId,
                reticle.sourceTemplateId,
                reticle.info.label,
                reticle.info.category,
                candidatePosition,
                std::sqrt(distanceSquared),
                reticle.info.metadata};
        }

        if (runMagnetScan &&
            std::isfinite(distanceSquared) &&
            distanceSquared <= magnetRadiusSquared &&
            distanceSquared < bestMagnetDistanceSquared &&
            IsDynamicTemplateStrobeMagnetEnabledByNormalizedId(pageName, reticleComponent.normalizedSourceTemplateId))
        {
            bestMagnetDistanceSquared = distanceSquared;
            bestMagnetTarget = &reticleComponent;
        }
    }

    if (bestMagnetTarget != nullptr)
    {
        result.magnet->magnetized = true;
        result.magnet->runtimeReticleId = bestMagnetTarget->runtimeReticleId;
        result.magnet->reticleId = bestMagnetTarget->group.id;
        result.magnet->targetPosition = bestMagnetTarget->group.transform.position;
        result.magnet->distance = std::sqrt(bestMagnetDistanceSquared);
    }

    return result;
}

void SceneRegistry::UpsertDynamicReticle(const std::string_view pageName, ReticleGroup reticle)
{
    UpsertDynamicReticleByKey(NormalizePageName(pageName), std::move(reticle));
}

void SceneRegistry::UpsertDynamicReticleByKey(const std::string_view normalizedPageName, ReticleGroup reticle)
{
    PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr || NormalizeReticleId(reticle.id).empty())
    {
        return;
    }

    const DynamicReticleLayerBinding* binding = page->ResolveDynamicBinding(reticle.sourceTemplateId);
    if (binding == nullptr)
    {
        return;
    }

    reticle.layerId = binding->layerId;
    page->ResolveBlinkForReticle(reticle);

    const entt::entity existingEntity = FindReticleEntity(normalizedPageName, reticle.id);
    if (existingEntity != entt::null)
    {
        if (registry_.all_of<DynamicTag>(existingEntity))
        {
            if (auto* component = registry_.try_get<ReticleComponent>(existingEntity))
            {
                component->drawOrder = page->BuildDynamicReticleDrawOrderKey(reticle, component->dynamicCreationSequence);
                component->cachedLayerOrder = component->drawOrder.layerOrder;
                component->normalizedSourceTemplateId = NormalizePageName(reticle.sourceTemplateId);
                component->group = std::move(reticle);
                RemoveReticleFromPageDrawList(normalizedPageName, existingEntity);
                InsertReticleIntoPageDrawList(normalizedPageName, existingEntity);
                RefreshStickyStrobePosition(normalizedPageName);
            }
        }
        return;
    }

    const entt::entity entity = registry_.create();
    const std::uint64_t creationSequence = nextDynamicCreationSequence_++;
    const ReticleDrawOrderKey drawOrder = page->BuildDynamicReticleDrawOrderKey(reticle, creationSequence);
    std::string normalizedSourceTemplateId = NormalizePageName(reticle.sourceTemplateId);
    ReticleComponent component {std::move(reticle), drawOrder, 0, 0, creationSequence};
    component.cachedLayerOrder = drawOrder.layerOrder;
    component.normalizedSourceTemplateId = std::move(normalizedSourceTemplateId);
    registry_.emplace<ReticleComponent>(entity, std::move(component));
    registry_.emplace<PageMembership>(entity, PageMembership {std::string(normalizedPageName)});
    registry_.emplace<DynamicTag>(entity);
    IndexReticle(normalizedPageName, registry_.get<ReticleComponent>(entity).group, entity);
    InsertReticleIntoPageDrawList(normalizedPageName, entity);
    RefreshStickyStrobePosition(normalizedPageName);
}

bool SceneRegistry::RemoveDynamicReticle(const std::string_view pageName, const std::string_view reticleId)
{
    return RemoveDynamicReticleByKey(NormalizePageName(pageName), reticleId);
}

bool SceneRegistry::RemoveDynamicReticleByKey(const std::string_view normalizedPageName,
                                              const std::string_view reticleId)
{
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    if (entity == entt::null || !registry_.all_of<DynamicTag>(entity))
    {
        return false;
    }

    RemoveReticleIndex(normalizedPageName, reticleId);
    RemoveReticleFromPageDrawList(normalizedPageName, entity);
    registry_.destroy(entity);
    RefreshStickyStrobePosition(normalizedPageName);
    return true;
}

void SceneRegistry::ClearDynamicReticles(const std::string_view pageName)
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    std::vector<entt::entity> entitiesToDestroy;
    auto dynamicView = registry_.view<PageMembership, DynamicTag>();

    for (const entt::entity entity : dynamicView)
    {
        const auto& membership = dynamicView.get<PageMembership>(entity);
        if (membership.pageName == normalizedPageName)
        {
            entitiesToDestroy.push_back(entity);
        }
    }

    for (const entt::entity entity : entitiesToDestroy)
    {
        const auto& membership = registry_.get<PageMembership>(entity);
        const auto& reticle = registry_.get<ReticleComponent>(entity).group;
        RemoveReticleIndex(membership.pageName, reticle.id);
        RemoveReticleFromPageDrawList(membership.pageName, entity);
        registry_.destroy(entity);
        RefreshStickyStrobePosition(membership.pageName);
    }
}

void SceneRegistry::ClearAllDynamicReticles()
{
    std::vector<entt::entity> entitiesToDestroy;
    auto dynamicView = registry_.view<DynamicTag>();

    for (const entt::entity entity : dynamicView)
    {
        entitiesToDestroy.push_back(entity);
    }

    for (const entt::entity entity : entitiesToDestroy)
    {
        const auto& membership = registry_.get<PageMembership>(entity);
        const auto& reticle = registry_.get<ReticleComponent>(entity).group;
        RemoveReticleIndex(membership.pageName, reticle.id);
        RemoveReticleFromPageDrawList(membership.pageName, entity);
        registry_.destroy(entity);
        RefreshStickyStrobePosition(membership.pageName);
    }
}

void SceneRegistry::ResetToInitialState()
{
    LoadDocument(document_, transportMap_);
}

entt::entity SceneRegistry::FindReticleEntity(const std::string_view normalizedPageName,
                                              const std::string_view reticleId) const noexcept
{
    const auto iterator = reticleEntities_.find(MakeReticleLookupKey(normalizedPageName, reticleId));
    return iterator == reticleEntities_.end() ? entt::null : iterator->second;
}

SceneRegistry::ReticleComponent* SceneRegistry::FindReticle(const std::string_view normalizedPageName,
                                                            const std::string_view reticleId) noexcept
{
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    return entity == entt::null ? nullptr : registry_.try_get<ReticleComponent>(entity);
}

const SceneRegistry::ReticleComponent* SceneRegistry::FindReticle(const std::string_view normalizedPageName,
                                                                  const std::string_view reticleId) const noexcept
{
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    return entity == entt::null ? nullptr : registry_.try_get<ReticleComponent>(entity);
}

bool SceneRegistry::SetDynamicReticleRuntimeIdentifiers(const std::string_view normalizedPageName,
                                                        const std::string_view reticleId,
                                                        const RuntimeDynamicId runtimeReticleId,
                                                        const TransportId sourceTemplateTransportId) noexcept
{
    ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId);
    if (reticle == nullptr)
    {
        return false;
    }

    reticle->runtimeReticleId = runtimeReticleId;
    if (sourceTemplateTransportId != 0)
    {
        reticle->sourceTemplateTransportId = sourceTemplateTransportId;
    }
    else if (reticle->sourceTemplateTransportId == 0 && !reticle->group.sourceTemplateId.empty())
    {
        reticle->sourceTemplateTransportId = ResolveTemplateTransportId(reticle->group.sourceTemplateId);
    }

    return true;
}

SceneRegistry::PageComponent* SceneRegistry::FindPage(const std::string_view normalizedPageName) noexcept
{
    const auto iterator = pageEntities_.find(std::string(normalizedPageName));
    return iterator == pageEntities_.end() ? nullptr : registry_.try_get<PageComponent>(iterator->second);
}

const SceneRegistry::PageComponent* SceneRegistry::FindPage(const std::string_view normalizedPageName) const noexcept
{
    const auto iterator = pageEntities_.find(std::string(normalizedPageName));
    return iterator == pageEntities_.end() ? nullptr : registry_.try_get<PageComponent>(iterator->second);
}

std::string SceneRegistry::MakeStrobeLookupKey(const std::string_view normalizedPageName,
                                               const std::string_view strobeName) const
{
    std::string key;
    key.reserve(normalizedPageName.size() + strobeName.size() + 1U);
    key.append(normalizedPageName);
    key.push_back('\x1F');
    key.append(NormalizePageName(strobeName));
    return key;
}

entt::entity SceneRegistry::FindStrobeEntity(const std::string_view normalizedPageName,
                                             const std::string_view strobeName) const noexcept
{
    const auto iterator = strobeEntities_.find(MakeStrobeLookupKey(normalizedPageName, strobeName));
    return iterator == strobeEntities_.end() ? entt::null : iterator->second;
}

entt::entity SceneRegistry::FindActiveStrobeEntity(const std::string_view normalizedPageName) const noexcept
{
    const PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr || page->normalizedActiveStrobeName.empty())
    {
        return entt::null;
    }

    return FindStrobeEntity(normalizedPageName, page->normalizedActiveStrobeName);
}

std::string SceneRegistry::MakeReticleLookupKey(const std::string_view normalizedPageName,
                                                const std::string_view reticleId) const
{
    std::string key;
    key.reserve(normalizedPageName.size() + reticleId.size() + 1U);
    key.append(normalizedPageName);
    key.push_back(':');
    key.append(NormalizeReticleId(reticleId));
    return key;
}

void SceneRegistry::IndexReticle(const std::string_view normalizedPageName,
                                 const ReticleGroup& reticle,
                                 const entt::entity entity)
{
    reticleEntities_.insert_or_assign(MakeReticleLookupKey(normalizedPageName, reticle.id), entity);
}

void SceneRegistry::RemoveReticleIndex(const std::string_view normalizedPageName, const std::string_view reticleId)
{
    reticleEntities_.erase(MakeReticleLookupKey(normalizedPageName, reticleId));
}

std::string SceneRegistry::MakeDynamicTemplateLookupKey(const std::string_view normalizedPageName,
                                                        const std::string_view templateId) const
{
    return std::string(normalizedPageName) + '\x1F' + std::string(templateId);
}

bool SceneRegistry::IsDynamicTemplateVisible(const std::string_view normalizedPageName,
                                             const std::string_view templateId) const noexcept
{
    return IsDynamicTemplateVisibleByNormalizedId(normalizedPageName, NormalizePageName(templateId));
}

bool SceneRegistry::IsDynamicTemplateVisibleByNormalizedId(const std::string_view normalizedPageName,
                                                           const std::string_view normalizedTemplateId) const noexcept
{
    if (normalizedTemplateId.empty())
    {
        return true;
    }

    const auto iterator = dynamicTemplateVisibility_.find(
        MakeDynamicTemplateLookupKey(normalizedPageName, normalizedTemplateId));
    if (iterator == dynamicTemplateVisibility_.end())
    {
        return true;
    }

    return iterator->second;
}

bool SceneRegistry::IsDynamicTemplateStrobeMagnetEnabled(const std::string_view normalizedPageName,
                                                         const std::string_view templateId) const noexcept
{
    return IsDynamicTemplateStrobeMagnetEnabledByNormalizedId(normalizedPageName, NormalizePageName(templateId));
}

bool SceneRegistry::IsDynamicTemplateStrobeMagnetEnabledByNormalizedId(
    const std::string_view normalizedPageName, const std::string_view normalizedTemplateId) const noexcept
{
    if (normalizedTemplateId.empty())
    {
        return false;
    }

    const auto iterator = dynamicTemplateStrobeMagnetEnabled_.find(
        MakeDynamicTemplateLookupKey(normalizedPageName, normalizedTemplateId));
    if (iterator == dynamicTemplateStrobeMagnetEnabled_.end())
    {
        return false;
    }

    return iterator->second;
}

const DynamicReticleLayerBinding* SceneRegistry::FindDynamicReticleLayerBinding(
    const std::string_view normalizedPageName,
    const std::string_view templateId) const noexcept
{
    const PageComponent* page = FindPage(normalizedPageName);
    return page == nullptr ? nullptr : page->ResolveDynamicBinding(templateId);
}

void SceneRegistry::InsertReticleIntoPageDrawList(const std::string_view normalizedPageName, const entt::entity entity)
{
    PageComponent* page = FindPage(normalizedPageName);
    const ReticleComponent* reticle = registry_.try_get<ReticleComponent>(entity);
    if (page == nullptr || reticle == nullptr)
    {
        return;
    }

    auto& drawList = page->orderedReticleEntities;
    if (std::find(drawList.begin(), drawList.end(), entity) != drawList.end())
    {
        return;
    }

    const auto insertionPoint = std::lower_bound(
        drawList.begin(),
        drawList.end(),
        reticle->drawOrder,
        [this](const entt::entity candidate, const ReticleDrawOrderKey& drawOrder)
        {
            const ReticleComponent* candidateReticle = registry_.try_get<ReticleComponent>(candidate);
            return candidateReticle != nullptr && candidateReticle->drawOrder < drawOrder;
        });
    drawList.insert(insertionPoint, entity);
}

void SceneRegistry::RemoveReticleFromPageDrawList(const std::string_view normalizedPageName, const entt::entity entity)
{
    PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return;
    }

    auto& drawList = page->orderedReticleEntities;
    drawList.erase(std::remove(drawList.begin(), drawList.end(), entity), drawList.end());
}

void SceneRegistry::SetActiveFlag(const std::string_view pageName, const bool active)
{
    const auto iterator = pageEntities_.find(std::string(pageName));
    if (iterator == pageEntities_.end())
    {
        return;
    }

    if (auto* page = registry_.try_get<PageComponent>(iterator->second))
    {
        page->active = active;
    }
}
} // namespace mfd
