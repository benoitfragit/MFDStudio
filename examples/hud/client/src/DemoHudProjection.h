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
 * @brief Single angular projection model shared by every conformal HUD symbol.
 *
 * The HUD uses one explicit field of view so pitch ladder bars, radar/target
 * cues, missile cues and A-G cues all map angles to authored HUD units with the
 * same 1:1 scale. All values are resolved in `DemoHudProjection.cpp`; this
 * struct only exposes them so integrations and tests can reason about the scale.
 */
struct HudAngularProjection
{
    /** Total horizontal field of view in degrees. */
    float horizontalFovDeg = 30.0f;
    /** Total vertical field of view in degrees. */
    float verticalFovDeg = 30.0f;
    /** Authored HUD half-width mapped to half the horizontal field of view. */
    float halfWidthUnits = 1.0f;
    /** Authored HUD half-height mapped to half the vertical field of view. */
    float halfHeightUnits = 1.0f;
};

/**
 * @brief One conformal symbol position with explicit field-of-view state.
 *
 * `insideFov` and `limited` are exposed so callers never silently clamp a cue:
 * a symbol outside the field of view is meant to be hidden, not pinned to the
 * edge.
 */
struct ProjectedHudPoint
{
    /** Projected HUD position, clamped to the authored conformal extent. */
    HudVec2 position {};
    /** True when the source angles are inside the conformal field of view. */
    bool insideFov = true;
    /** True when the position was clamped to the conformal extent. */
    bool limited = false;
};

/**
 * @brief Returns the shared conformal HUD field of view and extent.
 * @return The single projection model used by all conformal symbols.
 */
HudAngularProjection HudConformalProjection() noexcept;

/**
 * @brief Returns the total vertical field of view of the pitch ladder, degrees.
 * @return `kHudPitchLadderVerticalFovDeg` (30 degrees by default).
 * @note The authored `demo_hud_pitch_ladder.json` bar positions must match this
 * scale; a regression test guards the C++/JSON consistency.
 */
float HudPitchLadderVerticalFovDegrees() noexcept;

/**
 * @brief Projects a boresight-relative angular offset into HUD space.
 * @param azimuthRad Azimuth relative to boresight in radians; right is positive.
 * @param elevationRad Elevation relative to boresight in radians; up is positive.
 * @return Clamped HUD position with field-of-view and clamp flags.
 * @note Non-finite inputs are treated as zero so the output is always finite.
 */
ProjectedHudPoint ProjectBoresightAngularOffsetToHud(float azimuthRad, float elevationRad) noexcept;

/**
 * @brief Projects a vertical pitch offset into a HUD ladder position.
 * @param pitchOffsetDeg Pitch line minus displayed pitch, in degrees.
 * @return HUD position on the vertical axis using the pitch ladder scale.
 * @note The ladder is allowed to move beyond the visible aperture, so this
 * value is not clamped.
 */
HudVec2 ProjectPitchOffsetToHud(float pitchOffsetDeg) noexcept;

/**
 * @brief Tests whether a boresight-relative angle is inside the HUD field of view.
 * @param azimuthRad Azimuth relative to boresight in radians.
 * @param elevationRad Elevation relative to boresight in radians.
 * @return True when both angles are within half the conformal field of view.
 */
bool IsInsideHudFov(float azimuthRad, float elevationRad) noexcept;

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
 * - normal load factor, speed and FPM position bend and drift the funnel spine;
 * - target range and `targetWingspanMeters` scale range-sampled wall widths;
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
