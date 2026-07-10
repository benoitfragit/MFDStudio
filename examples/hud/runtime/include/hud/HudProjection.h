/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Stateless projection from external SI aircraft data to HUD symbology.
 */

#include <string>

#include "hud/HudRuntimeExport.h"
#include "hud/HudTypes.h"

namespace hud
{
/**
 * @addtogroup hud_integration
 * @{
 */

/**
 * @brief Single angular projection model shared by every conformal HUD symbol.
 *
 * The HUD uses one explicit field of view so pitch ladder bars, radar/target
 * cues, missile cues and A-G cues all map angles to authored HUD units with the
 * same 1:1 scale. All values are resolved in `HudProjection.cpp`; this
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
 * `insideFov` and `limited` are exposed so callers never silently clamp a cue.
 * A symbol outside the field of view is explicitly reported as limited; the
 * owning symbology decides whether to clamp with a limit-X, use a locator line,
 * or hide it only when the real mode requires it.
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
HUD_RUNTIME_API HudAngularProjection HudConformalProjection() noexcept;

/**
 * @brief Returns the total vertical field of view of the pitch ladder, degrees.
 * @return `kHudPitchLadderVerticalFovDeg` (30 degrees by default).
 * @note The authored `hud_pitch_ladder.json` bar positions must match this
 * scale; a regression test guards the C++/JSON consistency.
 */
HUD_RUNTIME_API float HudPitchLadderVerticalFovDegrees() noexcept;

/**
 * @brief Projects a boresight-relative angular offset into HUD space.
 * @param azimuthRad Azimuth relative to boresight in radians; right is positive.
 * @param elevationRad Elevation relative to boresight in radians; up is positive.
 * @return Clamped HUD position with field-of-view and clamp flags.
 * @note Non-finite inputs are treated as zero so the output is always finite.
 */
HUD_RUNTIME_API ProjectedHudPoint ProjectBoresightAngularOffsetToHud(float azimuthRad, float elevationRad) noexcept;

/**
 * @brief Projects a vertical pitch offset into a HUD ladder position.
 * @param pitchOffsetDeg Pitch line minus displayed pitch, in degrees.
 * @return HUD position on the vertical axis using the pitch ladder scale.
 * @note The ladder is allowed to move beyond the visible aperture, so this
 * value is not clamped.
 */
HUD_RUNTIME_API HudVec2 ProjectPitchOffsetToHud(float pitchOffsetDeg) noexcept;

/**
 * @brief Tests whether a boresight-relative angle is inside the HUD field of view.
 * @param azimuthRad Azimuth relative to boresight in radians.
 * @param elevationRad Elevation relative to boresight in radians.
 * @return True when both angles are within half the conformal field of view.
 */
HUD_RUNTIME_API bool IsInsideHudFov(float azimuthRad, float elevationRad) noexcept;

/**
 * @brief Builds one projected HUD frame from a physical input sample.
 * @param input SI-unit aircraft, target, weapon, A-G, approach and ILS values.
 * @return Projected HUD symbology state consumed by `HudController`.
 * @pre `input` should be a complete snapshot for one aircraft/frame timestamp.
 * @note This function is stateless. A real simulation can call it directly, or
 * let `HudController::Populate()` call it before publishing generated UI
 * commands.
 *
 * The EEGS funnel algorithm lives in this projection layer. External code
 * provides aircraft and target facts, not generated Bezier coordinates:
 *
 * - aircraft pitch/roll and NED velocity provide the flight-path marker;
 * - the funnel spine follows the tracer trail of previously fired rounds:
 *   each range sample maps to a bullet time of flight, then lags behind the
 *   load-factor-driven lift-plane rotation and keeps its roll-signed gravity
 *   drop, so the far end sweeps farther than the near end while maneuvering;
 * - target range and `targetWingspanMeters` scale range-sampled wall widths;
 * - target azimuth/elevation and `targetAccelerationMps2` drift the spine;
 * - the result is two five-point Bezier rails in `HudGunFrame`.
 *
 * Weapon-specific values are the opposite: launch zone, time of flight, labels
 * and quantities are caller-resolved facts read from `WeaponInputSample`, never
 * computed by the HUD runtime.
 */
HUD_RUNTIME_API HudFrame BuildHudFrame(const HudInputSample& input) noexcept;

/**
 * @brief Converts SI aircraft inputs into legacy HUD display units.
 * @param aircraft SI-unit aircraft input sample.
 * @return Derived state in knots, feet, degrees and normalized energy cue units.
 * @note Non-finite values are defensively replaced by safe fallbacks.
 */
HUD_RUNTIME_API AircraftState BuildAircraftStateForHud(const AircraftInputSample& aircraft) noexcept;

/**
 * @brief Converts SI target inputs into legacy HUD display units.
 * @param target SI-unit target input sample.
 * @return Derived target state in nautical miles, knots, feet and degrees.
 * @note Non-finite values are defensively replaced by safe fallbacks.
 */
HUD_RUNTIME_API TargetState BuildTargetStateForHud(const TargetInputSample& target) noexcept;

/**
 * @brief Formats a heading as a three-digit HUD value.
 * @param headingDegrees Heading in degrees; values are wrapped into [0, 360).
 * @return Three-character heading such as `"000"` or `"275"`.
 */
HUD_RUNTIME_API std::string FormatHeading(float headingDegrees);

/**
 * @brief Returns the short HUD mnemonic of one missile flight phase.
 * @param phase Missile phase.
 * @return Stable uppercase HUD status mnemonic.
 */
HUD_RUNTIME_API const char* MissilePhaseMnemonic(MissileFlightPhase phase) noexcept;
/** @} */
} // namespace hud
