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
#include <cstdio>
#include <string_view>
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
 * @brief Tutorial template id reserved for the integrated strobe-cursor walkthrough.
 */
inline constexpr std::string_view kTutorialStrobeCursorTemplateId = "mfd_tutorial_strobe_cursor";

/**
 * @brief Fixed Page1 anchor used by the tutorial for the static ownship aircraft symbol.
 */
inline constexpr mfd::Vec2 kTutorialPage1OwnshipAnchor {0.0f, -0.7f};

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
