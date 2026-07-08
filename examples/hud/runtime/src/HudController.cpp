/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the HUD generated-UI adapter.
 */

#include "hud/HudController.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "HudUi.h"
#include "mfd/model/Types.h"

namespace hud
{
namespace
{
constexpr mfd::ColorRgba kHudGreen {51, 255, 136, 255};
constexpr mfd::ColorRgba kHudBright {123, 255, 180, 255};
constexpr mfd::ColorRgba kHudAmber {249, 214, 111, 255};

// Formatting helpers keep text-generation policy local to the controller. The
// simulation still owns physics and units; this file only chooses HUD captions.
std::string FormatInteger(const float value)
{
    char buffer[24] {};
    std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::lround(value)));
    return buffer;
}

std::string FormatSignedDecimal(const char* prefix, const float value, const int precision)
{
    char buffer[32] {};
    if (precision == 0)
    {
        std::snprintf(buffer, sizeof(buffer), "%s%+d", prefix, static_cast<int>(std::lround(value)));
    }
    else
    {
        std::snprintf(buffer, sizeof(buffer), "%s%+0.*f", prefix, precision, value);
    }
    return buffer;
}

std::string FormatFixedDecimal(const float value, const int precision)
{
    char buffer[32] {};
    std::snprintf(buffer, sizeof(buffer), "%0.*f", precision, value);
    return buffer;
}

std::string FormatSpeedTapeLabel(const float speedKts, const float deltaKts)
{
    const int label = static_cast<int>(std::lround(std::max(0.0f, speedKts + deltaKts) / 10.0f));
    char buffer[16] {};
    std::snprintf(buffer, sizeof(buffer), "%02d", label);
    return buffer;
}

std::string FormatAltitudeTapeLabel(const float altitudeFeet, const float deltaFeet)
{
    char buffer[16] {};
    std::snprintf(buffer, sizeof(buffer), "%.1f", std::max(0.0f, altitudeFeet + deltaFeet) / 1000.0f);
    return buffer;
}

std::string FormatMach(const float mach)
{
    char buffer[24] {};
    std::snprintf(buffer, sizeof(buffer), "M %.2f", mach);
    return buffer;
}

std::string FormatG(const float normalLoadG)
{
    char buffer[24] {};
    std::snprintf(buffer, sizeof(buffer), "G %.1f", normalLoadG);
    return buffer;
}

std::string FormatRadarAltitude(const AircraftState& aircraft, const bool visible)
{
    if (!visible)
    {
        // Real HUDs blank or mark radar altitude when the cue is unreliable.
        return "R ----";
    }

    char buffer[24] {};
    if (aircraft.radarAltitudeFeet < 1500.0f)
    {
        std::snprintf(buffer, sizeof(buffer), "R %d", static_cast<int>(std::lround(aircraft.radarAltitudeFeet)));
    }
    else
    {
        std::snprintf(buffer, sizeof(buffer), "R %.1f", aircraft.radarAltitudeFeet / 1000.0f);
    }
    return buffer;
}

const char* MasterModeCaption(const HudMasterMode mode) noexcept
{
    switch (mode)
    {
    case HudMasterMode::Nav:
        return "NAV";
    case HudMasterMode::AirToAir:
        return "A-A";
    case HudMasterMode::AirToGround:
        return "A-G";
    case HudMasterMode::Landing:
        return "LDG";
    }

    return "NAV";
}

std::string WeaponModeCaption(const WeaponInputSample& weapon)
{
    switch (weapon.weaponMode)
    {
    case HudWeaponMode::None:
        return "";
    case HudWeaponMode::AirToAirMissile:
        // The label is an avionics-resolved fact; the HUD does not know which
        // concrete missile type the aircraft carries.
        return weapon.selectedWeaponLabel;
    case HudWeaponMode::AirToAirGun:
        return "EEGS";
    case HudWeaponMode::AirToGroundCcip:
        return "CCIP";
    case HudWeaponMode::AirToGroundStrafe:
        return "STRF";
    }

    return "";
}

std::string FormatWeaponInventoryText(const WeaponInputSample& weapon)
{
    char buffer[48] {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%s %d",
        weapon.selectedWeaponLabel.c_str(),
        weapon.selectedWeaponQuantity);
    return buffer;
}

std::string FormatRangeText(const TargetState& target, const HudInputSample& input, const HudFrame& frame)
{
    if (input.weapon.weaponMode == HudWeaponMode::AirToGroundCcip && frame.airGround.ccipVisible)
    {
        char buffer[24] {};
        std::snprintf(buffer, sizeof(buffer), "F%05.1f", frame.airGround.slantRangeFeet / 1000.0f);
        return buffer;
    }

    if (input.weapon.weaponMode == HudWeaponMode::AirToGroundStrafe && frame.gun.strafeVisible)
    {
        char buffer[24] {};
        std::snprintf(buffer, sizeof(buffer), "F%05.1f", frame.gun.strafeSlantRangeFeet / 1000.0f);
        return buffer;
    }

    if (input.weapon.masterMode != HudMasterMode::AirToAir || !target.valid)
    {
        // Preserve a stable NAV-mode placeholder from the authored page.
        return "B005.4";
    }

    char buffer[24] {};
    std::snprintf(buffer, sizeof(buffer), "F%05.1f", target.rangeNm);
    return buffer;
}

std::string FormatTimingText(const HudFrame& frame, const HudInputSample& input)
{
    char buffer[32] {};
    if (input.weapon.weaponMode == HudWeaponMode::AirToGroundCcip && frame.airGround.ccipVisible)
    {
        if (frame.airGround.timeToReleaseSeconds > 0.0f)
        {
            std::snprintf(
                buffer,
                sizeof(buffer),
                "%03d:%02d",
                static_cast<int>(frame.airGround.timeToReleaseSeconds) / 60,
                static_cast<int>(frame.airGround.timeToReleaseSeconds) % 60);
            return buffer;
        }

        return "REL --";
    }

    if (!frame.weapon.airToAirVisible)
    {
        // NAV mode keeps the authored clock-style text instead of showing A-A
        // launch timing that is not meaningful outside weapon mode.
        return "001:44";
    }

    if (frame.weapon.missileInFlight)
    {
        std::snprintf(
            buffer,
            sizeof(buffer),
            "T%03d",
            static_cast<int>(std::ceil(std::max(frame.weapon.activeMissileTimeRemainingSeconds, 0.0f))));
    }
    else
    {
        std::snprintf(
            buffer,
            sizeof(buffer),
            "A%02d",
            static_cast<int>(std::ceil(frame.weapon.selectedMissileTimeOfFlightSeconds)));
    }
    return buffer;
}

std::string FormatEnergy(const AircraftState& aircraft)
{
    char buffer[32] {};
    // Energy is already normalized by `BuildAircraftStateForHud`; this layer
    // only annotates afterburner state for the operator-facing caption.
    std::snprintf(
        buffer,
        sizeof(buffer),
        aircraft.afterburnerActive ? "E %+0.1f AB" : "E %+0.1f",
        aircraft.energyRate);
    return buffer;
}

std::string FormatStatusCaption(const HudInputSample& input, const HudFrame& frame)
{
    if (frame.gun.strafeVisible)
    {
        return "GUN STRF";
    }

    if (frame.gun.airToAirGunVisible)
    {
        return input.weapon.targetLocked ? "EEGS LCK" : "EEGS";
    }

    if (frame.airGround.ccipVisible)
    {
        return "CCIP";
    }

    if (frame.approach.landingVisible)
    {
        return frame.ils.barsVisible ? "ILS LDG" : "LDG";
    }

    if (frame.weapon.airToAirVisible)
    {
        // The mnemonic is caller-resolved; an empty value degrades to "RDY".
        std::string caption = input.weapon.selectedWeaponMnemonic;
        if (!caption.empty())
        {
            caption += ' ';
        }
        return caption + "RDY";
    }

    return "";
}

std::string FormatTargetAltitudeCircle(const TargetState& target)
{
    char buffer[16] {};
    std::snprintf(buffer, sizeof(buffer), "%02d", static_cast<int>(std::lround(target.altitudeFeet / 1000.0f)));
    return buffer;
}

mfd::Vec2 ToMfdVec2(const HudVec2 value) noexcept
{
    // Keep the type conversion explicit at the generated-UI boundary.
    return mfd::Vec2 {value.x, value.y};
}

std::vector<mfd::Vec2> ToMfdVec2Vector(const HudFunnelControlPoints& points)
{
    std::vector<mfd::Vec2> result;
    result.reserve(points.size());
    for (const HudVec2 point : points)
    {
        result.push_back(ToMfdVec2(point));
    }

    return result;
}

float HudDistance(const HudVec2 start, const HudVec2 end) noexcept
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    return std::sqrt(dx * dx + dy * dy);
}

float HudAngleDegrees(const HudVec2 start, const HudVec2 end) noexcept
{
    return std::atan2(end.y - start.y, end.x - start.x) * 57.29577951308232f;
}

void HideAirToAirGunReticles(hud_ui::HUDMockupPage& hud)
{
    hud.eegsFunnel.SetVisible(false);
    hud.eegsFunnel.FunnelLeft().SetVisible(false);
    hud.eegsFunnel.FunnelRight().SetVisible(false);
    hud.eegsMrgs.SetVisible(false);
    hud.eegsFeds.SetVisible(false);
    hud.eegsTdCircle.SetVisible(false);
    hud.eegsOneGPipper.SetVisible(false);
    hud.eegsMaxGPipper.SetVisible(false);
    hud.eegsSolutionCircle.SetVisible(false);
    hud.eegsBatr.SetVisible(false);
    hud.tdCircleLimitX.SetVisible(false);
}

void ResetContextualReticles(hud_ui::HUDMockupPage& hud)
{
    hud.targetDesignator.SetVisible(false);
    hud.missileDiamond.SetVisible(false);
    hud.missileLimitX.SetVisible(false);
    hud.missileCircle.SetVisible(false);
    hud.dynamicLaunchZone.SetVisible(false);
    hud.rangeCue.SetVisible(false);
    hud.breakX.SetVisible(false);
    hud.attackSteeringCue.SetVisible(false);
    hud.missileDiamond.Blink = nullptr;
    hud.breakX.Blink = nullptr;

    HideAirToAirGunReticles(hud);

    hud.ccipLimitX.SetVisible(false);

    hud.strafeReticle.SetVisible(false);
    hud.strafeInRangeCue.SetVisible(false);
    hud.strafeBulletTrack.SetVisible(false);
    hud.ccipPipper.SetVisible(false);
    hud.ccipBombFallLine.SetVisible(false);
    hud.ccipSolutionCue.SetVisible(false);
    hud.ccipPullupAnticipationCue.SetVisible(false);

    hud.landingPitchLine.SetVisible(false);
    hud.ilsLocalizerBar.SetVisible(false);
    hud.ilsGlideslopeBar.SetVisible(false);
    hud.ilsCommandSteeringCue.SetVisible(false);
    hud.ilsWSteeringCue.SetVisible(false);
}

void ApplyAttitude(hud_ui::HUDMockupPage& hud, const HudFrame& frame)
{
    const HudAttitudeFrame& attitude = frame.attitude;

    // The pitch ladder is allowed to move beyond the visible horizon clamp so
    // high-attitude motion remains continuous when the true horizon ghosts.
    hud.pitchLadder.SetPosition(ToMfdVec2(attitude.ladderPosition));
    hud.pitchLadder.SetRotationDegrees(attitude.ladderRotationDegrees);

    // The true horizon is hidden when it leaves the HUD glass; a ghost cue is
    // then pinned to the edge to preserve orientation context.
    hud.trueHorizon.SetVisible(attitude.trueHorizonVisible);
    hud.trueHorizon.SetPosition(ToMfdVec2(attitude.trueHorizonPosition));
    hud.trueHorizon.SetRotationDegrees(attitude.trueHorizonRotationDegrees);

    hud.ghostHorizon.SetVisible(attitude.ghostHorizonVisible);
    hud.ghostHorizon.SetPosition(ToMfdVec2(attitude.ghostHorizonPosition));
    hud.ghostHorizon.SetRotationDegrees(attitude.ghostHorizonRotationDegrees);

    hud.flightPathMarker.SetPosition(ToMfdVec2(attitude.fpmPosition));
    hud.fpmLimitX.SetVisible(attitude.fpmLimited);
    hud.fpmLimitX.SetPosition(ToMfdVec2(attitude.fpmPosition));

    // The bank indicator follows the FPM so it remains visually tied to the
    // aircraft flight path instead of the fixed screen center.
    hud.bankAngleIndicator.SetPosition(ToMfdVec2(attitude.bankAngleIndicatorPosition));
    hud.bankAngleIndicator.SetRotationDegrees(attitude.bankAngleIndicatorRotationDegrees);

    hud.zenithCue.SetVisible(attitude.zenithVisible);
    hud.zenithCue.SetPosition(ToMfdVec2(attitude.zenithPosition));
    hud.zenithCue.SetRotationDegrees(attitude.ladderRotationDegrees);

    hud.nadirCue.SetVisible(attitude.nadirVisible);
    hud.nadirCue.SetPosition(ToMfdVec2(attitude.nadirPosition));
    hud.nadirCue.SetRotationDegrees(attitude.ladderRotationDegrees);

    hud.levelRecoveryCue.SetVisible(attitude.levelRecoveryVisible);
    hud.levelRecoveryCue.SetPosition(mfd::Vec2 {0.0f, attitude.inverted ? 0.26f : -0.22f});
    hud.levelRecoveryCue.SetRotationDegrees(attitude.inverted ? attitude.ladderRotationDegrees : 0.0f);
    if (attitude.levelRecoveryVisible)
    {
        // Blink only while visible; clearing the pointer avoids stale animation
        // state when the cue is hidden on the next frame.
        hud.levelRecoveryCue.Blink = hud.caution;
    }
    else
    {
        hud.levelRecoveryCue.Blink = nullptr;
    }
}

void ApplyAirData(hud_ui::HUDMockupPage& hud,
                  const HudInputSample& input,
                  const HudFrame& frame)
{
    const AircraftState aircraft = BuildAircraftStateForHud(input.aircraft);
    const TargetState target = BuildTargetStateForHud(input.target);
    const HudAirDataFrame& airData = frame.airData;
    // Low radar altitude needs finer tape granularity than barometric cruise.
    const float altitudeStepFeet =
        frame.approach.fineAltitudeScale ? 100.0f : (aircraft.radarAltitudeFeet < 1500.0f ? 100.0f : 500.0f);

    // Speed and altitude tapes show the current value plus one coarse neighbor
    // above and below, matching the compact authored HUD layout.
    hud.speedTape.SpeedValue().SetText(FormatInteger(aircraft.speedKts));
    hud.speedTape.SpeedKind().SetText("C");
    hud.speedTape.SpeedTop().SetText(FormatSpeedTapeLabel(aircraft.speedKts, 50.0f));
    hud.speedTape.SpeedBottom().SetText(FormatSpeedTapeLabel(aircraft.speedKts, -50.0f));

    hud.altitudeTape.AltValue().SetText(FormatInteger(aircraft.altitudeFeet));
    hud.altitudeTape.AltMode().SetText(airData.radarAltitudeVisible ? "R" : "B");
    hud.altitudeTape.AltTop().SetText(FormatAltitudeTapeLabel(aircraft.altitudeFeet, altitudeStepFeet));
    hud.altitudeTape.AltBottom().SetText(FormatAltitudeTapeLabel(aircraft.altitudeFeet, -altitudeStepFeet));

    hud.verticalVelocity.VviCaret().SetPosition(mfd::Vec2 {0.0f, airData.verticalVelocityCueY});
    hud.verticalVelocity.VviValue().SetText(FormatSignedDecimal("", aircraft.verticalSpeedFpm / 1000.0f, 0));

    // Side heading labels drop the hundreds digit, while the center value keeps
    // the full three-digit heading.
    const std::string heading = FormatHeading(aircraft.headingDegrees);
    hud.headingTape.HeadingValue().SetText(heading);
    hud.headingTape.HeadingLeft().SetText(FormatHeading(aircraft.headingDegrees - 10.0f).substr(1));
    hud.headingTape.HeadingRight().SetText(FormatHeading(aircraft.headingDegrees + 10.0f).substr(1));

    hud.leftStatus.MachValue().SetText(FormatMach(aircraft.mach));
    hud.leftStatus.GValue().SetText(FormatG(aircraft.normalLoadG));
    hud.leftStatus.ModeValue().SetText(MasterModeCaption(input.weapon.masterMode));
    if (input.weapon.masterMode == HudMasterMode::AirToAir &&
        input.weapon.weaponMode == HudWeaponMode::AirToAirMissile)
    {
        hud.leftStatus.WeaponValue().SetText(FormatWeaponInventoryText(input.weapon));
    }
    else if (input.weapon.weaponMode == HudWeaponMode::AirToAirGun ||
             input.weapon.weaponMode == HudWeaponMode::AirToGroundStrafe)
    {
        hud.leftStatus.WeaponValue().SetText(FormatInteger(static_cast<float>(std::max(input.weapon.gunRoundsRemaining, 0))) + " GUN");
    }
    else
    {
        hud.leftStatus.WeaponValue().SetText(WeaponModeCaption(input.weapon));
    }

    // The right status block mixes air-data, target-data and weapon timing, so
    // it is populated from the already projected frame and the SI input sample.
    hud.rightStatus.RadarAltitudeValue().SetText(FormatRadarAltitude(aircraft, airData.radarAltitudeVisible));
    hud.rightStatus.EnergyValue().SetText(FormatEnergy(aircraft));
    hud.rightStatus.TargetValue().SetText(FormatRangeText(target, input, frame));
    hud.rightStatus.TimeValue().SetText(FormatTimingText(frame, input));
    hud.rightStatus.StatusCaption().SetText(FormatStatusCaption(input, frame));
    hud.referenceCuePack.TargetAltitudeText().SetText(FormatTargetAltitudeCircle(target));

    hud.energyChevronUp.SetVisible(airData.energyChevronUpVisible);
    hud.energyChevronUp.SetPosition(ToMfdVec2(airData.energyChevronUpPosition));
    // Positive excess energy is caution-colored when it comes from afterburner.
    hud.energyChevronUp.SetColor(aircraft.afterburnerActive ? kHudAmber : kHudGreen);

    hud.energyChevronDown.SetVisible(airData.energyChevronDownVisible);
    hud.energyChevronDown.SetPosition(ToMfdVec2(airData.energyChevronDownPosition));
    hud.energyChevronDown.SetColor(kHudAmber);
}

void ApplyAirToAirMissile(hud_ui::HUDMockupPage& hud, const HudFrame& frame)
{
    const HudWeaponFrame& weapon = frame.weapon;
    const bool noEscape = weapon.launchZone.inNoEscapeZone;

    // Weapon symbology is driven entirely by the projected frame. The generated
    // handles do not inspect raw target or panel state.
    hud.targetDesignator.SetVisible(weapon.targetVisible);
    hud.targetDesignator.SetPosition(ToMfdVec2(weapon.targetPosition));
    hud.targetDesignator.SetColor(noEscape ? kHudBright : kHudGreen);

    hud.missileDiamond.SetVisible(weapon.targetVisible);
    hud.missileDiamond.SetPosition(ToMfdVec2(weapon.missileDiamondPosition));
    hud.missileDiamond.SetScale(mfd::Vec2 {weapon.missileDiamondScale, weapon.missileDiamondScale});
    hud.missileDiamond.SetColor(noEscape ? kHudBright : kHudGreen);
    if (noEscape)
    {
        // Blink the diamond only in the no-escape zone; outside that range it
        // should remain a stable target cue.
        hud.missileDiamond.Blink = hud.fast;
    }
    else
    {
        hud.missileDiamond.Blink = nullptr;
    }

    // The geometric limit-X marks a field-of-view-limited diamond. It is a
    // different cue from the too-close Break-X and uses its own reticle.
    hud.missileLimitX.SetVisible(weapon.missileLimitXVisible);
    hud.missileLimitX.SetPosition(ToMfdVec2(weapon.missileLimitXPosition));

    hud.missileCircle.SetVisible(weapon.missileCircleVisible);
    // The missile circle is centered on the FPM to express steering relative
    // to the current flight path, not the screen center.
    hud.missileCircle.SetPosition(ToMfdVec2(frame.attitude.fpmPosition));

    hud.dynamicLaunchZone.SetVisible(weapon.dynamicLaunchZoneVisible);
    hud.rangeCue.SetVisible(weapon.rangeCueVisible);
    hud.rangeCue.SetPosition(ToMfdVec2(weapon.rangeCuePosition));

    hud.breakX.SetVisible(weapon.breakXVisible);
    hud.breakX.SetPosition(ToMfdVec2(weapon.targetPosition));
    if (weapon.breakXVisible)
    {
        // The break-X is a caution condition and therefore uses the caution
        // blink channel while the target is too close.
        hud.breakX.Blink = hud.caution;
    }
    else
    {
        hud.breakX.Blink = nullptr;
    }

    hud.attackSteeringCue.SetVisible(weapon.attackSteeringCueVisible);
    hud.attackSteeringCue.SetPosition(ToMfdVec2(weapon.attackSteeringCuePosition));
    hud.attackSteeringCue.SetRotationDegrees(-frame.attitude.displayRollDegrees);

    if (!weapon.airToAirVisible)
    {
        // Defensive visibility reset: authored A-A primitives may retain their
        // last positions, but they must not remain visible in NAV mode.
        hud.targetDesignator.SetVisible(false);
        hud.missileDiamond.SetVisible(false);
        hud.missileLimitX.SetVisible(false);
        hud.missileCircle.SetVisible(false);
        hud.dynamicLaunchZone.SetVisible(false);
        hud.rangeCue.SetVisible(false);
        hud.breakX.SetVisible(false);
        hud.attackSteeringCue.SetVisible(false);
        hud.missileDiamond.Blink = nullptr;
        hud.breakX.Blink = nullptr;
    }
}

void ApplyAirToAirGun(hud_ui::HUDMockupPage& hud, const HudFrame& frame)
{
    const HudGunFrame& gun = frame.gun;
    hud.eegsFunnel.SetVisible(gun.eegsFunnelVisible);
    if (gun.eegsFunnelVisible)
    {
        hud.eegsFunnel.SetPosition(mfd::Vec2 {0.0f, 0.0f});
        hud.eegsFunnel.SetRotationDegrees(0.0f);
        hud.eegsFunnel.SetScale(mfd::Vec2 {1.0f, 1.0f});
        hud.eegsFunnel.FunnelLeft().SetVisible(true);
        hud.eegsFunnel.FunnelRight().SetVisible(true);
        hud.eegsFunnel.FunnelLeft().SetControlPoints(ToMfdVec2Vector(gun.eegsFunnelLeftControlPoints));
        hud.eegsFunnel.FunnelRight().SetControlPoints(ToMfdVec2Vector(gun.eegsFunnelRightControlPoints));
    }
    else
    {
        hud.eegsFunnel.FunnelLeft().SetVisible(false);
        hud.eegsFunnel.FunnelRight().SetVisible(false);
    }

    hud.eegsMrgs.SetVisible(gun.mrgsVisible);
    hud.eegsMrgs.SetPosition(ToMfdVec2(gun.mrgsPosition));
    hud.eegsMrgs.SetRotationDegrees(gun.mrgsRotationDegrees);
    hud.eegsMrgs.SetScale(mfd::Vec2 {gun.eegsFunnelScaleX, gun.eegsFunnelScaleY});
    hud.eegsFeds.SetVisible(gun.fedsVisible);

    hud.eegsTdCircle.SetVisible(gun.tdCircleVisible);
    hud.eegsTdCircle.SetPosition(ToMfdVec2(gun.tdCirclePosition));

    // Limit-X overlay when the locked track is clamped to the field-of-view edge.
    hud.tdCircleLimitX.SetVisible(gun.tdCircleLimitXVisible);
    hud.tdCircleLimitX.SetPosition(ToMfdVec2(gun.tdCircleLimitXPosition));

    hud.eegsOneGPipper.SetVisible(gun.oneGPipperVisible);
    hud.eegsOneGPipper.SetPosition(ToMfdVec2(gun.oneGPipperPosition));

    hud.eegsMaxGPipper.SetVisible(gun.maxGPipperVisible);
    hud.eegsMaxGPipper.SetPosition(ToMfdVec2(gun.maxGPipperPosition));

    hud.eegsSolutionCircle.SetVisible(gun.solutionCircleVisible);
    hud.eegsSolutionCircle.SetPosition(ToMfdVec2(gun.solutionCirclePosition));

    hud.eegsBatr.SetVisible(gun.batrVisible);
    hud.eegsBatr.SetPosition(ToMfdVec2(gun.batrPosition));

    if (!gun.airToAirGunVisible)
    {
        // Defensive mode-exit pass: switching straight from EEGS to A-A missile
        // must publish explicit hide commands for the funnel reticle and its
        // Bezier rail primitives.
        HideAirToAirGunReticles(hud);
    }
}

void ApplyAirToGroundCcip(hud_ui::HUDMockupPage& hud, const HudFrame& frame)
{
    const HudAirGroundFrame& airGround = frame.airGround;
    hud.ccipPipper.SetVisible(airGround.ccipVisible);
    hud.ccipPipper.SetPosition(ToMfdVec2(airGround.ccipPipperPosition));

    // When the CCIP pipper reaches the field-of-view edge it stays visible and a
    // limit-X is overlaid on the cue, matching BMS behavior.
    hud.ccipLimitX.SetVisible(airGround.ccipLimitXVisible);
    hud.ccipLimitX.SetPosition(ToMfdVec2(airGround.ccipLimitXPosition));

    hud.ccipBombFallLine.SetVisible(airGround.ccipVisible);
    const HudVec2 fallLineTop {airGround.bombFallLineX, 0.46f};
    const HudVec2 fallLineBottom {airGround.ccipPipperPosition.x, airGround.ccipPipperPosition.y + 0.055f};
    const float fallLineLength = HudDistance(fallLineTop, fallLineBottom);
    const HudVec2 fallLineCenter {
        (fallLineTop.x + fallLineBottom.x) * 0.5f,
        (fallLineTop.y + fallLineBottom.y) * 0.5f};
    hud.ccipBombFallLine.SetPosition(ToMfdVec2(fallLineCenter));
    hud.ccipBombFallLine.SetRotationDegrees(HudAngleDegrees(fallLineTop, fallLineBottom));
    hud.ccipBombFallLine.SetScale(mfd::Vec2 {fallLineLength, 1.0f});

    hud.ccipSolutionCue.SetVisible(airGround.solutionCueVisible);
    hud.ccipSolutionCue.SetPosition(ToMfdVec2(airGround.solutionCuePosition));

    hud.ccipPullupAnticipationCue.SetVisible(airGround.pullupAnticipationCueVisible);
    hud.ccipPullupAnticipationCue.SetPosition(ToMfdVec2(airGround.pullupAnticipationCuePosition));
}

void ApplyAirToGroundStrafe(hud_ui::HUDMockupPage& hud, const HudFrame& frame)
{
    const HudGunFrame& gun = frame.gun;
    hud.strafeReticle.SetVisible(gun.strafeVisible);
    hud.strafeReticle.SetPosition(ToMfdVec2(gun.strafePipperPosition));

    hud.strafeInRangeCue.SetVisible(gun.strafeInRangeCueVisible);
    hud.strafeInRangeCue.SetPosition(ToMfdVec2(gun.strafePipperPosition));

    hud.strafeBulletTrack.SetVisible(gun.strafeVisible);
    const HudVec2 boresightPosition {0.0f, 0.90f};
    const HudVec2 lineCenter {
        (boresightPosition.x + gun.bulletTrackEndPosition.x) * 0.5f,
        (boresightPosition.y + gun.bulletTrackEndPosition.y) * 0.5f};
    const float lineLength = HudDistance(boresightPosition, gun.bulletTrackEndPosition);
    hud.strafeBulletTrack.SetPosition(ToMfdVec2(lineCenter));
    hud.strafeBulletTrack.SetRotationDegrees(HudAngleDegrees(boresightPosition, gun.bulletTrackEndPosition));
    hud.strafeBulletTrack.SetScale(mfd::Vec2 {lineLength, 1.0f});
}

void ApplyLandingApproach(hud_ui::HUDMockupPage& hud, const HudInputSample& input, const HudFrame& frame)
{
    const HudApproachFrame& approach = frame.approach;
    hud.landingPitchLine.SetVisible(approach.minusTwoPointFivePitchLineVisible);
    hud.landingPitchLine.SetPosition(ToMfdVec2(approach.minusTwoPointFivePitchLinePosition));
    hud.landingPitchLine.SetRotationDegrees(frame.attitude.ladderRotationDegrees);

    hud.bankAngleIndicator.SetVisible(approach.rollIndicatorVisible);
    hud.headingTape.SetPosition(mfd::Vec2 {0.0f, approach.headingTapeShiftedUp ? 1.02f : 0.0f});
    hud.flightPathMarker.SetVisible(input.approach.flightPathMarkerAvailable);
    if (!input.approach.flightPathMarkerAvailable)
    {
        hud.fpmLimitX.SetVisible(false);
    }

    if (approach.declutterActive)
    {
        hud.attackSteeringCue.SetVisible(false);
    }
}

void ApplyIls(hud_ui::HUDMockupPage& hud, const HudFrame& frame)
{
    const HudIlsFrame& ils = frame.ils;
    hud.ilsLocalizerBar.SetVisible(ils.barsVisible);
    hud.ilsLocalizerBar.SetPosition(ToMfdVec2(ils.localizerBarPosition));

    hud.ilsGlideslopeBar.SetVisible(ils.barsVisible);
    hud.ilsGlideslopeBar.SetPosition(ToMfdVec2(ils.glideslopeBarPosition));

    hud.ilsCommandSteeringCue.SetVisible(ils.commandSteeringVisible);
    hud.ilsCommandSteeringCue.SetPosition(ToMfdVec2(ils.commandSteeringPosition));

    hud.ilsWSteeringCue.SetVisible(ils.wSteeringVisible);
    hud.ilsWSteeringCue.SetPosition(ToMfdVec2(ils.wSteeringPosition));
}

void ApplyWeapon(hud_ui::HUDMockupPage& hud, const HudFrame& frame)
{
    ApplyAirToAirMissile(hud, frame);
    ApplyAirToAirGun(hud, frame);
    ApplyAirToGroundCcip(hud, frame);
    ApplyAirToGroundStrafe(hud, frame);
}
} // namespace

void HudController::Populate(hud_ui::HudUi& ui, const HudInputSample& input) const
{
    // HUD input contract:
    // - Put aircraft physics in `input.aircraft` using SI units:
    //   altitudeMeters, radioAltitudeMeters, yawRad, pitchRad, rollRad,
    //   headingRad, northSpeedMps, eastSpeedMps, downSpeedMps, mach,
    //   normalLoadFactor and specificEnergyRateMps.
    // - Put A-A target data in `input.target`: rangeMeters, closingSpeedMps,
    //   aspectRad, azimuthRad, elevationRad and altitudeMeters.
    // - Put resolved avionics state in `input.weapon`: masterMode, the generic
    //   selected-weapon label/mnemonic/quantity, the already-computed launch
    //   zone and time of flight, and in-flight missile timing.
    //
    // This function is the only place where semantic aircraft data is converted
    // to generated HUD commands. Raw UI/panel commands must not be wired to
    // HUD output handles directly.
    const HudFrame frame = hud::BuildHudFrame(input);

    hud_ui::HUDMockupPage& hud = ui.HUD();
    ApplyAttitude(hud, frame);
    ApplyAirData(hud, input, frame);
    ResetContextualReticles(hud);
    ApplyWeapon(hud, frame);
    ApplyLandingApproach(hud, input, frame);
    ApplyIls(hud, frame);
}

} // namespace hud
