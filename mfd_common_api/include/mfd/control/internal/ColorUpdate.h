/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include <optional>
#include <stdexcept>

#include "mfd/model/Types.h"

namespace mfd::detail
{
/** @brief Checks the atomic preset/RGBA property, accepting legacy RGBA-only updates. */
inline bool IsValidColorUpdate(const std::optional<ColorRgba>& rgba,
                               const std::optional<ColorPreset>& preset) noexcept
{
    return !preset || (*preset == ColorPreset::Custom
                          ? rgba.has_value()
                          : !rgba && TryColorPreset(*preset).has_value());
}

/** @brief Resolves a validated color update at the runtime boundary.
 * @pre IsValidColorUpdate(rgba, preset) is true.
 */
inline std::optional<ColorRgba> ResolveColorUpdate(const std::optional<ColorRgba>& rgba,
                                                   const std::optional<ColorPreset>& preset) noexcept
{
    return preset && *preset != ColorPreset::Custom ? TryColorPreset(*preset) : rgba;
}

/** @brief Rejects Custom and unknown presets before modifying staged client state. */
inline void RequireBuiltInColor(const ColorPreset preset)
{
    if (!TryColorPreset(preset))
    {
        throw std::invalid_argument("Color preset requires a built-in color; use the RGBA overload for Custom");
    }
}

/** @brief Stages an explicit custom RGBA update, replacing the previous choice. */
inline void SetColorUpdate(std::optional<ColorRgba>& rgba, std::optional<ColorPreset>& preset,
                           const ColorRgba value) noexcept
{
    rgba = value;
    preset = ColorPreset::Custom;
}

/** @brief Stages a validated built-in preset, removing any stale RGBA value. */
inline void SetColorUpdate(std::optional<ColorRgba>& rgba, std::optional<ColorPreset>& preset,
                           const ColorPreset value)
{
    RequireBuiltInColor(value);
    rgba.reset();
    preset = value;
}

/** @brief Compares color choices including their wire representation and alpha. */
inline bool EqualColorUpdate(const std::optional<ColorRgba>& lhs,
                             const std::optional<ColorPreset>& lhsPreset,
                             const std::optional<ColorRgba>& rhs,
                             const std::optional<ColorPreset>& rhsPreset) noexcept
{
    return lhsPreset == rhsPreset && lhs.has_value() == rhs.has_value() &&
           (!lhs || (lhs->r == rhs->r && lhs->g == rhs->g && lhs->b == rhs->b && lhs->a == rhs->a));
}

/** @brief Copies both halves of a changed logical color property into a delta. */
inline void CopyChangedColorUpdate(const std::optional<ColorRgba>& desired,
                                   const std::optional<ColorPreset>& desiredPreset,
                                   const std::optional<ColorRgba>& previous,
                                   const std::optional<ColorPreset>& previousPreset,
                                   std::optional<ColorRgba>& delta,
                                   std::optional<ColorPreset>& deltaPreset) noexcept
{
    if (!EqualColorUpdate(desired, desiredPreset, previous, previousPreset))
    {
        delta = desired;
        deltaPreset = desiredPreset;
    }
}

/** @brief Merges one logical property; absent newest fields preserve the older choice.
 * @note Destination may alias either input, as used by both coalescing layers.
 */
inline void MergeColorUpdate(std::optional<ColorRgba>& destination,
                             std::optional<ColorPreset>& destinationPreset,
                             const std::optional<ColorRgba>& older,
                             const std::optional<ColorPreset>& olderPreset,
                             const std::optional<ColorRgba>& newer,
                             const std::optional<ColorPreset>& newerPreset) noexcept
{
    const bool hasNewer = newer.has_value() || newerPreset.has_value();
    destination = hasNewer ? newer : older;
    destinationPreset = hasNewer ? newerPreset : olderPreset;
}
} // namespace mfd::detail
