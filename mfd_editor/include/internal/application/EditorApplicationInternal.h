/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Shared private helpers reused by responsibility-oriented `EditorApplication` implementation units.
 */

#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "mfd/io/JsonLoader.h"
#include "mfd/model/PageDefinition.h"

namespace editor::detail
{
/**
 * @brief Copies one UTF-8 string into one fixed-size UI buffer.
 * @tparam N Buffer capacity including the null terminator.
 * @param destination Buffer receiving the copied text.
 * @param value UTF-8 string view to copy.
 */
template <std::size_t N>
inline void CopyTextBuffer(std::array<char, N>& destination, const std::string_view value)
{
    std::snprintf(destination.data(), destination.size(), "%.*s", static_cast<int>(value.size()), value.data());
}

/**
 * @brief Clamps the high-frequency feedback cadence to one strictly positive value.
 * @param seconds Requested interval in seconds.
 * @return Interval clamped to the editor-supported minimum.
 */
inline float ClampFeedbackFastIntervalSeconds(const float seconds) noexcept
{
    return std::max(0.001f, seconds);
}

/**
 * @brief Keeps the heartbeat cadence greater than or equal to the fast cadence.
 * @param heartbeatSeconds Requested heartbeat interval.
 * @param fastSeconds Current fast feedback interval.
 * @return Heartbeat interval clamped to one valid value.
 */
inline float ClampFeedbackHeartbeatIntervalSeconds(const float heartbeatSeconds, const float fastSeconds) noexcept
{
    return std::max(ClampFeedbackFastIntervalSeconds(fastSeconds), heartbeatSeconds);
}

/**
 * @brief Converts one normalized UI color channel to one authored byte channel.
 * @param value Normalized channel in the inclusive `[0, 1]` range.
 * @return Clamped 8-bit color channel.
 */
inline std::uint8_t ToColorChannelByte(const float value) noexcept
{
    return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

/**
 * @brief Converts one ImGui color to the authored runtime RGBA representation.
 * @param color Normalized editor color.
 * @return Authored runtime color.
 */
inline mfd::ColorRgba ToColorRgba(const ImVec4& color)
{
    return mfd::ColorRgba {
        ToColorChannelByte(color.x),
        ToColorChannelByte(color.y),
        ToColorChannelByte(color.z),
        ToColorChannelByte(color.w)};
}

/**
 * @brief Returns whether one authored color can produce one visible fill.
 * @param color Authored runtime color to inspect.
 * @return `true` when the alpha channel is non-zero.
 */
inline bool HasVisibleFillAlpha(const mfd::ColorRgba& color) noexcept
{
    return color.a != 0U;
}

/**
 * @brief Returns one visible fill color derived from one stroke color.
 * @param stroke Stroke color used as the first fallback.
 * @return Stroke color when it is already visible, or the default primitive stroke otherwise.
 */
inline mfd::ColorRgba VisibleFillColorFromStroke(const mfd::ColorRgba& stroke) noexcept
{
    if (HasVisibleFillAlpha(stroke))
    {
        return stroke;
    }

    return mfd::PrimitiveStyle {}.color;
}

/**
 * @brief Seeds one primitive fill color the first time filled rendering becomes visible.
 * @param style Primitive style updated in place.
 */
inline void SeedPrimitiveFillColorIfNeeded(mfd::PrimitiveStyle& style) noexcept
{
    if (!style.filled || HasVisibleFillAlpha(style.fillColor))
    {
        return;
    }

    style.fillColor = VisibleFillColorFromStroke(style.color);
}

/**
 * @brief Seeds one reticle-level fill override when filled rendering is enabled without one visible color.
 * @param overrides Reticle style overrides updated in place.
 * @param fallbackStroke Stroke color used to derive one visible fill.
 */
inline void SeedReticleFillOverrideIfNeeded(mfd::ReticleStyleOverride& overrides,
                                            const mfd::ColorRgba& fallbackStroke) noexcept
{
    if (!overrides.filled.value_or(false))
    {
        return;
    }

    if (overrides.fillColor.has_value() && HasVisibleFillAlpha(*overrides.fillColor))
    {
        return;
    }

    overrides.fillColor = VisibleFillColorFromStroke(fallbackStroke);
}

/**
 * @brief Returns whether one reticle contains at least one fill-capable primitive.
 * @param reticle Reticle template or instance to inspect.
 * @return `true` when at least one primitive supports filled rendering.
 */
inline bool ReticleHasFillCapablePrimitive(const mfd::ReticleGroup& reticle) noexcept
{
    return std::any_of(reticle.primitives.begin(),
                       reticle.primitives.end(),
                       [](const mfd::Primitive& primitive)
                       {
                           return mfd::SupportsFilledPrimitive(primitive);
                       });
}

/**
 * @brief Returns whether one reticle-library id already exists after normalization.
 * @param library Reticle library to inspect.
 * @param id Candidate authored template id.
 * @return `true` when the candidate collides with an existing key or reticle id after normalization.
 */
inline bool ReticleLibraryIdExistsNormalized(const mfd::ReticleLibrary& library, const std::string_view id) noexcept
{
    const std::string normalizedId = mfd::NormalizePageName(id);
    if (normalizedId.empty())
    {
        return false;
    }

    for (const auto& entry : library)
    {
        if (mfd::NormalizePageName(entry.first) == normalizedId ||
            mfd::NormalizePageName(entry.second.id) == normalizedId)
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief Generates one library reticle id that stays unique after normalization.
 * @param library Reticle library that must stay collision free.
 * @param baseId Preferred authored template id.
 * @return One unique authored template id, falling back to `reticle` when the preferred id normalizes to empty.
 */
inline std::string MakeUniqueLibraryReticleId(const mfd::ReticleLibrary& library, const std::string_view baseId)
{
    const std::string stableBase =
        mfd::NormalizePageName(baseId).empty() ? std::string {"reticle"} : std::string(baseId);
    std::string candidate = stableBase;
    int suffix = 1;

    while (ReticleLibraryIdExistsNormalized(library, candidate))
    {
        candidate = stableBase + "_" + std::to_string(suffix++);
    }

    return candidate;
}

/**
 * @brief Returns whether one page already owns a static reticle id after normalization.
 * @param page Page whose authored static reticles must stay collision free.
 * @param id Candidate page-reticle id.
 * @param excludedReticleId Optional current reticle id excluded while editing one existing instance.
 * @return `true` when another static reticle already uses the same normalized id.
 */
inline bool PageReticleIdExistsNormalized(const mfd::PageDefinition& page,
                                          const std::string_view id,
                                          const std::string_view excludedReticleId = {}) noexcept
{
    const std::string normalizedId = mfd::NormalizePageName(id);
    if (normalizedId.empty())
    {
        return false;
    }

    const std::string excludedNormalizedId = mfd::NormalizePageName(excludedReticleId);
    return std::any_of(page.staticReticles.begin(),
                       page.staticReticles.end(),
                       [&normalizedId, &excludedNormalizedId](const mfd::ReticleGroup& reticle)
                       {
                           const std::string normalizedReticleId = mfd::NormalizePageName(reticle.id);
                           if (normalizedReticleId.empty())
                           {
                               return false;
                           }

                           if (!excludedNormalizedId.empty() && normalizedReticleId == excludedNormalizedId)
                           {
                               return false;
                           }

                           return normalizedReticleId == normalizedId;
                       });
}

/**
 * @brief Returns whether one page strobe already reserves the provided reticle id after normalization.
 * @param page Page whose authored strobes must stay collision free.
 * @param id Candidate page-reticle id.
 * @param excludedStrobeReticleId Optional strobe reticle id excluded while replacing one existing strobe.
 * @return `true` when another strobe reticle already uses the same normalized id.
 */
inline bool PageStrobeReticleIdExistsNormalized(const mfd::PageDefinition& page,
                                                const std::string_view id,
                                                const std::string_view excludedStrobeReticleId = {}) noexcept
{
    const std::string normalizedId = mfd::NormalizePageName(id);
    if (normalizedId.empty())
    {
        return false;
    }

    const std::string excludedNormalizedId = mfd::NormalizePageName(excludedStrobeReticleId);
    return std::any_of(page.strobes.begin(),
                       page.strobes.end(),
                       [&normalizedId, &excludedNormalizedId](const mfd::PageStrobeDefinition& strobe)
                       {
                           const std::string normalizedStrobeId = mfd::NormalizePageName(strobe.reticle.id);
                           if (normalizedStrobeId.empty())
                           {
                               return false;
                           }

                           if (!excludedNormalizedId.empty() && normalizedStrobeId == excludedNormalizedId)
                           {
                               return false;
                           }

                           return normalizedStrobeId == normalizedId;
                       });
}

/**
 * @brief Generates one page-reticle id that stays unique against static reticles and strobe reticles.
 * @param page Page that will own the authored reticle instance.
 * @param baseId Preferred page-reticle id.
 * @param excludedReticleId Optional current reticle id excluded while renaming one existing instance.
 * @param excludedStrobeReticleId Optional strobe reticle id excluded while replacing one existing strobe.
 * @return Unique authored page-reticle id, falling back to `reticle` when the preferred id normalizes to empty.
 */
inline std::string MakeUniquePageReticleInstanceId(const mfd::PageDefinition& page,
                                                   const std::string_view baseId,
                                                   const std::string_view excludedReticleId = {},
                                                   const std::string_view excludedStrobeReticleId = {})
{
    const std::string stableBase =
        mfd::NormalizePageName(baseId).empty() ? std::string {"reticle"} : std::string(baseId);
    std::string candidate = stableBase;
    int suffix = 1;

    while (PageReticleIdExistsNormalized(page, candidate, excludedReticleId) ||
           PageStrobeReticleIdExistsNormalized(page, candidate, excludedStrobeReticleId))
    {
        candidate = stableBase + "_" + std::to_string(suffix++);
    }

    return candidate;
}

/**
 * @brief Returns the half width used by editor preview bounds for one authored text string.
 * @param text Authored text content, or the fallback formatting string when empty.
 * @param fontSize Authored logical font size.
 * @param letterSpacing Authored logical glyph spacing.
 * @return Conservative text half width used by editor hit-testing bounds.
 */
inline float EstimatePrimitiveTextHalfWidth(const std::string_view text,
                                            const float fontSize,
                                            const float letterSpacing) noexcept
{
    const std::size_t characterCount = std::max<std::size_t>(1, text.size());
    const float glyphWidth = fontSize * static_cast<float>(characterCount) * 0.30f;
    const float spacingWidth = std::max(0.0f, letterSpacing) *
                               static_cast<float>(characterCount > 0 ? characterCount - 1U : 0U) * 0.5f;
    return std::max(0.03f, glyphWidth + spacingWidth);
}

/**
 * @brief Returns the half width used by editor preview bounds for one text primitive.
 * @param text Authored text geometry.
 * @return Conservative text half width used by editor hit-testing bounds.
 */
inline float EstimatePrimitiveTextHalfWidth(const mfd::TextGeometry& text) noexcept
{
    return EstimatePrimitiveTextHalfWidth(text.text, text.fontSize, text.letterSpacing);
}

/**
 * @brief Returns the half width used by editor preview bounds for one time primitive.
 * @param time Authored time geometry.
 * @return Conservative time half width used by editor hit-testing bounds.
 */
inline float EstimatePrimitiveTextHalfWidth(const mfd::TimeGeometry& time) noexcept
{
    return EstimatePrimitiveTextHalfWidth(time.format.empty() ? std::string_view {"%H:%M:%S"} : std::string_view {time.format},
                                          time.fontSize,
                                          time.letterSpacing);
}

/**
 * @brief Returns the half height used by editor preview bounds for one font size.
 * @param fontSize Authored logical font size.
 * @return Conservative text half height used by editor hit-testing bounds.
 */
inline float EstimatePrimitiveTextHalfHeight(const float fontSize) noexcept
{
    return std::max(0.02f, fontSize * 0.50f);
}

/**
 * @brief Returns the half height used by editor preview bounds for one text primitive.
 * @param text Authored text geometry.
 * @return Conservative text half height used by editor hit-testing bounds.
 */
inline float EstimatePrimitiveTextHalfHeight(const mfd::TextGeometry& text) noexcept
{
    return EstimatePrimitiveTextHalfHeight(text.fontSize);
}

/**
 * @brief Returns the half height used by editor preview bounds for one time primitive.
 * @param time Authored time geometry.
 * @return Conservative time half height used by editor hit-testing bounds.
 */
inline float EstimatePrimitiveTextHalfHeight(const mfd::TimeGeometry& time) noexcept
{
    return EstimatePrimitiveTextHalfHeight(time.fontSize);
}

/**
 * @brief Returns the full logical preview width used by one text-like primitive.
 * @param halfWidth Conservative text half width.
 * @return Full logical preview width.
 */
inline float EstimatedPrimitiveTextWidth(const float halfWidth) noexcept
{
    return halfWidth * 2.0f;
}

/**
 * @brief Returns the horizontal origin inside one text preview box for the requested alignment.
 * @param width Conservative logical text width.
 * @param align Horizontal text alignment.
 * @return Local origin offset measured from the left preview edge.
 */
inline float PrimitiveTextOriginX(const float width, const mfd::Align align) noexcept
{
    switch (align)
    {
    case mfd::Align::Left:
        return 0.0f;
    case mfd::Align::Right:
        return width;
    case mfd::Align::Center:
    default:
        return width * 0.5f;
    }
}

/**
 * @brief Visits one conservative text preview rectangle for the requested alignment.
 * @tparam TCallback Callable receiving one geometry-local corner at a time.
 * @param width Conservative logical text width.
 * @param halfHeight Conservative logical text half height.
 * @param align Horizontal text alignment.
 * @param callback Visitor receiving the four preview corners.
 */
template <typename TCallback>
inline void ForEachAlignedTextBoundsLocalPoint(const float width,
                                               const float halfHeight,
                                               const mfd::Align align,
                                               TCallback&& callback)
{
    const float originX = PrimitiveTextOriginX(width, align);
    const float left = -originX;
    const float right = width - originX;

    callback(mfd::Vec2 {left, -halfHeight});
    callback(mfd::Vec2 {right, -halfHeight});
    callback(mfd::Vec2 {right, halfHeight});
    callback(mfd::Vec2 {left, halfHeight});
}

/**
 * @brief Visits the logical arc samples used to bound one preview arc.
 * @tparam TCallback Callable receiving one logical point per sample.
 * @param radius Authored arc radius.
 * @param startAngleDegrees Authored arc start angle in degrees.
 * @param endAngleDegrees Authored arc end angle in degrees.
 * @param segments Requested authored segment count.
 * @param callback Visitor receiving each generated point.
 */
template <typename TCallback>
inline void ForEachPrimitiveArcPoint(const float radius,
                                     const float startAngleDegrees,
                                     const float endAngleDegrees,
                                     const int segments,
                                     TCallback&& callback)
{
    constexpr float kPi = 3.14159265358979323846f;

    const int segmentCount = std::max(2, segments);
    const float safeRadius = std::max(0.0f, std::abs(radius));
    const float startRadians = startAngleDegrees * kPi / 180.0f;
    const float sweepRadians = (endAngleDegrees - startAngleDegrees) * kPi / 180.0f;

    for (int index = 0; index <= segmentCount; ++index)
    {
        const float factor = static_cast<float>(index) / static_cast<float>(segmentCount);
        const float angle = startRadians + sweepRadians * factor;
        callback(mfd::Vec2 {std::cos(angle) * safeRadius, std::sin(angle) * safeRadius});
    }
}

/**
 * @brief Visits the geometry-local points that define one primitive bounding hull.
 * @tparam TCallback Callable receiving one geometry-local point at a time.
 * @param primitive Primitive whose visible authored hull must be enumerated.
 * @param callback Visitor receiving each geometry-local point.
 * @note The primitive transform is intentionally not applied here so callers can compose
 *       primitive, reticle, world, or screen-space bounds without duplicating the geometry switch.
 */
template <typename TCallback>
inline void ForEachPrimitiveBoundsLocalPoint(const mfd::Primitive& primitive, TCallback&& callback)
{
    if (!primitive.style.visible)
    {
        return;
    }

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatePrimitiveTextHalfWidth(*text);
        const float halfHeight = EstimatePrimitiveTextHalfHeight(*text);
        ForEachAlignedTextBoundsLocalPoint(
            EstimatedPrimitiveTextWidth(halfWidth),
            halfHeight,
            text->align,
            callback);
    }
    else if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatePrimitiveTextHalfWidth(*time);
        const float halfHeight = EstimatePrimitiveTextHalfHeight(*time);
        ForEachAlignedTextBoundsLocalPoint(
            EstimatedPrimitiveTextWidth(halfWidth),
            halfHeight,
            time->align,
            callback);
    }
    else if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        callback(line->start);
        callback(line->end);
    }
    else if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        callback(mfd::Vec2 {-circle->radius, -circle->radius});
        callback(mfd::Vec2 {circle->radius, -circle->radius});
        callback(mfd::Vec2 {circle->radius, circle->radius});
        callback(mfd::Vec2 {-circle->radius, circle->radius});
    }
    else if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        callback(mfd::Vec2 {-ring->outerRadius, -ring->outerRadius});
        callback(mfd::Vec2 {ring->outerRadius, -ring->outerRadius});
        callback(mfd::Vec2 {ring->outerRadius, ring->outerRadius});
        callback(mfd::Vec2 {-ring->outerRadius, ring->outerRadius});
    }
    else if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        callback(mfd::Vec2 {-rectangle->width * 0.5f, -rectangle->height * 0.5f});
        callback(mfd::Vec2 {rectangle->width * 0.5f, -rectangle->height * 0.5f});
        callback(mfd::Vec2 {rectangle->width * 0.5f, rectangle->height * 0.5f});
        callback(mfd::Vec2 {-rectangle->width * 0.5f, rectangle->height * 0.5f});
    }
    else if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        callback(mfd::Vec2 {-ellipse->width * 0.5f, -ellipse->height * 0.5f});
        callback(mfd::Vec2 {ellipse->width * 0.5f, -ellipse->height * 0.5f});
        callback(mfd::Vec2 {ellipse->width * 0.5f, ellipse->height * 0.5f});
        callback(mfd::Vec2 {-ellipse->width * 0.5f, ellipse->height * 0.5f});
    }
    else if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        callback(mfd::Vec2 {-square->width * 0.5f, -square->height * 0.5f});
        callback(mfd::Vec2 {square->width * 0.5f, -square->height * 0.5f});
        callback(mfd::Vec2 {square->width * 0.5f, square->height * 0.5f});
        callback(mfd::Vec2 {-square->width * 0.5f, square->height * 0.5f});
    }
    else if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        callback(mfd::Vec2 {0.0f, diamond->height * 0.5f});
        callback(mfd::Vec2 {diamond->width * 0.5f, 0.0f});
        callback(mfd::Vec2 {0.0f, -diamond->height * 0.5f});
        callback(mfd::Vec2 {-diamond->width * 0.5f, 0.0f});
    }
    else if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        callback(triangle->points[0]);
        callback(triangle->points[1]);
        callback(triangle->points[2]);
    }
    else if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        for (const auto& point : polyline->points)
        {
            callback(point);
        }
    }
    else if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        for (const auto& point : bezier->controlPoints)
        {
            callback(point);
        }
    }
    else if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
    {
        ForEachPrimitiveArcPoint(
            arc->radius,
            arc->startAngleDegrees,
            arc->endAngleDegrees,
            arc->segments,
            callback);

        if (primitive.style.filled)
        {
            callback(mfd::Vec2 {});
        }
    }
    else if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
    {
        callback(mfd::Vec2 {-image->width * 0.5f, -image->height * 0.5f});
        callback(mfd::Vec2 {image->width * 0.5f, -image->height * 0.5f});
        callback(mfd::Vec2 {image->width * 0.5f, image->height * 0.5f});
        callback(mfd::Vec2 {-image->width * 0.5f, image->height * 0.5f});
    }
}

/**
 * @brief Suggests the next page to select after one page removal request.
 * @param pages Current authored pages.
 * @param removedPageIndex Page index scheduled for removal.
 * @return First remaining page index, or `-1` when the list becomes empty.
 */
inline int SuggestReplacementPageIndex(const std::vector<mfd::PageDefinition>& pages,
                                       const int removedPageIndex) noexcept
{
    for (int index = 0; index < static_cast<int>(pages.size()); ++index)
    {
        if (index != removedPageIndex)
        {
            return index;
        }
    }

    return -1;
}

/**
 * @brief Returns the default authored page index, or the first page when none is marked.
 * @param pages Current authored pages.
 * @return Index of the page marked as default, or `0` as the fallback.
 */
inline int DefaultPageIndex(const std::vector<mfd::PageDefinition>& pages) noexcept
{
    for (int index = 0; index < static_cast<int>(pages.size()); ++index)
    {
        if (pages[static_cast<std::size_t>(index)].defaultPage)
        {
            return index;
        }
    }

    return 0;
}

/**
 * @brief List of primitive kinds offered by the reticle-creation workflows.
 */
inline constexpr std::array<mfd::PrimitiveType, 14> kPrimitiveTypes {
    mfd::PrimitiveType::Text,
    mfd::PrimitiveType::Time,
    mfd::PrimitiveType::Line,
    mfd::PrimitiveType::Circle,
    mfd::PrimitiveType::Ring,
    mfd::PrimitiveType::Rectangle,
    mfd::PrimitiveType::Ellipse,
    mfd::PrimitiveType::Square,
    mfd::PrimitiveType::Diamond,
    mfd::PrimitiveType::Triangle,
    mfd::PrimitiveType::Polyline,
    mfd::PrimitiveType::Bezier,
    mfd::PrimitiveType::Arc,
    mfd::PrimitiveType::Image};

/**
 * @brief Tutorial template id reserved for the integrated aircraft-reference walkthrough.
 */
inline constexpr std::string_view kTutorialAircraftTemplateId = "mfd_tutorial_aircraft";

/**
 * @brief Tutorial primitive id reserved for the aircraft reticle label shown on the alternative strobe.
 */
inline constexpr std::string_view kTutorialAircraftLabelPrimitiveId = "aircraft_label";

/**
 * @brief Tutorial page-reticle id reserved for the renamed Page1 ownship instance.
 */
inline constexpr std::string_view kTutorialPage1OwnshipReticleId = "page1_ownship";

/**
 * @brief Tutorial template id reserved for the integrated strobe-cursor walkthrough.
 */
inline constexpr std::string_view kTutorialStrobeCursorTemplateId = "mfd_tutorial_strobe_cursor";

/**
 * @brief Tutorial dynamic template id reserved for the seeded steering-cue walkthrough.
 */
inline constexpr std::string_view kTutorialSteeringCueTemplateId = "inspired_steering_cue";

/**
 * @brief Fixed Page1 anchor used by the tutorial for the static ownship aircraft symbol.
 */
inline constexpr mfd::Vec2 kTutorialPage1OwnshipAnchor {0.0f, -0.7f};

/**
 * @brief Builds the steering-cue reticle seeded automatically by the integrated tutorial.
 * @return Reticle template matching the guided dynamic-line walkthrough.
 */
inline mfd::ReticleGroup MakeTutorialSteeringCueReticle()
{
    mfd::ReticleGroup reticle;
    reticle.id = std::string(kTutorialSteeringCueTemplateId);
    reticle.overrides.thickness = 0.0035f;

    mfd::Primitive primitive;
    primitive.id = "cue";
    primitive.type = mfd::PrimitiveType::Polyline;
    primitive.geometry = mfd::PolylineGeometry {
        {{0.0f, 0.08f},
         {0.05f, -0.02f},
         {0.015f, -0.02f},
         {0.015f, -0.08f},
         {-0.015f, -0.08f},
         {-0.015f, -0.02f},
         {-0.05f, -0.02f},
         {0.0f, 0.08f}},
        false};
    reticle.primitives.push_back(std::move(primitive));
    return reticle;
}

/**
 * @brief Returns whether one successful save should complete the active tutorial save step.
 * @param saveSucceeded `true` when the document was written successfully.
 * @param isTutorialSaveStep `true` when the guided tutorial is actively blocked on its save step.
 * @param tutorialSaveTargetMatched `true` when the exact coached `File > Save` menu target was matched.
 * @return `true` when the tutorial must advance after the save action.
 *
 * @note The integrated tutorial still coaches the menu path, but one successful
 * save must also unblock the walkthrough when the `File` menu phase was not
 * captured or when the user used `Ctrl+S`.
 */
inline bool ShouldAdvanceTutorialOnSuccessfulSave(const bool saveSucceeded,
                                                  const bool isTutorialSaveStep,
                                                  const bool tutorialSaveTargetMatched) noexcept
{
    return saveSucceeded && (tutorialSaveTargetMatched || isTutorialSaveStep);
}

/**
 * @brief Keeps editor-only layer visibility state synchronized with runtime page layers.
 * @param page Page whose editor-layer mirror must be rebuilt.
 */
inline void BootstrapEditorLayersForPage(mfd::PageDefinition& page)
{
    if (page.layers.empty())
    {
        page.layers.push_back(mfd::PageLayerDefinition {std::string(mfd::kDefaultPageLayerId)});
    }

    std::vector<mfd::EditorLayerDefinition> synchronizedStates;
    synchronizedStates.reserve(page.layers.size());
    for (const mfd::PageLayerDefinition& runtimeLayer : page.layers)
    {
        const auto iterator = std::find_if(page.editor.layers.begin(),
                                           page.editor.layers.end(),
                                           [&runtimeLayer](const mfd::EditorLayerDefinition& layer)
                                           {
                                               return mfd::PageNamesEqual(layer.id, runtimeLayer.id);
                                           });
        synchronizedStates.push_back(mfd::EditorLayerDefinition {
            runtimeLayer.id,
            iterator == page.editor.layers.end() ? true : iterator->visible});
    }
    page.editor.layers = std::move(synchronizedStates);

    const std::string fallbackLayerId = page.layers.front().id;
    for (auto& reticle : page.staticReticles)
    {
        if (reticle.layerId.empty() || mfd::FindPageLayerDefinition(page, reticle.layerId) == nullptr)
        {
            reticle.layerId = fallbackLayerId;
        }
    }

    for (auto& binding : page.dynamicReticleBindings)
    {
        if (binding.layerId.empty() || mfd::FindPageLayerDefinition(page, binding.layerId) == nullptr)
        {
            binding.layerId = fallbackLayerId;
        }
    }
}

/**
 * @brief Direction in which one runtime page layer can be shifted inside the page order.
 */
enum class PageLayerMoveDirection
{
    /** @brief Moves the layer one slot earlier in the render order. */
    Up,
    /** @brief Moves the layer one slot later in the render order. */
    Down
};

/**
 * @brief Moves one runtime page layer up or down by a single slot, keeping editor state in sync.
 * @param page Mutable page whose layer order is updated.
 * @param layerIndex Index of the layer to move inside `page.layers`.
 * @param direction Direction of the requested single-slot move.
 * @return `true` when the layer actually moved, `false` for any no-op.
 *
 * @note `PageDefinition::layers` and `PageDefinition::editor.layers` are reordered together so the
 * runtime draw order and the editor-only visibility mirror never desynchronize. The whole layer
 * elements are swapped, so every layer id, editor-only visibility flag and any other
 * `PageLayerDefinition` property travels with the moved layer. Reticle and dynamic-binding layer
 * references are keyed by id and therefore stay valid without modification.
 * @note Invalid indices, moving the first layer up and moving the last layer down are no-ops that
 * leave the page untouched.
 * @pre `BootstrapEditorLayersForPage` is invoked before the swap so the editor mirror is aligned
 * one-to-one with the runtime layers.
 */
inline bool MovePageLayer(mfd::PageDefinition& page,
                          const std::size_t layerIndex,
                          const PageLayerMoveDirection direction)
{
    if (layerIndex >= page.layers.size())
    {
        return false;
    }

    const bool movingUp = direction == PageLayerMoveDirection::Up;
    if (movingUp && layerIndex == 0U)
    {
        return false;
    }
    if (!movingUp && layerIndex + 1U >= page.layers.size())
    {
        return false;
    }

    const std::size_t targetIndex = movingUp ? layerIndex - 1U : layerIndex + 1U;

    BootstrapEditorLayersForPage(page);

    std::swap(page.layers[layerIndex], page.layers[targetIndex]);
    std::swap(page.editor.layers[layerIndex], page.editor.layers[targetIndex]);
    return true;
}

/**
 * @brief Finds one page by authored display name.
 * @param loaded Loaded editor document wrapper.
 * @param pageName Page name to search.
 * @return Matching page index, or `-1` when no page matches.
 */
inline int FindPageIndexByName(const mfd::LoadedWindowConfiguration& loaded, const std::string_view pageName)
{
    for (int index = 0; index < static_cast<int>(loaded.document.pages.size()); ++index)
    {
        const auto& page = loaded.document.pages[static_cast<std::size_t>(index)];
        if (page.name == pageName)
        {
            return index;
        }
    }

    return -1;
}

/**
 * @brief Finds one authored page-reticle instance by public id.
 * @param page Page to inspect.
 * @param reticleId Reticle id to search.
 * @return Matching reticle index, or `-1` when no reticle matches.
 */
inline int FindPageReticleIndexById(const mfd::PageDefinition& page, const std::string_view reticleId)
{
    for (int index = 0; index < static_cast<int>(page.staticReticles.size()); ++index)
    {
        if (page.staticReticles[static_cast<std::size_t>(index)].id == reticleId)
        {
            return index;
        }
    }

    return -1;
}
} // namespace editor::detail
