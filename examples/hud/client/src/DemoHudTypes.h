/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Semantic input and projected-frame types for the reusable Demo HUD core.
 */

#include <array>

namespace demo_hud
{
/**
 * @defgroup demo_hud_integration Demo HUD integration API
 * @brief Semantic SI-unit API used to drive the generated Demo HUD.
 *
 * External aircraft code should fill `HudInputSample` once per rendered HUD
 * frame, then pass it to `DemoHudController::Populate()`. The generated
 * `DemoHudUi` API remains the only writer to the authored HUD page handles.
 *
 * @note The integration boundary is semantic state, not raw cockpit commands.
 * Panel/HOTAS inputs must be resolved by the aircraft adapter before filling
 * `WeaponInputSample`.
 */

/**
 * @addtogroup demo_hud_integration
 * @{
 */

/**
 * @brief Lightweight 2D vector expressed in HUD logical coordinates.
 */
struct HudVec2
{
    /** Horizontal HUD-space coordinate, usually normalized around screen center. */
    float x = 0.0f;
    /** Vertical HUD-space coordinate, usually normalized around screen center. */
    float y = 0.0f;
};

/**
 * @brief Five control points describing one EEGS funnel Bezier rail.
 */
using HudFunnelControlPoints = std::array<HudVec2, 5>;

/**
 * @brief HUD master mode exercised by the demo.
 */
enum class HudMasterMode
{
    /** Navigation/master mode without A-A weapon symbology. */
    Nav,
    /** Air-to-air weapon mode with target and launch-zone symbology. */
    AirToAir,
    /** Air-to-ground master mode with CCIP or STRF symbology. */
    AirToGround,
    /** Landing/approach presentation with landing-specific flight symbology. */
    Landing
};

/**
 * @brief Resolved weapon presentation mode selected by aircraft avionics.
 */
enum class HudWeaponMode
{
    /** No weapon-specific HUD symbology. */
    None,
    /** Air-to-air missile cues and dynamic launch zone. */
    AirToAirMissile,
    /** Air-to-air gun cues using the EEGS family. */
    AirToAirGun,
    /** Air-to-ground Continuously Computed Impact Point bombing cues. */
    AirToGroundCcip,
    /** Air-to-ground strafe gunnery cues. */
    AirToGroundStrafe
};

/**
 * @brief Gun sight family selected by the aircraft avionics.
 */
enum class HudGunMode
{
    /** No gun sight is selected. */
    None,
    /** Air-to-air Enhanced Envelope Gunsight. */
    Eegs,
    /** Air-to-ground strafe gun sight. */
    Strafe
};

/**
 * @brief Ammunition family used for HUD gunnery thresholds.
 */
enum class HudAmmoType
{
    /** M56 ammunition, default STRF in-range value 4000 ft. */
    M56,
    /** PGU-28 ammunition, default STRF in-range value 12000 ft. */
    Pgu28
};

/**
 * @brief Missile type selectable in the A-A demonstration.
 */
enum class MissileType
{
    /** Short-range infrared missile cue. */
    Aim9M,
    /** Medium-range radar missile cue. */
    Aim120C
};

/**
 * @brief Simplified missile flight phase used by HUD status text.
 */
enum class MissileFlightPhase
{
    /** Initial motor boost phase. */
    Boost,
    /** Midcourse guidance phase. */
    Midcourse,
    /** Active seeker phase. */
    Active,
    /** Terminal intercept phase. */
    Terminal,
    /** Impact or post-impact status phase. */
    Impact
};

/**
 * @brief Aircraft physical inputs expected by the HUD adapter.
 *
 * All quantities use SI units. The velocity vector uses an NED navigation frame:
 * north/east are positive horizontally and down is positive toward the earth.
 * A climb therefore has a negative `downSpeedMps`.
 *
 * @pre Values should be finite. The projection helpers defensively replace
 * non-finite values with safe fallbacks, but integrations should not rely on
 * that sanitization for normal operation.
 */
struct AircraftInputSample
{
    /** Monotonic aircraft sample time in seconds. */
    float elapsedSeconds = 0.0f;
    /** Aircraft yaw angle in radians. Kept for adapters that expose full attitude. */
    float yawRad = 0.0f;
    /** Aircraft pitch angle in radians, positive nose-up. */
    float pitchRad = 0.034906585f;
    /** Aircraft roll angle in radians, positive right-wing-down. */
    float rollRad = 0.0f;
    /** Navigation heading in radians, wrapped by the HUD as needed. */
    float headingRad = 0.0f;
    /** Mean sea level altitude in meters. */
    float altitudeMeters = 4572.0f;
    /** Radar/radio altitude above ground level in meters. */
    float radioAltitudeMeters = 4419.6f;
    /** North velocity component in meters per second, NED frame. */
    float northSpeedMps = 221.21f;
    /** East velocity component in meters per second, NED frame. */
    float eastSpeedMps = 0.0f;
    /** Down velocity component in meters per second, NED frame; negative while climbing. */
    float downSpeedMps = -3.86f;
    /** Current Mach number. Set <= 0 to let the HUD derive an approximate fallback. */
    float mach = 0.65f;
    /** Normal acceleration in g. */
    float normalLoadFactor = 1.0f;
    /** Throttle ratio in [0, 1]. */
    float throttleRatio = 0.62f;
    /** Specific energy rate in meters per second. */
    float specificEnergyRateMps = 0.0f;
    /** True when afterburner is active. */
    bool afterburnerActive = false;
};

/**
 * @brief Aircraft display state derived from SI inputs for HUD text.
 */
struct AircraftState
{
    /** Aircraft sample time in seconds. */
    float elapsedSeconds = 0.0f;
    /** Pitch angle in degrees after normalization. */
    float pitchDegrees = 0.0f;
    /** Roll angle in degrees after normalization. */
    float rollDegrees = 0.0f;
    /** Heading in degrees in [0, 360). */
    float headingDegrees = 0.0f;
    /** Flight-path angle in degrees derived from velocity, not attitude. */
    float flightPathAngleDegrees = 0.0f;
    /** True airspeed in knots. */
    float speedKts = 430.0f;
    /** Mach number. */
    float mach = 0.65f;
    /** Barometric altitude in feet. */
    float altitudeFeet = 15000.0f;
    /** Radar altitude in feet. */
    float radarAltitudeFeet = 14500.0f;
    /** Vertical speed in feet per minute. */
    float verticalSpeedFpm = 0.0f;
    /** Normal acceleration in g. */
    float normalLoadG = 1.0f;
    /** Throttle ratio in [0, 1]. */
    float throttle = 0.62f;
    /** Normalized specific energy cue value used by HUD chevrons. */
    float energyRate = 0.0f;
    /** True when afterburner is active. */
    bool afterburnerActive = false;
};

/**
 * @brief Air-to-air target physical inputs expected by the HUD adapter.
 */
struct TargetInputSample
{
    /** True when target track data are valid. */
    bool valid = true;
    /** Slant range to target in meters. */
    float rangeMeters = 5.4f * 1852.0f;
    /** Closing speed in meters per second; positive means range is decreasing. */
    float closingSpeedMps = 360.0f * 0.514444444f;
    /** Target aspect angle in radians. */
    float aspectRad = 2.1816616f;
    /** Target azimuth error in radians relative to aircraft nose; right is positive. */
    float azimuthRad = 0.0f;
    /** Target elevation error in radians relative to aircraft nose; up is positive. */
    float elevationRad = 0.0f;
    /** Target altitude in meters MSL. */
    float altitudeMeters = 6096.0f;
};

/**
 * @brief Target display state derived from SI inputs.
 */
struct TargetState
{
    /** True when target track data are valid. */
    bool valid = true;
    /** Slant range in nautical miles. */
    float rangeNm = 5.4f;
    /** Closing speed in knots. */
    float closureKts = 360.0f;
    /** Target aspect in degrees. */
    float aspectDegrees = 125.0f;
    /** Target azimuth in degrees. */
    float azimuthDegrees = 0.0f;
    /** Target elevation in degrees. */
    float elevationDegrees = 0.0f;
    /** Target altitude in feet MSL. */
    float altitudeFeet = 20000.0f;
};

/**
 * @brief Missile inventory used by the HUD adapter.
 */
struct MissileInventory
{
    /** Remaining AIM-9M missiles. Must not be negative in external adapters. */
    int aim9m = 2;
    /** Remaining AIM-120C missiles. Must not be negative in external adapters. */
    int aim120c = 4;
};

/**
 * @brief Weapon and master-mode inputs expected by the HUD adapter.
 *
 * These are already semantic avionics values. They are not raw panel commands:
 * the aircraft integration layer must resolve HOTAS/panel state into the active
 * master mode, selected missile and inventory before calling the HUD adapter.
 */
struct WeaponInputSample
{
    /** Resolved HUD master mode. */
    HudMasterMode masterMode = HudMasterMode::Nav;
    /** Resolved weapon symbology mode. This is avionics state, not a panel command. */
    HudWeaponMode weaponMode = HudWeaponMode::None;
    /** Resolved gun sight family. Use `None` unless the active weapon is a gun. */
    HudGunMode gunMode = HudGunMode::None;
    /** True when MASTER ARM is ARM. SIM still enables demo symbology through `simulateMode`. */
    bool masterArm = true;
    /** True when avionics are in SIM mode and should show armed symbology without expenditure. */
    bool simulateMode = false;
    /** Missile type selected by avionics for HUD cues and inventory text. */
    MissileType selectedMissile = MissileType::Aim120C;
    /** Remaining inventory by missile type. */
    MissileInventory inventory {};
    /** Gun ammunition family used for range defaults and HUD labels. */
    HudAmmoType ammoType = HudAmmoType::Pgu28;
    /** Remaining gun rounds. Negative values are sanitized to zero by projection. */
    int gunRoundsRemaining = 510;
    /** True while the gun trigger is held by the resolved aircraft control state. */
    bool triggerHeld = false;
    /** True when radar/avionics provide a valid gun target lock. */
    bool targetLocked = false;
    /** Target wingspan in meters for EEGS funnel width; 35 ft is the default fallback. */
    float targetWingspanMeters = 10.668f;
    /** Target line-of-sight acceleration in meters per second squared; zero means unavailable. */
    float targetAccelerationMps2 = 0.0f;
    /** Optional STRF in-range threshold in feet; <= 0 uses the ammunition default. */
    float strafeInRangeFeet = 0.0f;
    /** True when the HUD should display an active missile timeline. */
    bool missileInFlight = false;
    /** Time remaining for the newest active missile in seconds. */
    float activeMissileTimeRemainingSeconds = 0.0f;
    /** Flight phase for the newest active missile. */
    MissileFlightPhase activeMissilePhase = MissileFlightPhase::Impact;
};

/**
 * @brief Air-to-ground weapon-delivery inputs expected by the HUD adapter.
 *
 * Coordinates are semantic angular offsets relative to aircraft boresight. The
 * projection converts them into the authored HUD coordinate space and clamps
 * them before publishing generated UI commands.
 */
struct AirGroundInputSample
{
    /** True when A-G delivery data are valid enough to show CCIP/STRF cues. */
    bool valid = false;
    /** Slant range to the computed impact point in meters. */
    float slantRangeMeters = 1828.8f;
    /** Computed impact point azimuth in radians relative to boresight; right is positive. */
    float pipperAzimuthRad = 0.0f;
    /** Computed impact point depression in radians below boresight; down is positive. */
    float pipperDepressionRad = 0.045f;
    /** Bomb-fall-line azimuth in radians relative to boresight; right is positive. */
    float fallLineAzimuthRad = 0.0f;
    /** True when a CCIP solution cue is available. */
    bool solutionCueValid = true;
    /** CCIP solution cue depression in radians below boresight; down is positive. */
    float solutionCueDepressionRad = 0.020f;
    /** True when pull-up anticipation cue data are available. */
    bool pullupAnticipationCueValid = false;
    /** Pull-up anticipation cue depression in radians below boresight; down is positive. */
    float pullupAnticipationCueDepressionRad = 0.006f;
    /** Optional time to release in seconds; <= 0 hides release timing. */
    float timeToReleaseSeconds = 0.0f;
    /** Optional time to steerpoint/target in seconds; <= 0 hides time-to-go. */
    float timeToGoSeconds = 0.0f;
    /** True when SOI/star cue is semantically available for A-G display. */
    bool sensorOfInterestVisible = false;
    /** True when a target/steerpoint cue is semantically available for A-G display. */
    bool targetCueVisible = false;
};

/**
 * @brief Landing and runway inputs expected by the HUD adapter.
 */
struct ApproachInputSample
{
    /** True when landing gear is down and CAS/landing symbology should be forced. */
    bool landingGearDown = false;
    /** True when weight-on-wheels is detected; cancels landing declutter in the demo. */
    bool weightOnWheels = false;
    /** True when landing/approach HUD mode is selected by avionics. */
    bool landingModeActive = false;
    /** True when landing declutter is active, e.g. cage/uncage declutter. */
    bool landingDeclutterActive = false;
    /** True when FPM is available from avionics. False enables W-steering fallback. */
    bool flightPathMarkerAvailable = true;
    /** True when runway reference values are valid. */
    bool runwayReferenceValid = false;
    /** Runway magnetic/true heading in radians as provided by the adapter. */
    float runwayHeadingRad = 0.0f;
    /** Distance to runway threshold in meters; negative means unavailable. */
    float runwayDistanceMeters = -1.0f;
    /** Runway threshold elevation in meters MSL. */
    float runwayElevationMeters = 0.0f;
};

/**
 * @brief ILS avionics inputs expected by the HUD adapter.
 *
 * Deviation signs follow the HUD command convention used by this demo:
 * positive localizer dots move the vertical localizer bar to the right, and
 * positive glideslope dots move the horizontal bar up. The controller clamps
 * both values before publishing generated UI commands.
 */
struct IlsInputSample
{
    /** True when the ILS receiver is powered by avionics. */
    bool powered = false;
    /** True when ILS is selected for HUD display. */
    bool selected = false;
    /** True when the ILS signal is valid. */
    bool signalValid = false;
    /** Localizer deviation in dots; finite values are clamped to the display range. */
    float localizerDeviationDots = 0.0f;
    /** Glideslope deviation in dots; finite values are clamped to the display range. */
    float glideslopeDeviationDots = 0.0f;
    /** Selected ILS course in radians. */
    float courseRad = 0.0f;
    /** True when ILS command steering is active and reliable enough to show. */
    bool commandSteeringActive = false;
};

/**
 * @brief Complete physical input sample consumed by HUD projection algorithms.
 *
 * Fill this structure once per rendered HUD frame. The HUD controller treats it
 * as a complete snapshot: do not rely on previous values being remembered.
 *
 * @pre `aircraft`, `target` and `weapon` should describe the same simulation or
 * aircraft timestamp.
 */
struct HudInputSample
{
    /** Aircraft attitude, velocity and energy state in SI units. */
    AircraftInputSample aircraft {};
    /** Current A-A target track in SI units and radians. */
    TargetInputSample target {};
    /** Resolved master-mode and weapon state. */
    WeaponInputSample weapon {};
    /** Resolved air-to-ground delivery data. */
    AirGroundInputSample airGround {};
    /** Resolved landing and runway data. */
    ApproachInputSample approach {};
    /** Resolved ILS avionics state. */
    IlsInputSample ils {};
};

/**
 * @brief Dynamic launch zone values in nautical miles.
 */
struct LaunchZone
{
    /** Maximum aerodynamic range in nautical miles. */
    float rmax1Nm = 0.0f;
    /** No-escape maximum range in nautical miles. */
    float rmax2Nm = 0.0f;
    /** No-escape minimum range in nautical miles. */
    float rmin2Nm = 0.0f;
    /** Minimum launch range in nautical miles. */
    float rmin1Nm = 0.0f;
    /** True when target range is between `rmin2Nm` and `rmax2Nm`. */
    bool inNoEscapeZone = false;
    /** True when target range is below `rmin1Nm`. */
    bool tooClose = false;
};

/**
 * @brief HUD-projected attitude and flight-path cues.
 */
struct HudAttitudeFrame
{
    /** Pitch value actually displayed after inverted-attitude folding, degrees. */
    float displayPitchDegrees = 0.0f;
    /** Roll value actually displayed after inverted-attitude folding, degrees. */
    float displayRollDegrees = 0.0f;
    /** True when the source attitude is inverted beyond +/-90 degrees pitch. */
    bool inverted = false;
    /** Pitch-ladder HUD position. */
    HudVec2 ladderPosition {};
    /** Pitch-ladder rotation in degrees. */
    float ladderRotationDegrees = 0.0f;
    /** True-horizon HUD position after visible-aperture clamping. */
    HudVec2 trueHorizonPosition {};
    /** True-horizon rotation in degrees. */
    float trueHorizonRotationDegrees = 0.0f;
    /** True when the true horizon is inside the visible HUD aperture. */
    bool trueHorizonVisible = true;
    /** Edge-clamped ghost-horizon HUD position. */
    HudVec2 ghostHorizonPosition {};
    /** Ghost-horizon rotation in degrees. */
    float ghostHorizonRotationDegrees = 0.0f;
    /** True when the true horizon is outside the visible HUD aperture. */
    bool ghostHorizonVisible = false;
    /** Flight-path marker HUD position. */
    HudVec2 fpmPosition {};
    /** True when the flight-path marker was clipped to the HUD aperture. */
    bool fpmLimited = false;
    /** Bank-angle indicator HUD position. */
    HudVec2 bankAngleIndicatorPosition {};
    /** Bank-angle indicator rotation in degrees. */
    float bankAngleIndicatorRotationDegrees = 0.0f;
    /** True when the zenith cue should be shown. */
    bool zenithVisible = false;
    /** Zenith cue HUD position. */
    HudVec2 zenithPosition {};
    /** True when the nadir cue should be shown. */
    bool nadirVisible = false;
    /** Nadir cue HUD position. */
    HudVec2 nadirPosition {};
    /** True when recovery guidance should be shown for unusual attitude or sink rate. */
    bool levelRecoveryVisible = false;
};

/**
 * @brief HUD-projected air-data, navigation and energy cues.
 */
struct HudAirDataFrame
{
    /** True when radar altitude is considered reliable enough to show. */
    bool radarAltitudeVisible = true;
    /** Vertical-velocity caret Y position in HUD space. */
    float verticalVelocityCueY = 0.0f;
    /** True when positive energy chevron should be visible. */
    bool energyChevronUpVisible = false;
    /** True when negative energy chevron should be visible. */
    bool energyChevronDownVisible = false;
    /** Positive energy chevron HUD position. */
    HudVec2 energyChevronUpPosition {};
    /** Negative energy chevron HUD position. */
    HudVec2 energyChevronDownPosition {};
    /** True when landing gear/mode forces calibrated airspeed presentation. */
    bool forceCalibratedAirspeed = false;
    /** True when low-altitude tape spacing should be used. */
    bool fineAltitudeScale = false;
};

/**
 * @brief HUD-projected A-A weapon cues.
 */
struct HudWeaponFrame
{
    /** True when A-A HUD symbology should be available. */
    bool airToAirVisible = false;
    /** True when a valid target should be displayed. */
    bool targetVisible = false;
    /** Target designator HUD position, clamped to the HUD field of view. */
    HudVec2 targetPosition {};
    /** True when the target designator was clamped to the field-of-view edge. */
    bool targetLimited = false;
    /** Missile diamond HUD position, clamped to the HUD field of view. */
    HudVec2 missileDiamondPosition {};
    /** True when the missile diamond was clamped to the field-of-view edge. */
    bool missileDiamondLimited = false;
    /** True when a geometric limit-X should overlay a field-of-view-limited diamond. */
    bool missileLimitXVisible = false;
    /** Limit-X HUD position, coincident with the clamped missile diamond. */
    HudVec2 missileLimitXPosition {};
    /** Missile diamond scale multiplier. */
    float missileDiamondScale = 1.0f;
    /** True when the missile circle should be visible. */
    bool missileCircleVisible = false;
    /** True when the dynamic launch zone should be visible. */
    bool dynamicLaunchZoneVisible = false;
    /** True when the range cue should be visible on the DLZ. */
    bool rangeCueVisible = false;
    /** Range cue HUD position on the DLZ. */
    HudVec2 rangeCuePosition {};
    /** True when break-X should be displayed because the target is too close. */
    bool breakXVisible = false;
    /** True when the attack-steering cue should be visible. */
    bool attackSteeringCueVisible = false;
    /** Attack-steering cue HUD position. */
    HudVec2 attackSteeringCuePosition {};
    /** Dynamic launch zone values for the selected missile. */
    LaunchZone launchZone {};
    /** Computed selected-missile time of flight in seconds. */
    float selectedMissileTimeOfFlightSeconds = 0.0f;
    /** True when a launched missile status should be displayed. */
    bool missileInFlight = false;
    /** Remaining time for the newest launched missile. */
    float activeMissileTimeRemainingSeconds = 0.0f;
    /** Display phase for the newest launched missile. */
    MissileFlightPhase activeMissilePhase = MissileFlightPhase::Impact;
};

/**
 * @brief HUD-projected gun cues for A-A EEGS and A-G strafe.
 */
struct HudGunFrame
{
    /** True when A-A gun symbology should be available. */
    bool airToAirGunVisible = false;
    /** True when the EEGS funnel should be displayed. */
    bool eegsFunnelVisible = false;
    /** Horizontal EEGS funnel scale derived from target range and wingspan. */
    float eegsFunnelScaleX = 1.0f;
    /** Vertical EEGS funnel scale derived from speed/load cues. */
    float eegsFunnelScaleY = 1.0f;
    /** HUD-space funnel center drift. */
    HudVec2 eegsFunnelPosition {};
    /** Funnel roll/skew rotation in degrees. */
    float eegsFunnelRotationDegrees = 0.0f;
    /** Left EEGS funnel Bezier rail control points. */
    HudFunnelControlPoints eegsFunnelLeftControlPoints {};
    /** Right EEGS funnel Bezier rail control points. */
    HudFunnelControlPoints eegsFunnelRightControlPoints {};
    /** True when the multiple-reference gunsight reticle should be displayed. */
    bool mrgsVisible = false;
    /** MRGS reticle position. */
    HudVec2 mrgsPosition {};
    /** MRGS reticle rotation in degrees. */
    float mrgsRotationDegrees = 0.0f;
    /** True when FEDS should be displayed while trigger is held. */
    bool fedsVisible = false;
    /** True when a locked target designator circle should be shown. */
    bool tdCircleVisible = false;
    /** Locked target designator circle position, clamped to the HUD field of view. */
    HudVec2 tdCirclePosition {};
    /** True when the TD circle was clamped to the field-of-view edge. */
    bool tdCircleLimited = false;
    /** True when a geometric limit-X should overlay a field-of-view-limited TD circle. */
    bool tdCircleLimitXVisible = false;
    /** Limit-X HUD position, coincident with the clamped TD circle. */
    HudVec2 tdCircleLimitXPosition {};
    /** True when the 1G pipper should be displayed. */
    bool oneGPipperVisible = false;
    /** 1G pipper position. */
    HudVec2 oneGPipperPosition {};
    /** True when the max-G pipper should be displayed. */
    bool maxGPipperVisible = false;
    /** Max-G pipper position. */
    HudVec2 maxGPipperPosition {};
    /** True when the EEGS solution circle should be displayed. */
    bool solutionCircleVisible = false;
    /** EEGS solution circle position. */
    HudVec2 solutionCirclePosition {};
    /** True when the out-of-plane bar cue should be displayed. */
    bool outOfPlaneBarsVisible = false;
    /** True when the Bullets At Target Range cue should be displayed. */
    bool batrVisible = false;
    /** BATR cue position. */
    HudVec2 batrPosition {};
    /** True when A-G strafe symbology should be available. */
    bool strafeVisible = false;
    /** A-G strafe pipper position, clamped to the HUD field of view. */
    HudVec2 strafePipperPosition {};
    /** True when the strafe pipper was clamped to the field-of-view edge. */
    bool strafePipperLimited = false;
    /** True when strafe in-range cue should be displayed. */
    bool strafeInRangeCueVisible = false;
    /** Bullet-track end position. */
    HudVec2 bulletTrackEndPosition {};
    /** Target/slant range for gun cues in feet. */
    float targetRangeFeet = 0.0f;
    /** A-G strafe slant range in feet. */
    float strafeSlantRangeFeet = 0.0f;
};

/**
 * @brief HUD-projected A-G CCIP cues.
 */
struct HudAirGroundFrame
{
    /** True when CCIP symbology should be displayed. */
    bool ccipVisible = false;
    /** CCIP pipper HUD position, clamped to the HUD field of view. */
    HudVec2 ccipPipperPosition {};
    /** True when the CCIP pipper was clamped to the field-of-view edge. */
    bool ccipPipperLimited = false;
    /** True when a limit-X should overlay a field-of-view-limited CCIP pipper. */
    bool ccipLimitXVisible = false;
    /** Limit-X HUD position, coincident with the clamped CCIP pipper. */
    HudVec2 ccipLimitXPosition {};
    /** Bomb-fall-line X position in HUD space. */
    float bombFallLineX = 0.0f;
    /** True when CCIP solution cue should be displayed. */
    bool solutionCueVisible = false;
    /** CCIP solution cue position, clamped to the HUD field of view. */
    HudVec2 solutionCuePosition {};
    /** True when the CCIP solution cue was clamped to the field-of-view edge. */
    bool solutionCueLimited = false;
    /** True when pull-up anticipation cue should be displayed. */
    bool pullupAnticipationCueVisible = false;
    /** Pull-up anticipation cue position, clamped to the HUD field of view. */
    HudVec2 pullupAnticipationCuePosition {};
    /** True when the pull-up anticipation cue was clamped to the field-of-view edge. */
    bool pullupAnticipationCueLimited = false;
    /** CCIP slant range in feet. */
    float slantRangeFeet = 0.0f;
    /** CCIP time to release in seconds. */
    float timeToReleaseSeconds = 0.0f;
    /** Time to go in seconds. */
    float timeToGoSeconds = 0.0f;
};

/**
 * @brief HUD-projected landing and approach cues.
 */
struct HudApproachFrame
{
    /** True when landing presentation is active. */
    bool landingVisible = false;
    /** True when calibrated airspeed presentation is forced. */
    bool forceCalibratedAirspeed = false;
    /** True when altitude tape should use fine low-altitude spacing. */
    bool fineAltitudeScale = false;
    /** True when the -2.5 degree pitch line should be shown. */
    bool minusTwoPointFivePitchLineVisible = false;
    /** -2.5 degree pitch line HUD position. */
    HudVec2 minusTwoPointFivePitchLinePosition {};
    /** True when approach declutter masks selected cues. */
    bool declutterActive = false;
    /** True when roll indicator remains visible. */
    bool rollIndicatorVisible = true;
    /** True when heading tape is shifted upward for landing. */
    bool headingTapeShiftedUp = false;
};

/**
 * @brief HUD-projected ILS cues.
 */
struct HudIlsFrame
{
    /** True when localizer and glideslope bars should be shown. */
    bool barsVisible = false;
    /** Clamped localizer deviation in dots. */
    float localizerDeviationDots = 0.0f;
    /** Clamped glideslope deviation in dots. */
    float glideslopeDeviationDots = 0.0f;
    /** Localizer bar HUD position. */
    HudVec2 localizerBarPosition {};
    /** Glideslope bar HUD position. */
    HudVec2 glideslopeBarPosition {};
    /** True when ILS command-steering cue should be shown. */
    bool commandSteeringVisible = false;
    /** ILS command-steering cue HUD position. */
    HudVec2 commandSteeringPosition {};
    /** True when W-steering fallback should be shown below the boresight cross. */
    bool wSteeringVisible = false;
    /** W-steering cue HUD position. */
    HudVec2 wSteeringPosition {};
};

/**
 * @brief Complete HUD frame produced from one physical input sample.
 */
struct HudFrame
{
    /** Attitude, horizon and flight-path projection. */
    HudAttitudeFrame attitude {};
    /** Air-data, altitude and energy projection. */
    HudAirDataFrame airData {};
    /** A-A weapon and target projection. */
    HudWeaponFrame weapon {};
    /** A-A and A-G gun projection. */
    HudGunFrame gun {};
    /** A-G CCIP projection. */
    HudAirGroundFrame airGround {};
    /** Landing and approach projection. */
    HudApproachFrame approach {};
    /** ILS bars and command-steering projection. */
    HudIlsFrame ils {};
};
/** @} */
} // namespace demo_hud
