/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal authored-layout constants shared by HUD runtime components.
 */

#include "hud/HudTypes.h"

namespace hud
{
namespace detail
{
/**
 * @brief Returns the authored Gun Bore Cross position in HUD page coordinates.
 * @return High-mounted gun-line reference with a visible upper combiner margin.
 */
constexpr HudVec2 GunBoreCrossHudPosition() noexcept
{
    return HudVec2 {0.0f, 0.82f};
}
} // namespace detail
} // namespace hud
