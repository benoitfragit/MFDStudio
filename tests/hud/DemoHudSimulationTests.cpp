/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Regression tests for the Demo HUD demo simulation rules.
 */

#include "DemoHudSimulation.h"
#include "DemoHudController.h"
#include "DemoHudUi.h"

#include <cmath>
#include <limits>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

namespace
{
using demo_hud::ComputeLaunchZone;
using demo_hud::DemoHudSimulation;
using demo_hud::DemoHudController;
using demo_hud::BuildAircraftStateForHud;
using demo_hud::BuildHudFrame;
using demo_hud::HudInputSample;
using demo_hud::HudMasterMode;
using demo_hud::HudGunMode;
using demo_hud::HudWeaponMode;
using demo_hud::MissileType;
using demo_hud::PilotControls;

constexpr float kDegreesToRadians = 0.017453292519943295f;

void StepMany(DemoHudSimulation& simulation, const PilotControls& controls, const int steps)
{
    simulation.SetControls(controls);
    for (int index = 0; index < steps; ++index)
    {
        simulation.Step(0.080f);
    }
}

bool IsFiniteHudVec(const demo_hud::HudVec2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool IsFiniteFunnelRail(const demo_hud::HudFunnelControlPoints& rail)
{
    for (const demo_hud::HudVec2 point : rail)
    {
        if (!IsFiniteHudVec(point))
        {
            return false;
        }
    }

    return true;
}

float HorizontalGap(const demo_hud::HudVec2 left, const demo_hud::HudVec2 right)
{
    return std::fabs(right.x - left.x);
}

float HorizontalGap(const mfd::Vec2 left, const mfd::Vec2 right)
{
    return std::fabs(right.x - left.x);
}

void ExpectAirToAirMissileReticlesHidden(demo_hud_ui::HUDMockupPage& hud)
{
    EXPECT_FALSE(hud.targetDesignator.GetVisible());
    EXPECT_FALSE(hud.missileDiamond.GetVisible());
    EXPECT_FALSE(hud.missileCircle.GetVisible());
    EXPECT_FALSE(hud.dynamicLaunchZone.GetVisible());
    EXPECT_FALSE(hud.rangeCue.GetVisible());
    EXPECT_FALSE(hud.breakX.GetVisible());
    EXPECT_FALSE(hud.attackSteeringCue.GetVisible());
}

void ExpectAirToAirGunReticlesHidden(demo_hud_ui::HUDMockupPage& hud)
{
    EXPECT_FALSE(hud.eegsFunnel.GetVisible());
    EXPECT_FALSE(hud.eegsMrgs.GetVisible());
    EXPECT_FALSE(hud.eegsFeds.GetVisible());
    EXPECT_FALSE(hud.eegsTdCircle.GetVisible());
    EXPECT_FALSE(hud.eegsOneGPipper.GetVisible());
    EXPECT_FALSE(hud.eegsMaxGPipper.GetVisible());
    EXPECT_FALSE(hud.eegsSolutionCircle.GetVisible());
    EXPECT_FALSE(hud.eegsBatr.GetVisible());
}

void ExpectAirToGroundReticlesHidden(demo_hud_ui::HUDMockupPage& hud)
{
    EXPECT_FALSE(hud.strafeReticle.GetVisible());
    EXPECT_FALSE(hud.strafeInRangeCue.GetVisible());
    EXPECT_FALSE(hud.strafeBulletTrack.GetVisible());
    EXPECT_FALSE(hud.ccipPipper.GetVisible());
    EXPECT_FALSE(hud.ccipBombFallLine.GetVisible());
    EXPECT_FALSE(hud.ccipSolutionCue.GetVisible());
    EXPECT_FALSE(hud.ccipPullupAnticipationCue.GetVisible());
}

void ExpectApproachReticlesHidden(demo_hud_ui::HUDMockupPage& hud)
{
    EXPECT_FALSE(hud.landingPitchLine.GetVisible());
    EXPECT_FALSE(hud.ilsLocalizerBar.GetVisible());
    EXPECT_FALSE(hud.ilsGlideslopeBar.GetVisible());
    EXPECT_FALSE(hud.ilsCommandSteeringCue.GetVisible());
    EXPECT_FALSE(hud.ilsWSteeringCue.GetVisible());
}

const mfd::UpdateReticleCommand* FindReticleUpdate(const std::vector<mfd::UserCommand>& commands, const char* reticleId)
{
    for (const mfd::UserCommand& command : commands)
    {
        const auto* update = std::get_if<mfd::UpdateReticleCommand>(&command);
        if (update != nullptr && update->target.reticle == reticleId)
        {
            return update;
        }
    }

    return nullptr;
}

std::size_t CountPrimitiveVisibilityPatches(const mfd::UpdateReticleCommand& update, const bool expectedVisible)
{
    std::size_t count = 0U;
    for (const auto& entry : update.patch.primitivePatchesById)
    {
        if (entry.second.visible.has_value() && *entry.second.visible == expectedVisible)
        {
            ++count;
        }
    }

    return count;
}

TEST(DemoHudSimulationTests, IgnoresNonFiniteAndClampsLargeDeltas)
{
    DemoHudSimulation simulation;
    const float initialPitch = simulation.Aircraft().pitchDegrees;

    simulation.Step(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(simulation.Aircraft().pitchDegrees, initialPitch);

    PilotControls controls;
    controls.pitchCommand = 1.0f;
    simulation.SetControls(controls);
    simulation.Step(10.0f);

    EXPECT_TRUE(std::isfinite(simulation.Aircraft().pitchDegrees));
    EXPECT_LT(std::fabs(simulation.Aircraft().pitchDegrees - initialPitch), 10.0f);
}

TEST(DemoHudSimulationTests, LoopingAttitudeKeepsHudPitchBoundedAndMarksInversion)
{
    DemoHudSimulation simulation;
    PilotControls controls;
    controls.pitchCommand = 1.0f;
    controls.throttle = 0.95f;
    controls.afterburnerRequested = true;

    StepMany(simulation, controls, 35);

    const demo_hud::HudFrame frame = simulation.BuildHudFrame();
    EXPECT_LE(std::fabs(frame.attitude.displayPitchDegrees), 90.0f);
    EXPECT_TRUE(frame.attitude.inverted);
    EXPECT_TRUE(frame.attitude.ghostHorizonVisible || frame.attitude.trueHorizonVisible);
}

TEST(DemoHudSimulationTests, LadderPivotsAboutHudCenterUnderBank)
{
    DemoHudSimulation simulation;
    PilotControls controls;
    controls.pitchCommand = 0.4f;
    controls.rollCommand = 1.0f;
    controls.throttle = 0.85f;

    StepMany(simulation, controls, 9);

    const demo_hud::HudFrame frame = simulation.BuildHudFrame();
    // Preconditions: a genuine climbing bank so the pivot difference is observable.
    ASSERT_GT(frame.attitude.displayPitchDegrees, 4.0f);
    ASSERT_GT(std::fabs(frame.attitude.displayRollDegrees), 25.0f);
    ASSERT_LT(std::fabs(frame.attitude.displayPitchDegrees), 90.0f);

    // Un-rotating the ladder position by the bank angle must recover a pure vertical
    // pitch offset (no lateral component) below the horizon center: proof the ladder
    // pivots about the HUD center, not the moving horizon point.
    const demo_hud::HudVec2 pos = frame.attitude.ladderPosition;
    const float bankRadians = frame.attitude.displayRollDegrees * kDegreesToRadians;
    const float unrotatedX = pos.x * std::cos(bankRadians) - pos.y * std::sin(bankRadians);
    const float unrotatedY = pos.x * std::sin(bankRadians) + pos.y * std::cos(bankRadians);

    EXPECT_NEAR(unrotatedX, 0.0f, 1.0e-4f);
    EXPECT_LT(unrotatedY, 0.0f);
    // Under bank the world-space ladder offset genuinely gains a lateral component
    // (this is exactly what the previous {0, horizonRawY} placement got wrong).
    EXPECT_GT(std::fabs(pos.x), 1.0e-3f);
}

TEST(DemoHudSimulationTests, LevelRecoveryDoesNotStayVisibleAfterPureRoll)
{
    HudInputSample input;
    input.aircraft.rollRad = 170.0f * kDegreesToRadians;
    input.aircraft.pitchRad = 0.0f;
    input.aircraft.radioAltitudeMeters = 2000.0f;
    input.aircraft.downSpeedMps = 0.0f;

    const demo_hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_FALSE(frame.attitude.inverted);
    EXPECT_FALSE(frame.attitude.levelRecoveryVisible);
}

TEST(DemoHudSimulationTests, FullLoopKeepsAttitudeSymbologyFinite)
{
    DemoHudSimulation simulation;
    PilotControls controls;
    controls.pitchCommand = 1.0f;
    controls.throttle = 0.95f;
    controls.afterburnerRequested = true;
    simulation.SetControls(controls);

    for (int index = 0; index < 140; ++index)
    {
        simulation.Step(0.080f);
        const demo_hud::HudFrame frame = simulation.BuildHudFrame();
        ASSERT_TRUE(IsFiniteHudVec(frame.attitude.ladderPosition));
        ASSERT_TRUE(IsFiniteHudVec(frame.attitude.trueHorizonPosition));
        ASSERT_TRUE(IsFiniteHudVec(frame.attitude.ghostHorizonPosition));
        ASSERT_TRUE(IsFiniteHudVec(frame.attitude.fpmPosition));
        ASSERT_TRUE(IsFiniteHudVec(frame.attitude.zenithPosition));
        ASSERT_TRUE(IsFiniteHudVec(frame.attitude.nadirPosition));
        ASSERT_TRUE(std::isfinite(frame.attitude.ladderRotationDegrees));
        ASSERT_LE(std::fabs(frame.attitude.displayPitchDegrees), 90.0f);
    }
}

TEST(DemoHudSimulationTests, PitchCommandIsSmoothedAcrossSeveralFrames)
{
    DemoHudSimulation simulation;
    PilotControls controls;
    controls.pitchCommand = 1.0f;
    simulation.SetControls(controls);

    const float initialPitch = simulation.Aircraft().pitchDegrees;
    simulation.Step(0.080f);
    const float firstStepPitch = simulation.Aircraft().pitchDegrees;

    StepMany(simulation, controls, 8);

    EXPECT_LT(firstStepPitch - initialPitch, 0.9f);
    EXPECT_GT(simulation.Aircraft().pitchDegrees - firstStepPitch, 8.0f);
}

TEST(DemoHudSimulationTests, RadarAltitudeIsBlankedAtHighAltitudeForLargeAttitude)
{
    DemoHudSimulation simulation;
    PilotControls controls;
    controls.pitchCommand = 1.0f;

    StepMany(simulation, controls, 12);

    const demo_hud::HudFrame frame = simulation.BuildHudFrame();
    ASSERT_GT(simulation.Aircraft().radarAltitudeFeet, 5000.0f);
    EXPECT_FALSE(frame.airData.radarAltitudeVisible);
}

TEST(DemoHudSimulationTests, LaunchZoneRemainsOrderedAndDetectsNoEscapeZone)
{
    DemoHudSimulation simulation;
    const demo_hud::LaunchZone zone =
        ComputeLaunchZone(simulation.Inputs().aircraft, simulation.Inputs().target, MissileType::Aim120C);
    const demo_hud::TargetState target = simulation.Target();

    EXPECT_GT(zone.rmax1Nm, zone.rmax2Nm);
    EXPECT_GT(zone.rmax2Nm, zone.rmin2Nm);
    EXPECT_GT(zone.rmin2Nm, zone.rmin1Nm);
    EXPECT_EQ(zone.inNoEscapeZone, target.rangeNm >= zone.rmin2Nm && target.rangeNm <= zone.rmax2Nm);
    EXPECT_FALSE(zone.tooClose);
}

TEST(DemoHudSimulationTests, ExternalSiInputsDriveHudFrameWithoutDemoControls)
{
    HudInputSample input;
    input.aircraft.pitchRad = 5.0f * kDegreesToRadians;
    input.aircraft.rollRad = -12.0f * kDegreesToRadians;
    input.aircraft.headingRad = 91.0f * kDegreesToRadians;
    input.aircraft.altitudeMeters = 3000.0f;
    input.aircraft.radioAltitudeMeters = 220.0f;
    input.aircraft.northSpeedMps = 140.0f;
    input.aircraft.eastSpeedMps = 30.0f;
    input.aircraft.downSpeedMps = -12.0f;
    input.aircraft.mach = 0.48f;
    input.aircraft.normalLoadFactor = 1.4f;
    input.aircraft.specificEnergyRateMps = 9.0f;

    const demo_hud::AircraftState display = BuildAircraftStateForHud(input.aircraft);
    EXPECT_NEAR(display.altitudeFeet, 9842.52f, 0.5f);
    EXPECT_NEAR(display.radarAltitudeFeet, 721.78f, 0.5f);
    EXPECT_NEAR(display.headingDegrees, 91.0f, 0.1f);
    EXPECT_GT(display.flightPathAngleDegrees, 4.0f);

    const demo_hud::HudFrame frame = BuildHudFrame(input);
    EXPECT_TRUE(IsFiniteHudVec(frame.attitude.fpmPosition));
    EXPECT_LT(frame.attitude.fpmPosition.y, 0.05f);
    EXPECT_TRUE(frame.airData.energyChevronUpVisible);
}

TEST(DemoHudSimulationTests, AirToAirLaunchConsumesInventoryAndCountsDown)
{
    DemoHudSimulation simulation;
    PilotControls controls;
    controls.masterMode = HudMasterMode::AirToAir;
    controls.throttle = 0.90f;
    simulation.SetControls(controls);
    simulation.SelectMissile(MissileType::Aim120C);

    ASSERT_TRUE(simulation.FireSelectedMissile());
    ASSERT_EQ(simulation.Inventory().aim120c, 3);
    ASSERT_EQ(simulation.MissileShots().size(), 1U);

    const float initialRemaining = simulation.MissileShots().front().timeRemainingSeconds;
    simulation.Step(0.5f);

    ASSERT_EQ(simulation.MissileShots().size(), 1U);
    EXPECT_LT(simulation.MissileShots().front().timeRemainingSeconds, initialRemaining);
}

TEST(DemoHudSimulationTests, MasterArmGatesAirToAirMissileSymbologyAndLaunch)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirMissile;
    input.weapon.masterArm = false;
    input.weapon.simulateMode = false;
    input.target.valid = true;

    const demo_hud::HudFrame safeFrame = BuildHudFrame(input);
    EXPECT_FALSE(safeFrame.weapon.airToAirVisible);
    EXPECT_FALSE(safeFrame.weapon.targetVisible);
    EXPECT_FALSE(safeFrame.weapon.missileCircleVisible);

    input.weapon.simulateMode = true;
    const demo_hud::HudFrame simulateFrame = BuildHudFrame(input);
    EXPECT_TRUE(simulateFrame.weapon.airToAirVisible);
    EXPECT_TRUE(simulateFrame.weapon.targetVisible);
    EXPECT_TRUE(simulateFrame.weapon.missileCircleVisible);

    DemoHudSimulation simulation;
    PilotControls controls;
    controls.masterMode = HudMasterMode::AirToAir;
    controls.weaponMode = HudWeaponMode::AirToAirMissile;
    controls.masterArm = false;
    controls.simulateMode = false;
    controls.throttle = 0.90f;
    simulation.SetControls(controls);

    EXPECT_FALSE(simulation.FireSelectedMissile());
    EXPECT_EQ(simulation.Inventory().aim120c, 4);
}

TEST(DemoHudSimulationTests, ControllerClearsAirToAirReticlesWhenReturningToNav)
{
    demo_hud_ui::DemoHudUi ui;
    ui.Initialize();

    DemoHudController controller;
    HudInputSample missileInput;
    missileInput.weapon.masterMode = HudMasterMode::AirToAir;
    missileInput.weapon.weaponMode = HudWeaponMode::AirToAirMissile;
    missileInput.target.valid = true;

    controller.Populate(ui, missileInput);
    EXPECT_TRUE(ui.HUD().targetDesignator.GetVisible());
    EXPECT_TRUE(ui.HUD().missileDiamond.GetVisible());
    EXPECT_TRUE(ui.HUD().missileCircle.GetVisible());

    HudInputSample navInput;
    navInput.weapon.masterMode = HudMasterMode::Nav;
    navInput.weapon.weaponMode = HudWeaponMode::None;

    controller.Populate(ui, navInput);
    EXPECT_FALSE(ui.HUD().targetDesignator.GetVisible());
    EXPECT_FALSE(ui.HUD().missileDiamond.GetVisible());
    EXPECT_FALSE(ui.HUD().missileCircle.GetVisible());
    EXPECT_FALSE(ui.HUD().dynamicLaunchZone.GetVisible());
    EXPECT_FALSE(ui.HUD().rangeCue.GetVisible());
    EXPECT_FALSE(ui.HUD().breakX.GetVisible());
    EXPECT_FALSE(ui.HUD().attackSteeringCue.GetVisible());
}

TEST(DemoHudSimulationTests, ControllerClearsContextualReticlesWhenChangingModes)
{
    demo_hud_ui::DemoHudUi ui;
    ui.Initialize();

    DemoHudController controller;

    HudInputSample eegsInput;
    eegsInput.weapon.masterMode = HudMasterMode::AirToAir;
    eegsInput.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    eegsInput.weapon.gunMode = HudGunMode::Eegs;
    eegsInput.target.rangeMeters = 900.0f;

    controller.Populate(ui, eegsInput);
    EXPECT_TRUE(ui.HUD().eegsFunnel.GetVisible());
    EXPECT_TRUE(ui.HUD().eegsMrgs.GetVisible());
    ExpectAirToAirMissileReticlesHidden(ui.HUD());
    ExpectAirToGroundReticlesHidden(ui.HUD());

    HudInputSample strafeInput;
    strafeInput.weapon.masterMode = HudMasterMode::AirToGround;
    strafeInput.weapon.weaponMode = HudWeaponMode::AirToGroundStrafe;
    strafeInput.weapon.gunMode = HudGunMode::Strafe;
    strafeInput.airGround.valid = true;
    strafeInput.airGround.slantRangeMeters = 900.0f;
    strafeInput.airGround.pipperDepressionRad = 0.040f;

    controller.Populate(ui, strafeInput);
    ExpectAirToAirMissileReticlesHidden(ui.HUD());
    ExpectAirToAirGunReticlesHidden(ui.HUD());
    EXPECT_TRUE(ui.HUD().strafeReticle.GetVisible());
    EXPECT_TRUE(ui.HUD().strafeInRangeCue.GetVisible());
    EXPECT_TRUE(ui.HUD().strafeBulletTrack.GetVisible());
    EXPECT_FALSE(ui.HUD().ccipPipper.GetVisible());

    HudInputSample ccipInput;
    ccipInput.weapon.masterMode = HudMasterMode::AirToGround;
    ccipInput.weapon.weaponMode = HudWeaponMode::AirToGroundCcip;
    ccipInput.airGround.valid = true;
    ccipInput.airGround.pipperDepressionRad = 0.070f;
    ccipInput.airGround.fallLineAzimuthRad = 0.015f;
    ccipInput.airGround.solutionCueValid = true;
    ccipInput.airGround.solutionCueDepressionRad = 0.090f;
    ccipInput.airGround.pullupAnticipationCueValid = true;
    ccipInput.airGround.pullupAnticipationCueDepressionRad = 0.120f;

    controller.Populate(ui, ccipInput);
    ExpectAirToAirMissileReticlesHidden(ui.HUD());
    ExpectAirToAirGunReticlesHidden(ui.HUD());
    EXPECT_FALSE(ui.HUD().strafeReticle.GetVisible());
    EXPECT_FALSE(ui.HUD().strafeInRangeCue.GetVisible());
    EXPECT_FALSE(ui.HUD().strafeBulletTrack.GetVisible());
    EXPECT_TRUE(ui.HUD().ccipPipper.GetVisible());
    EXPECT_TRUE(ui.HUD().ccipBombFallLine.GetVisible());
    EXPECT_TRUE(ui.HUD().ccipSolutionCue.GetVisible());
    EXPECT_TRUE(ui.HUD().ccipPullupAnticipationCue.GetVisible());

    HudInputSample ilsInput;
    ilsInput.weapon.masterMode = HudMasterMode::Landing;
    ilsInput.approach.landingModeActive = true;
    ilsInput.ils.powered = true;
    ilsInput.ils.selected = true;
    ilsInput.ils.signalValid = true;
    ilsInput.ils.commandSteeringActive = true;
    ilsInput.ils.localizerDeviationDots = 0.7f;
    ilsInput.ils.glideslopeDeviationDots = -0.4f;

    controller.Populate(ui, ilsInput);
    ExpectAirToAirMissileReticlesHidden(ui.HUD());
    ExpectAirToAirGunReticlesHidden(ui.HUD());
    ExpectAirToGroundReticlesHidden(ui.HUD());
    EXPECT_TRUE(ui.HUD().landingPitchLine.GetVisible());
    EXPECT_TRUE(ui.HUD().ilsLocalizerBar.GetVisible());
    EXPECT_TRUE(ui.HUD().ilsGlideslopeBar.GetVisible());
    EXPECT_TRUE(ui.HUD().ilsCommandSteeringCue.GetVisible());

    HudInputSample navInput;
    navInput.weapon.masterMode = HudMasterMode::Nav;
    navInput.weapon.weaponMode = HudWeaponMode::None;

    controller.Populate(ui, navInput);
    ExpectAirToAirMissileReticlesHidden(ui.HUD());
    ExpectAirToAirGunReticlesHidden(ui.HUD());
    ExpectAirToGroundReticlesHidden(ui.HUD());
    ExpectApproachReticlesHidden(ui.HUD());
}

TEST(DemoHudSimulationTests, ControllerPublishesFunnelHideCommandWhenLeavingEegs)
{
    demo_hud_ui::DemoHudUi ui;
    ui.Initialize();

    DemoHudController controller;
    HudInputSample eegsInput;
    eegsInput.weapon.masterMode = HudMasterMode::AirToAir;
    eegsInput.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    eegsInput.weapon.gunMode = HudGunMode::Eegs;
    eegsInput.target.rangeMeters = 900.0f;

    controller.Populate(ui, eegsInput);
    const mfd::CommandBatch eegsBatch = ui.BuildCommandBatch(1U);
    const mfd::UpdateReticleCommand* eegsUpdate = FindReticleUpdate(eegsBatch.commands, "eegsFunnel");
    ASSERT_NE(eegsUpdate, nullptr);
    ASSERT_TRUE(eegsUpdate->patch.visible.has_value());
    EXPECT_TRUE(*eegsUpdate->patch.visible);
    EXPECT_GE(CountPrimitiveVisibilityPatches(*eegsUpdate, true), 2U);

    HudInputSample navInput;
    navInput.weapon.masterMode = HudMasterMode::Nav;
    navInput.weapon.weaponMode = HudWeaponMode::None;

    controller.Populate(ui, navInput);
    const mfd::CommandBatch navBatch = ui.BuildCommandBatch(2U);
    const mfd::UpdateReticleCommand* navUpdate = FindReticleUpdate(navBatch.commands, "eegsFunnel");
    ASSERT_NE(navUpdate, nullptr);
    ASSERT_TRUE(navUpdate->patch.visible.has_value());
    EXPECT_FALSE(*navUpdate->patch.visible);
    EXPECT_GE(CountPrimitiveVisibilityPatches(*navUpdate, false), 2U);
}

TEST(DemoHudSimulationTests, EegsWithoutLockShowsFunnelAndMrgsScaledByWingspan)
{
    HudInputSample narrowTarget;
    narrowTarget.weapon.masterMode = HudMasterMode::AirToAir;
    narrowTarget.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    narrowTarget.weapon.gunMode = HudGunMode::Eegs;
    narrowTarget.weapon.targetLocked = false;
    narrowTarget.weapon.targetWingspanMeters = 8.0f;
    narrowTarget.target.rangeMeters = 900.0f;

    HudInputSample wideTarget = narrowTarget;
    wideTarget.weapon.targetWingspanMeters = 14.0f;

    const demo_hud::HudFrame narrowFrame = BuildHudFrame(narrowTarget);
    const demo_hud::HudFrame wideFrame = BuildHudFrame(wideTarget);

    EXPECT_TRUE(narrowFrame.gun.eegsFunnelVisible);
    EXPECT_TRUE(narrowFrame.gun.mrgsVisible);
    EXPECT_FALSE(narrowFrame.gun.tdCircleVisible);
    EXPECT_GT(
        narrowFrame.gun.eegsFunnelLeftControlPoints.front().y,
        narrowFrame.gun.mrgsPosition.y + 0.12f);
    EXPECT_GT(
        narrowFrame.gun.eegsFunnelRightControlPoints.front().y,
        narrowFrame.gun.mrgsPosition.y + 0.12f);
    EXPECT_GT(wideFrame.gun.eegsFunnelScaleX, narrowFrame.gun.eegsFunnelScaleX);
}

TEST(DemoHudSimulationTests, EegsFunnelRespondsToFlightPathLoadAndTargetDynamics)
{
    HudInputSample stableInput;
    stableInput.weapon.masterMode = HudMasterMode::AirToAir;
    stableInput.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    stableInput.weapon.gunMode = HudGunMode::Eegs;
    stableInput.target.rangeMeters = 900.0f;

    HudInputSample maneuveringInput = stableInput;
    maneuveringInput.aircraft.rollRad = 55.0f * kDegreesToRadians;
    maneuveringInput.aircraft.downSpeedMps = -42.0f;
    maneuveringInput.aircraft.normalLoadFactor = 6.0f;
    maneuveringInput.target.azimuthRad = 5.0f * kDegreesToRadians;
    maneuveringInput.target.elevationRad = -2.0f * kDegreesToRadians;
    maneuveringInput.weapon.targetAccelerationMps2 = 45.0f;

    const demo_hud::HudFrame stableFrame = BuildHudFrame(stableInput);
    const demo_hud::HudFrame maneuveringFrame = BuildHudFrame(maneuveringInput);

    EXPECT_TRUE(maneuveringFrame.gun.eegsFunnelVisible);
    EXPECT_TRUE(IsFiniteHudVec(maneuveringFrame.gun.eegsFunnelPosition));
    EXPECT_TRUE(IsFiniteFunnelRail(maneuveringFrame.gun.eegsFunnelLeftControlPoints));
    EXPECT_TRUE(IsFiniteFunnelRail(maneuveringFrame.gun.eegsFunnelRightControlPoints));
    EXPECT_TRUE(std::isfinite(maneuveringFrame.gun.eegsFunnelRotationDegrees));
    EXPECT_NE(maneuveringFrame.gun.eegsFunnelPosition.x, stableFrame.gun.eegsFunnelPosition.x);
    EXPECT_NE(maneuveringFrame.gun.eegsFunnelPosition.y, stableFrame.gun.eegsFunnelPosition.y);
    EXPECT_NE(maneuveringFrame.gun.eegsFunnelRotationDegrees, stableFrame.gun.eegsFunnelRotationDegrees);
    EXPECT_GT(maneuveringFrame.gun.eegsFunnelScaleY, stableFrame.gun.eegsFunnelScaleY);
    EXPECT_NE(
        maneuveringFrame.gun.eegsFunnelLeftControlPoints.back().x,
        stableFrame.gun.eegsFunnelLeftControlPoints.back().x);
    EXPECT_NE(
        maneuveringFrame.gun.eegsFunnelLeftControlPoints.back().y,
        stableFrame.gun.eegsFunnelLeftControlPoints.back().y);
    EXPECT_LT(
        maneuveringFrame.gun.eegsFunnelLeftControlPoints.front().y,
        maneuveringFrame.gun.eegsFunnelLeftControlPoints.back().y);
    EXPECT_LT(
        HorizontalGap(
            maneuveringFrame.gun.eegsFunnelLeftControlPoints.front(),
            maneuveringFrame.gun.eegsFunnelRightControlPoints.front()),
        HorizontalGap(
            maneuveringFrame.gun.eegsFunnelLeftControlPoints.back(),
            maneuveringFrame.gun.eegsFunnelRightControlPoints.back()));
    EXPECT_GT(
        maneuveringFrame.gun.eegsFunnelLeftControlPoints.front().y,
        maneuveringFrame.gun.mrgsPosition.y + 0.12f);
    EXPECT_GT(
        maneuveringFrame.gun.eegsFunnelRightControlPoints.front().y,
        maneuveringFrame.gun.mrgsPosition.y + 0.12f);
    EXPECT_GT(maneuveringFrame.gun.eegsFunnelLeftControlPoints.front().y, 0.0f);
}

TEST(DemoHudSimulationTests, ControllerPublishesDynamicEegsFunnelBezierRails)
{
    demo_hud_ui::DemoHudUi ui;
    ui.Initialize();

    DemoHudController controller;

    HudInputSample stableInput;
    stableInput.weapon.masterMode = HudMasterMode::AirToAir;
    stableInput.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    stableInput.weapon.gunMode = HudGunMode::Eegs;
    stableInput.target.rangeMeters = 900.0f;

    controller.Populate(ui, stableInput);
    ASSERT_TRUE(ui.HUD().eegsFunnel.GetVisible());
    const std::vector<mfd::Vec2> stableLeft = ui.HUD().eegsFunnel.FunnelLeft().GetControlPoints();
    const std::vector<mfd::Vec2> stableRight = ui.HUD().eegsFunnel.FunnelRight().GetControlPoints();
    ASSERT_EQ(stableLeft.size(), 5U);
    ASSERT_EQ(stableRight.size(), 5U);

    HudInputSample maneuveringInput = stableInput;
    maneuveringInput.aircraft.rollRad = 55.0f * kDegreesToRadians;
    maneuveringInput.aircraft.downSpeedMps = -42.0f;
    maneuveringInput.aircraft.normalLoadFactor = 6.0f;
    maneuveringInput.target.azimuthRad = 5.0f * kDegreesToRadians;
    maneuveringInput.target.elevationRad = -2.0f * kDegreesToRadians;
    maneuveringInput.weapon.targetAccelerationMps2 = 45.0f;

    controller.Populate(ui, maneuveringInput);
    ASSERT_TRUE(ui.HUD().eegsFunnel.GetVisible());
    const std::vector<mfd::Vec2> maneuveringLeft = ui.HUD().eegsFunnel.FunnelLeft().GetControlPoints();
    const std::vector<mfd::Vec2> maneuveringRight = ui.HUD().eegsFunnel.FunnelRight().GetControlPoints();
    ASSERT_EQ(maneuveringLeft.size(), 5U);
    ASSERT_EQ(maneuveringRight.size(), 5U);

    EXPECT_NE(maneuveringLeft.back().x, stableLeft.back().x);
    EXPECT_NE(maneuveringLeft.back().y, stableLeft.back().y);
    EXPECT_NE(maneuveringRight.back().x, stableRight.back().x);
    EXPECT_NE(maneuveringRight.back().y, stableRight.back().y);
    EXPECT_LT(maneuveringLeft.front().y, maneuveringLeft.back().y);
    EXPECT_LT(maneuveringRight.front().y, maneuveringRight.back().y);
    EXPECT_LT(
        HorizontalGap(maneuveringLeft.front(), maneuveringRight.front()),
        HorizontalGap(maneuveringLeft.back(), maneuveringRight.back()));
    EXPECT_GT(maneuveringLeft.front().y, 0.0f);
    EXPECT_GT(maneuveringRight.front().y, 0.0f);
}

TEST(DemoHudSimulationTests, EegsWithLockHidesMrgsAndShowsPippers)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    input.weapon.gunMode = HudGunMode::Eegs;
    input.weapon.targetLocked = true;
    input.target.rangeMeters = 2500.0f;

    const demo_hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_FALSE(frame.gun.mrgsVisible);
    EXPECT_FALSE(frame.gun.eegsFunnelVisible);
    EXPECT_TRUE(frame.gun.tdCircleVisible);
    EXPECT_TRUE(frame.gun.oneGPipperVisible);
    EXPECT_TRUE(frame.gun.maxGPipperVisible);
    EXPECT_TRUE(frame.gun.solutionCircleVisible);
    EXPECT_TRUE(IsFiniteHudVec(frame.gun.oneGPipperPosition));
    EXPECT_TRUE(IsFiniteHudVec(frame.gun.maxGPipperPosition));
}

TEST(DemoHudSimulationTests, EegsTriggerSelectsFedsOrBatrByLockState)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    input.weapon.gunMode = HudGunMode::Eegs;
    input.weapon.triggerHeld = true;

    const demo_hud::HudFrame unlockedFrame = BuildHudFrame(input);
    EXPECT_TRUE(unlockedFrame.gun.fedsVisible);
    EXPECT_FALSE(unlockedFrame.gun.batrVisible);

    input.weapon.targetLocked = true;
    const demo_hud::HudFrame lockedFrame = BuildHudFrame(input);
    EXPECT_FALSE(lockedFrame.gun.fedsVisible);
    EXPECT_TRUE(lockedFrame.gun.batrVisible);
}

TEST(DemoHudSimulationTests, MissileInventoryUsesHudWeaponFamilyLabel)
{
    demo_hud::MissileInventory inventory;
    inventory.aim120c = 4;
    inventory.aim9m = 2;

    EXPECT_EQ(demo_hud::FormatMissileInventory(MissileType::Aim120C, inventory), "AIM-120C 4");
    EXPECT_EQ(demo_hud::FormatMissileInventory(MissileType::Aim9M, inventory), "AIM-9M 2");
}

TEST(DemoHudSimulationTests, StrafeInRangeCueUsesAmmoThreshold)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToGround;
    input.weapon.weaponMode = HudWeaponMode::AirToGroundStrafe;
    input.weapon.gunMode = HudGunMode::Strafe;
    input.weapon.ammoType = demo_hud::HudAmmoType::M56;
    input.airGround.valid = true;
    input.airGround.slantRangeMeters = 3500.0f * 0.3048f;

    const demo_hud::HudFrame inRangeFrame = BuildHudFrame(input);
    EXPECT_TRUE(inRangeFrame.gun.strafeVisible);
    EXPECT_TRUE(inRangeFrame.gun.strafeInRangeCueVisible);

    input.airGround.slantRangeMeters = 5000.0f * 0.3048f;
    const demo_hud::HudFrame outOfRangeFrame = BuildHudFrame(input);
    EXPECT_TRUE(outOfRangeFrame.gun.strafeVisible);
    EXPECT_FALSE(outOfRangeFrame.gun.strafeInRangeCueVisible);
}

TEST(DemoHudSimulationTests, CcipPipperBombFallLineAndSolutionCueRemainFinite)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToGround;
    input.weapon.weaponMode = HudWeaponMode::AirToGroundCcip;
    input.airGround.valid = true;
    input.airGround.pipperAzimuthRad = 2.0f * kDegreesToRadians;
    input.airGround.pipperDepressionRad = 6.0f * kDegreesToRadians;
    input.airGround.fallLineAzimuthRad = 1.0f * kDegreesToRadians;
    input.airGround.solutionCueValid = true;
    input.airGround.solutionCueDepressionRad = 3.0f * kDegreesToRadians;
    input.airGround.pullupAnticipationCueValid = true;
    input.airGround.pullupAnticipationCueDepressionRad = 1.0f * kDegreesToRadians;

    const demo_hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_TRUE(frame.airGround.ccipVisible);
    EXPECT_TRUE(frame.airGround.solutionCueVisible);
    EXPECT_TRUE(frame.airGround.pullupAnticipationCueVisible);
    EXPECT_TRUE(IsFiniteHudVec(frame.airGround.ccipPipperPosition));
    EXPECT_TRUE(IsFiniteHudVec(frame.airGround.solutionCuePosition));
    EXPECT_TRUE(std::isfinite(frame.airGround.bombFallLineX));
}

TEST(DemoHudSimulationTests, LandingGearForcesCasAndFineAltitudeScale)
{
    HudInputSample input;
    input.approach.landingGearDown = true;
    input.approach.landingModeActive = true;

    const demo_hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_TRUE(frame.approach.landingVisible);
    EXPECT_TRUE(frame.approach.forceCalibratedAirspeed);
    EXPECT_TRUE(frame.approach.fineAltitudeScale);
    EXPECT_TRUE(frame.approach.minusTwoPointFivePitchLineVisible);
    EXPECT_TRUE(IsFiniteHudVec(frame.approach.minusTwoPointFivePitchLinePosition));
}

TEST(DemoHudSimulationTests, LandingDeclutterMasksIlsAndRollIndicator)
{
    HudInputSample input;
    input.approach.landingModeActive = true;
    input.approach.landingDeclutterActive = true;
    input.ils.powered = true;
    input.ils.selected = true;
    input.ils.signalValid = true;
    input.ils.commandSteeringActive = true;

    const demo_hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_TRUE(frame.approach.declutterActive);
    EXPECT_FALSE(frame.approach.rollIndicatorVisible);
    EXPECT_FALSE(frame.ils.barsVisible);
    EXPECT_FALSE(frame.ils.commandSteeringVisible);
}

TEST(DemoHudSimulationTests, IlsVisibilityAndDeviationClampAreFinite)
{
    HudInputSample input;
    input.ils.powered = true;
    input.ils.selected = true;
    input.ils.signalValid = true;
    input.ils.commandSteeringActive = true;
    input.ils.localizerDeviationDots = std::numeric_limits<float>::infinity();
    input.ils.glideslopeDeviationDots = -8.0f;

    const demo_hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_TRUE(frame.ils.barsVisible);
    EXPECT_TRUE(frame.ils.commandSteeringVisible);
    EXPECT_FLOAT_EQ(frame.ils.localizerDeviationDots, 0.0f);
    EXPECT_FLOAT_EQ(frame.ils.glideslopeDeviationDots, -2.5f);
    EXPECT_TRUE(IsFiniteHudVec(frame.ils.localizerBarPosition));
    EXPECT_TRUE(IsFiniteHudVec(frame.ils.glideslopeBarPosition));

    input.ils.powered = false;
    const demo_hud::HudFrame offFrame = BuildHudFrame(input);
    EXPECT_FALSE(offFrame.ils.barsVisible);
}

TEST(DemoHudSimulationTests, WSteeringCueIsElevenMilsBelowBoresightWhenFpmUnavailable)
{
    HudInputSample input;
    input.approach.flightPathMarkerAvailable = false;
    input.ils.powered = true;
    input.ils.selected = true;
    input.ils.signalValid = true;

    const demo_hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_TRUE(frame.ils.wSteeringVisible);
    EXPECT_NEAR(frame.ils.wSteeringPosition.x, 0.0f, 1.0e-5f);
    EXPECT_NEAR(frame.ils.wSteeringPosition.y, 0.8384f, 1.0e-4f);
}

TEST(DemoHudSimulationTests, ImpactMissileIsRetainedBrieflyThenRemoved)
{
    DemoHudSimulation simulation;
    PilotControls controls;
    controls.masterMode = HudMasterMode::AirToAir;
    simulation.SetControls(controls);

    ASSERT_TRUE(simulation.FireSelectedMissile());
    ASSERT_EQ(simulation.MissileShots().size(), 1U);

    const int stepsUntilRetentionExpired =
        static_cast<int>(std::ceil((simulation.MissileShots().front().timeRemainingSeconds + 8.4f) / 0.080f));
    StepMany(simulation, controls, stepsUntilRetentionExpired);

    EXPECT_TRUE(simulation.MissileShots().empty());
}
} // namespace
