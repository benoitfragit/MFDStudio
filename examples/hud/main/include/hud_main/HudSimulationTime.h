/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include <cstddef>

namespace hud_main
{
/** @brief Authoritative fixed step used by every stateful HUD mini-simulation component. */
constexpr double kHudSimulationStepSeconds = 0.020;

/** @brief Maximum fixed ticks executed for one rendered frame. */
constexpr std::size_t kMaximumSimulationTicksPerFrame = 8U;
} // namespace hud_main
