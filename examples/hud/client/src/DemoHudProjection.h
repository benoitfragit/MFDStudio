/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Stateless projection from external SI aircraft data to Demo HUD symbology.
 */

#include <string>

#include "DemoHudTypes.h"

namespace demo_hud
{
/**
 * @addtogroup demo_hud_integration
 * @{
 */

/**
 * @brief Builds one projected HUD frame from a physical input sample.
 * @param input SI-unit aircraft, target, weapon, A-G, approach and ILS values.
 * @return Projected HUD symbology state consumed by `DemoHudController`.
 * @pre `input` should be a complete snapshot for one aircraft/frame timestamp.
 * @note This function is stateless. A real simulation can call it directly, or
 * let `DemoHudController::Populate()` call it before publishing generated UI
 * commands.
 *
 * The EEGS funnel algorithm lives in this projection layer. External code
 * provides aircraft and target facts, not generated Bezier coordinates:
 *
 * - aircraft pitch/roll and NED velocity provide the flight-path marker;
 * - normal load factor, speed and FPM position bend and drift the funnel;
 * - target range and `targetWingspanMeters` set the funnel width;
 * - target azimuth/elevation and `targetAccelerationMps2` add lead/skew;
 * - the result is two five-point Bezier rails in `HudGunFrame`.
 */
HudFrame BuildHudFrame(const HudInputSample& input) noexcept;

/**
 * @brief Converts SI aircraft inputs into legacy HUD display units.
 * @param aircraft SI-unit aircraft input sample.
 * @return Derived state in knots, feet, degrees and normalized energy cue units.
 * @note Non-finite values are defensively replaced by safe fallbacks.
 */
AircraftState BuildAircraftStateForHud(const AircraftInputSample& aircraft) noexcept;

/**
 * @brief Converts SI target inputs into legacy HUD display units.
 * @param target SI-unit target input sample.
 * @return Derived target state in nautical miles, knots, feet and degrees.
 * @note Non-finite values are defensively replaced by safe fallbacks.
 */
TargetState BuildTargetStateForHud(const TargetInputSample& target) noexcept;

/**
 * @brief Computes dynamic launch zone values from physical SI inputs.
 * @param aircraft SI-unit aircraft input sample.
 * @param target SI-unit target input sample.
 * @param selectedMissile Selected missile type.
 * @return Simplified but ordered DLZ values in nautical miles.
 */
LaunchZone ComputeLaunchZone(const AircraftInputSample& aircraft,
                             const TargetInputSample& target,
                             MissileType selectedMissile) noexcept;

/**
 * @brief Formats a heading as a three-digit HUD value.
 * @param headingDegrees Heading in degrees; values are wrapped into [0, 360).
 * @return Three-character heading such as `"000"` or `"275"`.
 */
std::string FormatHeading(float headingDegrees);

/**
 * @brief Formats the selected missile mnemonic plus quantity.
 * @param selectedMissile Missile type whose remaining count should be shown.
 * @param inventory Current missile inventory.
 * @return HUD text such as `"AIM-120C 4"` or `"AIM-9M 2"`.
 */
std::string FormatMissileInventory(MissileType selectedMissile, const MissileInventory& inventory);

/**
 * @brief Returns the short HUD mnemonic of one missile type.
 * @param selectedMissile Missile type.
 * @return `"MRM"` for AIM-120C and `"SRM"` for AIM-9M.
 */
const char* MissileMnemonic(MissileType selectedMissile) noexcept;

/**
 * @brief Returns the short HUD mnemonic of one missile flight phase.
 * @param phase Missile phase.
 * @return Stable uppercase HUD status mnemonic.
 */
const char* MissilePhaseMnemonic(MissileFlightPhase phase) noexcept;
/** @} */
} // namespace demo_hud
