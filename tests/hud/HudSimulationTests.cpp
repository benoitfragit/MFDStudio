/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Regression tests for the HUD simulation rules.
 */

#include "HudUi.h"
#include "hud/HudController.h"
#include "hud_main/HudSimulation.h"
#include "hud_main/HudSimulationTime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

namespace
{
using hud::BuildAircraftStateForHud;
using hud::BuildHudFrame;
using hud::HudConformalProjection;
using hud::HudController;
using hud::HudGunMode;
using hud::HudInputSample;
using hud::HudMasterMode;
using hud::HudPitchLadderVerticalFovDegrees;
using hud::HudWeaponMode;
using hud::ProjectBoresightAngularOffsetToHud;
using hud::ProjectPitchOffsetToHud;
using hud_main::ComputeLaunchZone;
using hud_main::ComputeMissileTimeOfFlight;
using hud_main::HudSimulation;
using hud_main::MissileLabel;
using hud_main::MissileType;
using hud_main::SimulationControls;

constexpr float kDegreesToRadians = 0.017453292519943295f;
constexpr float kFeetToMeters = 0.3048f;
constexpr float kGunBoreCrossHudY = 0.82f;
constexpr float kHudUnitsPerMil = 0.0056f;
constexpr float kFunnelNearRangeFeet = 600.0f;
constexpr float kFunnelFarRangeFeet = 3000.0f;

// Locates the repository root from this test's own path so asset-consistency
// tests do not depend on the runtime working directory.
std::filesystem::path RepositoryRoot()
{
    const std::filesystem::path testFile = std::filesystem::path(__FILE__).lexically_normal();
    return testFile.parent_path().parent_path().parent_path();
}

std::string ReadFileText(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

// Extracts the vertical coordinate of a pitch-ladder bar's `start` point from the
// authored reticle JSON. The layout is generated and stable, so a small explicit
// scan keeps the test free of a JSON dependency.
float ReadLadderBarStartY(const std::string& content, const std::string& elementId)
{
    const std::string key = "\"id\": \"" + elementId + "\"";
    const std::size_t idPosition = content.find(key);
    if (idPosition == std::string::npos)
    {
        return std::numeric_limits<float>::quiet_NaN();
    }

    const std::size_t startPosition = content.find("\"start\"", idPosition);
    const std::size_t openBracket = content.find('[', startPosition);
    const std::size_t comma = content.find(',', openBracket);
    const std::size_t closeBracket = content.find(']', comma);
    if (comma == std::string::npos || closeBracket == std::string::npos)
    {
        return std::numeric_limits<float>::quiet_NaN();
    }

    return std::stof(content.substr(comma + 1, closeBracket - comma - 1));
}

void StepMany(HudSimulation& simulation, const SimulationControls& controls, const int steps)
{
    simulation.SetSimulationControls(controls);
    for (int index = 0; index < steps; ++index)
    {
        for (int tick = 0; tick < 4; ++tick)
        {
            simulation.Step();
        }
    }
}

void PopulateStraightGunTrajectory(HudInputSample& input)
{
    for (std::size_t index = 0U; index < input.gunTrajectory.points.size(); ++index)
    {
        const float fraction = static_cast<float>(index) /
            static_cast<float>(input.gunTrajectory.points.size() - 1U);
        const float rangeFeet =
            kFunnelNearRangeFeet + (kFunnelFarRangeFeet - kFunnelNearRangeFeet) * fraction;
        hud::GunTrajectoryPointNed& point = input.gunTrajectory.points[index];
        point.northMeters = rangeFeet * kFeetToMeters;
        point.eastMeters = 0.0f;
        point.downMeters = 1.0f + 18.0f * fraction * fraction;
        point.ageSeconds = 0.18f + 0.80f * fraction;
        point.valid = true;
    }
}

bool IsFiniteHudVec(const hud::HudVec2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool IsFiniteFunnelRail(const hud::HudFunnelControlPoints& rail)
{
    for (const hud::HudVec2 point : rail)
    {
        if (!IsFiniteHudVec(point))
        {
            return false;
        }
    }

    return true;
}

float HorizontalGap(const hud::HudVec2 left, const hud::HudVec2 right)
{
    return std::fabs(right.x - left.x);
}

float HorizontalGap(const mfd::Vec2 left, const mfd::Vec2 right)
{
    return std::fabs(right.x - left.x);
}

float HorizontalGapAt(const hud::HudFunnelControlPoints& leftRail,
                      const hud::HudFunnelControlPoints& rightRail,
                      const std::size_t index)
{
    return HorizontalGap(leftRail[index], rightRail[index]);
}

float HorizontalGapAt(const std::vector<mfd::Vec2>& leftRail,
                      const std::vector<mfd::Vec2>& rightRail,
                      const std::size_t index)
{
    return HorizontalGap(leftRail[index], rightRail[index]);
}

float ProjectileSlantRangeMeters(const hud::GunTrajectoryPointNed& point)
{
    return std::sqrt(
        point.northMeters * point.northMeters +
        point.eastMeters * point.eastMeters +
        point.downMeters * point.downMeters);
}

float ProjectileBodyAzimuthRad(const hud::GunTrajectoryPointNed& point, const double aircraftYawRad)
{
    const double cosineYaw = std::cos(aircraftYawRad);
    const double sineYaw = std::sin(aircraftYawRad);
    const double forward = cosineYaw * static_cast<double>(point.northMeters) +
                           sineYaw * static_cast<double>(point.eastMeters);
    const double right = -sineYaw * static_cast<double>(point.northMeters) +
                         cosineYaw * static_cast<double>(point.eastMeters);
    return static_cast<float>(std::atan2(right, forward));
}

void ExpectAirToAirMissileReticlesHidden(hud_ui::HUDMockupPage& hud)
{
    EXPECT_FALSE(hud.targetDesignator.GetVisible());
    EXPECT_FALSE(hud.missileDiamond.GetVisible());
    EXPECT_FALSE(hud.missileCircle.GetVisible());
    EXPECT_FALSE(hud.dynamicLaunchZone.GetVisible());
    EXPECT_FALSE(hud.rangeCue.GetVisible());
    EXPECT_FALSE(hud.breakX.GetVisible());
    EXPECT_FALSE(hud.attackSteeringCue.GetVisible());
}

void ExpectAirToAirGunReticlesHidden(hud_ui::HUDMockupPage& hud)
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

void ExpectAirToGroundReticlesHidden(hud_ui::HUDMockupPage& hud)
{
    EXPECT_FALSE(hud.strafeReticle.GetVisible());
    EXPECT_FALSE(hud.strafeInRangeCue.GetVisible());
    EXPECT_FALSE(hud.strafeBulletTrack.GetVisible());
    EXPECT_FALSE(hud.ccipPipper.GetVisible());
    EXPECT_FALSE(hud.ccipBombFallLine.GetVisible());
    EXPECT_FALSE(hud.ccipSolutionCue.GetVisible());
    EXPECT_FALSE(hud.ccipPullupAnticipationCue.GetVisible());
}

void ExpectApproachReticlesHidden(hud_ui::HUDMockupPage& hud)
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

TEST(HudSimulationTests, UsesOneAuthoritativeFixedTick)
{
    HudSimulation simulation;
    simulation.Step();
    EXPECT_FLOAT_EQ(
        simulation.Inputs().aircraft.elapsedSeconds,
        static_cast<float>(hud_main::kHudSimulationStepSeconds));
    for (int tick = 1; tick < 10; ++tick)
    {
        simulation.Step();
    }
    EXPECT_FLOAT_EQ(simulation.Inputs().aircraft.elapsedSeconds, 0.2f);
}

TEST(HudSimulationTests, LoopingAttitudeKeepsHudPitchBoundedAndMarksInversion)
{
    HudSimulation simulation;
    SimulationControls controls;
    controls.pilot.pitchCommand = 1.0f;
    controls.pilot.throttle = 0.95f;
    controls.pilot.afterburnerRequested = true;

    StepMany(simulation, controls, 35);

    const hud::HudFrame frame = simulation.BuildHudFrame();
    EXPECT_LE(std::fabs(frame.attitude.displayPitchDegrees), 90.0f);
    EXPECT_TRUE(frame.attitude.inverted);
    EXPECT_TRUE(frame.attitude.ghostHorizonVisible || frame.attitude.trueHorizonVisible);
}

TEST(HudSimulationTests, LadderPivotsAboutHudCenterUnderBank)
{
    HudSimulation simulation;
    SimulationControls controls;
    controls.pilot.pitchCommand = 0.4f;
    controls.pilot.rollCommand = 1.0f;
    controls.pilot.throttle = 0.85f;

    StepMany(simulation, controls, 9);

    const hud::HudFrame frame = simulation.BuildHudFrame();
    // Preconditions: a genuine climbing bank so the pivot difference is observable.
    ASSERT_GT(frame.attitude.displayPitchDegrees, 4.0f);
    ASSERT_GT(std::fabs(frame.attitude.displayRollDegrees), 25.0f);
    ASSERT_LT(std::fabs(frame.attitude.displayPitchDegrees), 90.0f);

    // Un-rotating the ladder position by the bank angle must recover a pure vertical
    // pitch offset (no lateral component) below the horizon center: proof the ladder
    // pivots about the HUD center, not the moving horizon point.
    const hud::HudVec2 pos = frame.attitude.ladderPosition;
    const float bankRadians = frame.attitude.displayRollDegrees * kDegreesToRadians;
    const float unrotatedX = pos.x * std::cos(bankRadians) - pos.y * std::sin(bankRadians);
    const float unrotatedY = pos.x * std::sin(bankRadians) + pos.y * std::cos(bankRadians);

    EXPECT_NEAR(unrotatedX, 0.0f, 1.0e-4f);
    EXPECT_LT(unrotatedY, 0.0f);
    // Under bank the world-space ladder offset genuinely gains a lateral component
    // (this is exactly what the previous {0, horizonRawY} placement got wrong).
    EXPECT_GT(std::fabs(pos.x), 1.0e-3f);
}

TEST(HudSimulationTests, LevelRecoveryDoesNotStayVisibleAfterPureRoll)
{
    HudInputSample input;
    input.aircraft.rollRad = 170.0f * kDegreesToRadians;
    input.aircraft.pitchRad = 0.0f;
    input.aircraft.radioAltitudeMeters = 2000.0f;
    input.aircraft.downSpeedMps = 0.0f;

    const hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_FALSE(frame.attitude.inverted);
    EXPECT_FALSE(frame.attitude.levelRecoveryVisible);
}

TEST(HudSimulationTests, FullLoopKeepsAttitudeSymbologyFinite)
{
    HudSimulation simulation;
    SimulationControls controls;
    controls.pilot.pitchCommand = 1.0f;
    controls.pilot.throttle = 0.95f;
    controls.pilot.afterburnerRequested = true;
    simulation.SetSimulationControls(controls);

    for (int index = 0; index < 140; ++index)
    {
        for (int tick = 0; tick < 4; ++tick)
        {
            simulation.Step();
        }
        const hud::HudFrame frame = simulation.BuildHudFrame();
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

TEST(HudSimulationTests, PitchCommandIsSmoothedAcrossSeveralFrames)
{
    HudSimulation simulation;
    SimulationControls controls;
    controls.pilot.pitchCommand = 1.0f;
    simulation.SetSimulationControls(controls);

    const float initialPitch = simulation.Aircraft().pitchDegrees;
    for (int tick = 0; tick < 4; ++tick)
    {
        simulation.Step();
    }
    const float firstStepPitch = simulation.Aircraft().pitchDegrees;

    StepMany(simulation, controls, 8);

    EXPECT_LT(firstStepPitch - initialPitch, 0.9f);
    EXPECT_GT(simulation.Aircraft().pitchDegrees - firstStepPitch, 8.0f);
}

TEST(HudSimulationTests, RadarAltitudeIsBlankedAtHighAltitudeForLargeAttitude)
{
    HudSimulation simulation;
    SimulationControls controls;
    controls.pilot.pitchCommand = 1.0f;

    StepMany(simulation, controls, 12);

    const hud::HudFrame frame = simulation.BuildHudFrame();
    ASSERT_GT(simulation.Aircraft().radarAltitudeFeet, 5000.0f);
    EXPECT_FALSE(frame.airData.radarAltitudeVisible);
}

TEST(HudSimulationTests, LaunchZoneRemainsOrderedAndDetectsNoEscapeZone)
{
    HudSimulation simulation;
    const hud::LaunchZone zone =
        ComputeLaunchZone(simulation.Inputs().aircraft, simulation.Inputs().target, MissileType::Aim120C);
    const hud::TargetState target = simulation.Target();

    EXPECT_GT(zone.rmax1Nm, zone.rmax2Nm);
    EXPECT_GT(zone.rmax2Nm, zone.rmin2Nm);
    EXPECT_GT(zone.rmin2Nm, zone.rmin1Nm);
    EXPECT_EQ(zone.inNoEscapeZone, target.rangeNm >= zone.rmin2Nm && target.rangeNm <= zone.rmax2Nm);
    EXPECT_FALSE(zone.tooClose);

    // The simulation must publish the same resolved launch zone through the
    // generic weapon contract consumed by the HUD runtime.
    EXPECT_FLOAT_EQ(simulation.Inputs().weapon.launchZone.rmax1Nm, zone.rmax1Nm);
    EXPECT_FLOAT_EQ(simulation.Inputs().weapon.launchZone.rmin1Nm, zone.rmin1Nm);
}

TEST(HudSimulationTests, SimulationPublishesResolvedWeaponPresentation)
{
    // The reusable HUD runtime consumes only generic weapon facts; verify the
    // sample simulation resolves its typed armament into that contract.
    HudSimulation simulation;
    EXPECT_EQ(simulation.Inputs().weapon.selectedWeaponLabel, "AIM-120C");
    EXPECT_EQ(simulation.Inputs().weapon.selectedWeaponMnemonic, "MRM");
    EXPECT_EQ(simulation.Inputs().weapon.selectedWeaponQuantity, 4);
    EXPECT_GT(simulation.Inputs().weapon.selectedMissileTimeOfFlightSeconds, 0.0f);

    simulation.SelectMissile(MissileType::Aim9M);
    EXPECT_EQ(simulation.Inputs().weapon.selectedWeaponLabel, "AIM-9M");
    EXPECT_EQ(simulation.Inputs().weapon.selectedWeaponMnemonic, "SRM");
    EXPECT_EQ(simulation.Inputs().weapon.selectedWeaponQuantity, 2);
}

// Expected sample-profile facts checked by the DLZ/time-of-flight regression test.
struct MissileProfileExpectation
{
    MissileType missile;
    const char* label;
    const char* mnemonic;
    float minTimeOfFlightSeconds;
    float maxTimeOfFlightSeconds;
};

TEST(HudSimulationTests, SampleMissileProfilesKeepDlzOrderingAndTimeOfFlightBounds)
{
    // The AIM-120C/AIM-9M tuning lives in typed main-client profiles; verify
    // each profile still yields an ordered DLZ and a bounded time of flight
    // across a range of engagement geometries. Bounds match the profile limits.
    const MissileProfileExpectation expectations[] = {
        {MissileType::Aim120C, "AIM-120C", "MRM", 3.0f, 68.0f},
        {MissileType::Aim9M, "AIM-9M", "SRM", 3.0f, 28.0f}};

    for (const MissileProfileExpectation& expectation : expectations)
    {
        EXPECT_STREQ(MissileLabel(expectation.missile), expectation.label);
        EXPECT_STREQ(hud_main::MissileMnemonic(expectation.missile), expectation.mnemonic);

        for (float altitudeMeters = 0.0f; altitudeMeters <= 12000.0f; altitudeMeters += 4000.0f)
        {
            for (float rangeMeters = 800.0f; rangeMeters <= 55000.0f; rangeMeters += 9000.0f)
            {
                HudInputSample input;
                input.aircraft.altitudeMeters = altitudeMeters;
                input.aircraft.northSpeedMps = 240.0f;
                input.aircraft.specificEnergyRateMps = 15.0f;
                input.target.rangeMeters = rangeMeters;
                input.target.closingSpeedMps = 200.0f;

                const hud::LaunchZone zone =
                    ComputeLaunchZone(input.aircraft, input.target, expectation.missile);
                EXPECT_GT(zone.rmax1Nm, zone.rmax2Nm);
                EXPECT_GT(zone.rmax2Nm, zone.rmin2Nm);
                EXPECT_GT(zone.rmin2Nm, zone.rmin1Nm);

                const float timeOfFlight =
                    ComputeMissileTimeOfFlight(input.aircraft, input.target, expectation.missile);
                EXPECT_TRUE(std::isfinite(timeOfFlight));
                EXPECT_GE(timeOfFlight, expectation.minTimeOfFlightSeconds);
                EXPECT_LE(timeOfFlight, expectation.maxTimeOfFlightSeconds);
            }
        }
    }
}

TEST(HudSimulationTests, ExternalSiInputsDriveHudFrameWithoutPanelControls)
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

    const hud::AircraftState display = BuildAircraftStateForHud(input.aircraft);
    EXPECT_NEAR(display.altitudeFeet, 9842.52f, 0.5f);
    EXPECT_NEAR(display.radarAltitudeFeet, 721.78f, 0.5f);
    EXPECT_NEAR(display.headingDegrees, 91.0f, 0.1f);
    EXPECT_GT(display.flightPathAngleDegrees, 4.0f);

    const hud::HudFrame frame = BuildHudFrame(input);
    EXPECT_TRUE(IsFiniteHudVec(frame.attitude.fpmPosition));
    EXPECT_LT(frame.attitude.fpmPosition.y, 0.05f);
    EXPECT_TRUE(frame.airData.energyChevronUpVisible);
}

TEST(HudSimulationTests, AirToAirLaunchConsumesInventoryAndCountsDown)
{
    HudSimulation simulation;
    SimulationControls controls;
    controls.pilot.masterMode = HudMasterMode::AirToAir;
    controls.pilot.throttle = 0.90f;
    simulation.SetSimulationControls(controls);
    simulation.SelectMissile(MissileType::Aim120C);

    ASSERT_TRUE(simulation.FireSelectedMissile());
    ASSERT_EQ(simulation.Inventory().aim120c, 3);
    ASSERT_EQ(simulation.MissileShots().size(), 1U);

    const float initialRemaining = simulation.MissileShots().front().timeRemainingSeconds;
    simulation.Step();

    ASSERT_EQ(simulation.MissileShots().size(), 1U);
    EXPECT_LT(simulation.MissileShots().front().timeRemainingSeconds, initialRemaining);
}

TEST(HudSimulationTests, MasterArmGatesAirToAirMissileSymbologyAndLaunch)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirMissile;
    input.weapon.masterArm = false;
    input.weapon.simulateMode = false;
    input.target.valid = true;

    const hud::HudFrame safeFrame = BuildHudFrame(input);
    EXPECT_FALSE(safeFrame.weapon.airToAirVisible);
    EXPECT_FALSE(safeFrame.weapon.targetVisible);
    EXPECT_FALSE(safeFrame.weapon.missileCircleVisible);

    input.weapon.simulateMode = true;
    const hud::HudFrame simulateFrame = BuildHudFrame(input);
    EXPECT_TRUE(simulateFrame.weapon.airToAirVisible);
    EXPECT_TRUE(simulateFrame.weapon.targetVisible);
    EXPECT_TRUE(simulateFrame.weapon.missileCircleVisible);

    HudSimulation simulation;
    SimulationControls controls;
    controls.pilot.masterMode = HudMasterMode::AirToAir;
    controls.pilot.weaponMode = HudWeaponMode::AirToAirMissile;
    controls.pilot.masterArm = false;
    controls.pilot.simulateMode = false;
    controls.pilot.throttle = 0.90f;
    simulation.SetSimulationControls(controls);

    EXPECT_FALSE(simulation.FireSelectedMissile());
    EXPECT_EQ(simulation.Inventory().aim120c, 4);
}

TEST(HudSimulationTests, ControllerClearsAirToAirReticlesWhenReturningToNav)
{
    hud_ui::HudUi ui;
    ui.Initialize();

    HudController controller;
    HudInputSample missileInput;
    missileInput.weapon.masterMode = HudMasterMode::AirToAir;
    missileInput.weapon.weaponMode = HudWeaponMode::AirToAirMissile;
    missileInput.weapon.masterArm = true;
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

TEST(HudSimulationTests, ControllerClearsContextualReticlesWhenChangingModes)
{
    hud_ui::HudUi ui;
    ui.Initialize();

    HudController controller;

    HudInputSample eegsInput;
    eegsInput.weapon.masterMode = HudMasterMode::AirToAir;
    eegsInput.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    eegsInput.weapon.gunMode = HudGunMode::Eegs;
    eegsInput.weapon.masterArm = true;
    eegsInput.weapon.gunRoundsRemaining = 510;
    eegsInput.target.rangeMeters = 900.0f;
    PopulateStraightGunTrajectory(eegsInput);

    controller.Populate(ui, eegsInput);
    EXPECT_TRUE(ui.HUD().eegsFunnel.GetVisible());
    EXPECT_TRUE(ui.HUD().eegsMrgs.GetVisible());
    ExpectAirToAirMissileReticlesHidden(ui.HUD());
    ExpectAirToGroundReticlesHidden(ui.HUD());

    HudInputSample strafeInput;
    strafeInput.weapon.masterMode = HudMasterMode::AirToGround;
    strafeInput.weapon.weaponMode = HudWeaponMode::AirToGroundStrafe;
    strafeInput.weapon.gunMode = HudGunMode::Strafe;
    strafeInput.weapon.masterArm = true;
    strafeInput.weapon.gunRoundsRemaining = 510;
    strafeInput.weapon.strafeInRangeFeet = 12000.0f;
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
    ccipInput.weapon.masterArm = true;
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

TEST(HudSimulationTests, ControllerPublishesFunnelHideCommandWhenLeavingEegs)
{
    hud_ui::HudUi ui;
    ui.Initialize();

    HudController controller;
    HudInputSample eegsInput;
    eegsInput.weapon.masterMode = HudMasterMode::AirToAir;
    eegsInput.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    eegsInput.weapon.gunMode = HudGunMode::Eegs;
    eegsInput.weapon.masterArm = true;
    eegsInput.weapon.gunRoundsRemaining = 510;
    eegsInput.target.rangeMeters = 900.0f;
    PopulateStraightGunTrajectory(eegsInput);

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

TEST(HudSimulationTests, EegsWithoutLockShowsFunnelAndMrgsScaledByWingspan)
{
    HudInputSample narrowTarget;
    narrowTarget.weapon.masterMode = HudMasterMode::AirToAir;
    narrowTarget.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    narrowTarget.weapon.gunMode = HudGunMode::Eegs;
    narrowTarget.weapon.masterArm = true;
    narrowTarget.weapon.gunRoundsRemaining = 510;
    narrowTarget.weapon.targetLocked = false;
    narrowTarget.weapon.targetWingspanMeters = 8.0f;
    narrowTarget.target.rangeMeters = 900.0f;
    PopulateStraightGunTrajectory(narrowTarget);

    HudInputSample wideTarget = narrowTarget;
    wideTarget.weapon.targetWingspanMeters = 14.0f;

    const hud::HudFrame narrowFrame = BuildHudFrame(narrowTarget);
    const hud::HudFrame wideFrame = BuildHudFrame(wideTarget);

    EXPECT_TRUE(narrowFrame.gun.eegsFunnelVisible);
    EXPECT_TRUE(narrowFrame.gun.mrgsVisible);
    EXPECT_FALSE(narrowFrame.gun.tdCircleVisible);
    EXPECT_GT(
        HorizontalGapAt(
            wideFrame.gun.eegsFunnelLeftControlPoints,
            wideFrame.gun.eegsFunnelRightControlPoints,
            0U),
        HorizontalGapAt(
            narrowFrame.gun.eegsFunnelLeftControlPoints,
            narrowFrame.gun.eegsFunnelRightControlPoints,
            0U));
}

TEST(HudSimulationTests, EegsFunnelNarrowsFromSixHundredToThreeThousandFeet)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    input.weapon.gunMode = HudGunMode::Eegs;
    input.weapon.masterArm = true;
    input.weapon.gunRoundsRemaining = 510;
    input.target.rangeMeters = 900.0f;
    PopulateStraightGunTrajectory(input);

    const hud::HudFrame frame = BuildHudFrame(input);
    const hud::HudFunnelControlPoints& left = frame.gun.eegsFunnelLeftControlPoints;
    const hud::HudFunnelControlPoints& right = frame.gun.eegsFunnelRightControlPoints;
    ASSERT_EQ(left.size(), right.size());

    const float nearGap = HorizontalGapAt(left, right, 0U);
    const float farGap = HorizontalGapAt(left, right, left.size() - 1U);
    ASSERT_GT(farGap, 0.0f);
    EXPECT_GT(nearGap, farGap);
    EXPECT_NEAR(nearGap / farGap, 5.0f, 0.05f);
    EXPECT_TRUE(IsFiniteFunnelRail(left));
    EXPECT_TRUE(IsFiniteFunnelRail(right));
}

TEST(HudSimulationTests, EegsFunnelUsesGunBoreReferenceAndDoesNotFollowFpm)
{
    HudInputSample levelInput;
    levelInput.weapon.masterMode = HudMasterMode::AirToAir;
    levelInput.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    levelInput.weapon.gunMode = HudGunMode::Eegs;
    levelInput.weapon.masterArm = true;
    levelInput.weapon.gunRoundsRemaining = 510;
    levelInput.aircraft.northSpeedMps = 230.0f;
    PopulateStraightGunTrajectory(levelInput);

    HudInputSample descendingInput = levelInput;
    descendingInput.aircraft.downSpeedMps = 60.0f;

    const hud::HudFrame levelFrame = BuildHudFrame(levelInput);
    const hud::HudFrame descendingFrame = BuildHudFrame(descendingInput);

    EXPECT_NE(levelFrame.attitude.fpmPosition.y, descendingFrame.attitude.fpmPosition.y);
    EXPECT_FLOAT_EQ(levelFrame.gun.eegsFunnelPosition.x, 0.0f);
    EXPECT_FLOAT_EQ(levelFrame.gun.eegsFunnelPosition.y, kGunBoreCrossHudY);
    EXPECT_FLOAT_EQ(
        levelFrame.gun.eegsFunnelPosition.y,
        descendingFrame.gun.eegsFunnelPosition.y);
    for (std::size_t index = 0U; index < levelFrame.gun.eegsFunnelLeftControlPoints.size(); ++index)
    {
        EXPECT_FLOAT_EQ(
            levelFrame.gun.eegsFunnelLeftControlPoints[index].x,
            descendingFrame.gun.eegsFunnelLeftControlPoints[index].x);
        EXPECT_FLOAT_EQ(
            levelFrame.gun.eegsFunnelLeftControlPoints[index].y,
            descendingFrame.gun.eegsFunnelLeftControlPoints[index].y);
        EXPECT_FLOAT_EQ(
            levelFrame.gun.eegsFunnelRightControlPoints[index].x,
            descendingFrame.gun.eegsFunnelRightControlPoints[index].x);
        EXPECT_FLOAT_EQ(
            levelFrame.gun.eegsFunnelRightControlPoints[index].y,
            descendingFrame.gun.eegsFunnelRightControlPoints[index].y);
    }
}

TEST(HudSimulationTests, EegsFunnelHidesWithoutAValidBallisticSolution)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    input.weapon.gunMode = HudGunMode::Eegs;
    input.weapon.masterArm = true;
    input.weapon.gunRoundsRemaining = 510;

    EXPECT_FALSE(BuildHudFrame(input).gun.eegsFunnelVisible);
}

TEST(HudSimulationTests, EegsFixedRangeSamplingKeepsOpeningStableDuringStraightFlight)
{
    HudSimulation simulation;
    SimulationControls controls;
    controls.pilot.masterMode = HudMasterMode::AirToAir;
    controls.pilot.weaponMode = HudWeaponMode::AirToAirGun;
    controls.pilot.gunMode = HudGunMode::Eegs;
    simulation.SetSimulationControls(controls);

    float minimumWideGap = std::numeric_limits<float>::max();
    float maximumWideGap = 0.0f;
    for (std::size_t tick = 0U; tick < 20U; ++tick)
    {
        simulation.Step();
        const hud::HudFrame frame = simulation.BuildHudFrame();
        ASSERT_TRUE(frame.gun.eegsFunnelVisible);
        const hud::HudVec2 leftWide = frame.gun.eegsFunnelLeftControlPoints.front();
        const hud::HudVec2 rightWide = frame.gun.eegsFunnelRightControlPoints.front();
        const float wideGap = std::hypot(rightWide.x - leftWide.x, rightWide.y - leftWide.y);
        minimumWideGap = std::min(minimumWideGap, wideGap);
        maximumWideGap = std::max(maximumWideGap, wideGap);
    }

    EXPECT_LT(maximumWideGap - minimumWideGap, 0.0001f);
}

TEST(HudSimulationTests, EegsRailSidesStayStableWhenProjectedTrajectoryReversesVertically)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    input.weapon.gunMode = HudGunMode::Eegs;
    input.weapon.masterArm = true;
    input.weapon.gunRoundsRemaining = 510;
    PopulateStraightGunTrajectory(input);
    for (std::size_t index = 0U; index < input.gunTrajectory.points.size(); ++index)
    {
        input.gunTrajectory.points[index].downMeters = 60.0f - static_cast<float>(index) * 8.0f;
    }

    const hud::HudFrame frame = BuildHudFrame(input);
    ASSERT_TRUE(frame.gun.eegsFunnelVisible);
    for (std::size_t index = 0U; index < frame.gun.eegsFunnelLeftControlPoints.size(); ++index)
    {
        EXPECT_LT(
            frame.gun.eegsFunnelLeftControlPoints[index].x,
            frame.gun.eegsFunnelRightControlPoints[index].x);
    }
}

TEST(HudSimulationTests, EegsBezierEndpointsFollowNearAndFarRangeStations)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    input.weapon.gunMode = HudGunMode::Eegs;
    input.weapon.masterArm = true;
    input.weapon.gunRoundsRemaining = 510;
    PopulateStraightGunTrajectory(input);

    const hud::HudFrame frame = BuildHudFrame(input);
    ASSERT_TRUE(frame.gun.eegsFunnelVisible);
    const hud::HudVec2 leftNear = frame.gun.eegsFunnelLeftControlPoints.front();
    const hud::HudVec2 rightNear = frame.gun.eegsFunnelRightControlPoints.front();
    const hud::HudVec2 leftFar = frame.gun.eegsFunnelLeftControlPoints.back();
    const hud::HudVec2 rightFar = frame.gun.eegsFunnelRightControlPoints.back();
    const hud::GunTrajectoryPointNed& nearest = input.gunTrajectory.points.front();
    const hud::GunTrajectoryPointNed& farthest = input.gunTrajectory.points.back();
    const hud::ProjectedHudPoint expectedNear = ProjectBoresightAngularOffsetToHud(
        std::atan2(nearest.eastMeters, nearest.northMeters),
        std::atan2(-nearest.downMeters, std::hypot(nearest.northMeters, nearest.eastMeters)));
    const hud::ProjectedHudPoint expectedFar = ProjectBoresightAngularOffsetToHud(
        std::atan2(farthest.eastMeters, farthest.northMeters),
        std::atan2(-farthest.downMeters, std::hypot(farthest.northMeters, farthest.eastMeters)));
    const hud::HudVec2 farCenter {
        (leftFar.x + rightFar.x) * 0.5f,
        (leftFar.y + rightFar.y) * 0.5f};
    const hud::HudVec2 nearCenter {
        (leftNear.x + rightNear.x) * 0.5f,
        (leftNear.y + rightNear.y) * 0.5f};

    EXPECT_NEAR(farCenter.x, expectedFar.position.x, 0.000001f);
    EXPECT_NEAR(farCenter.y, expectedFar.position.y, 0.000001f);
    EXPECT_NEAR(nearCenter.x, expectedNear.position.x, 0.000001f);
    EXPECT_NEAR(nearCenter.y, expectedNear.position.y, 0.000001f);
    EXPECT_FLOAT_EQ(frame.gun.eegsFunnelPosition.y, kGunBoreCrossHudY);
    EXPECT_GT(
        nearCenter.y + frame.gun.eegsFunnelPosition.y,
        farCenter.y + frame.gun.eegsFunnelPosition.y);
}

TEST(GunProjectileSimulationTests, ResetPublishesSixteenRegularRangeStationsNearToFar)
{
    hud_main::GunProjectileSimulation projectiles;
    const hud_main::GunLaunchState aircraft {};
    projectiles.Reset(aircraft, hud_main::EnvironmentControls {});

    const hud::GunTrajectoryInputSample snapshot = projectiles.BuildSnapshot(aircraft.positionNedMeters);
    ASSERT_EQ(snapshot.points.size(), 16U);
    for (std::size_t index = 0U; index < snapshot.points.size(); ++index)
    {
        const float fraction =
            static_cast<float>(index) / static_cast<float>(snapshot.points.size() - 1U);
        const float expectedRangeMeters =
            (kFunnelNearRangeFeet + (kFunnelFarRangeFeet - kFunnelNearRangeFeet) * fraction) *
            kFeetToMeters;
        EXPECT_TRUE(snapshot.points[index].valid);
        EXPECT_NEAR(ProjectileSlantRangeMeters(snapshot.points[index]), expectedRangeMeters, 0.05f);
        if (index > 0U)
        {
            EXPECT_LT(snapshot.points[index - 1U].ageSeconds, snapshot.points[index].ageSeconds);
        }
    }
}

TEST(GunProjectileSimulationTests, FixedRangeStationsDoNotDevelopARefreshSawtooth)
{
    hud_main::GunProjectileSimulation projectiles;
    hud_main::GunLaunchState aircraft;
    aircraft.groundVelocityNedMps = hud_main::Vec3d {230.0, 0.0, 0.0};
    const hud_main::EnvironmentControls environment {};
    projectiles.Reset(aircraft, environment);

    std::array<float, hud::kGunTrajectoryPointCount> referenceRanges {};
    const hud::GunTrajectoryInputSample initial = projectiles.BuildSnapshot(aircraft.positionNedMeters);
    for (std::size_t index = 0U; index < initial.points.size(); ++index)
    {
        ASSERT_TRUE(initial.points[index].valid);
        referenceRanges[index] = ProjectileSlantRangeMeters(initial.points[index]);
    }

    for (std::size_t tick = 0U; tick < 25U; ++tick)
    {
        aircraft.positionNedMeters.x += aircraft.groundVelocityNedMps.x * hud_main::kHudSimulationStepSeconds;
        projectiles.Step(aircraft, environment);
        const hud::GunTrajectoryInputSample snapshot = projectiles.BuildSnapshot(aircraft.positionNedMeters);
        for (std::size_t index = 0U; index < snapshot.points.size(); ++index)
        {
            ASSERT_TRUE(snapshot.points[index].valid);
            EXPECT_NEAR(ProjectileSlantRangeMeters(snapshot.points[index]), referenceRanges[index], 0.05f);
        }
    }
}

TEST(GunProjectileSimulationTests, GravityProducesRangeDependentDownwardDrop)
{
    hud_main::GunProjectileConfig gravityConfig;
    gravityConfig.dragAreaCoefficientOverMass = 0.0;
    gravityConfig.muzzleVelocityMps = 1000.0;
    gravityConfig.muzzleOffsetBodyMeters = {};
    hud_main::GunProjectileConfig zeroGravityConfig = gravityConfig;
    zeroGravityConfig.gravityMps2 = 0.0;
    hud_main::GunProjectileSimulation gravityProjectiles(gravityConfig);
    hud_main::GunProjectileSimulation zeroGravityProjectiles(zeroGravityConfig);
    const hud_main::GunLaunchState aircraft {};
    const hud_main::EnvironmentControls environment {};
    gravityProjectiles.Reset(aircraft, environment);
    zeroGravityProjectiles.Reset(aircraft, environment);
    const hud::GunTrajectoryInputSample gravitySnapshot =
        gravityProjectiles.BuildSnapshot(aircraft.positionNedMeters);
    const hud::GunTrajectoryInputSample zeroGravitySnapshot =
        zeroGravityProjectiles.BuildSnapshot(aircraft.positionNedMeters);

    ASSERT_TRUE(gravitySnapshot.points.front().valid);
    ASSERT_TRUE(gravitySnapshot.points.back().valid);
    ASSERT_TRUE(zeroGravitySnapshot.points.front().valid);
    EXPECT_GT(gravitySnapshot.points.back().downMeters, gravitySnapshot.points.front().downMeters);
    EXPECT_GT(gravitySnapshot.points.back().downMeters, zeroGravitySnapshot.points.back().downMeters);
    EXPECT_NEAR(zeroGravitySnapshot.points.back().downMeters, 0.0f, 0.0001f);
}

TEST(GunProjectileSimulationTests, RightTurnPropagatesFromNearToFarWithBallisticInertia)
{
    hud_main::GunProjectileSimulation projectiles;
    hud_main::GunLaunchState aircraft;
    constexpr double speedMetersPerSecond = 230.0;
    constexpr double yawRateRadPerSecond = 25.0 * 0.017453292519943295;
    aircraft.groundVelocityNedMps = hud_main::Vec3d {speedMetersPerSecond, 0.0, 0.0};
    const hud_main::EnvironmentControls environment {};
    projectiles.Reset(aircraft, environment);

    for (std::size_t tick = 0U; tick < 30U; ++tick)
    {
        aircraft.yawRad += yawRateRadPerSecond * hud_main::kHudSimulationStepSeconds;
        aircraft.groundVelocityNedMps = hud_main::Vec3d {
            speedMetersPerSecond * std::cos(aircraft.yawRad),
            speedMetersPerSecond * std::sin(aircraft.yawRad),
            0.0};
        aircraft.positionNedMeters.x +=
            aircraft.groundVelocityNedMps.x * hud_main::kHudSimulationStepSeconds;
        aircraft.positionNedMeters.y +=
            aircraft.groundVelocityNedMps.y * hud_main::kHudSimulationStepSeconds;
        projectiles.Step(aircraft, environment);
    }

    const hud::GunTrajectoryInputSample snapshot = projectiles.BuildSnapshot(aircraft.positionNedMeters);
    ASSERT_TRUE(snapshot.points.front().valid);
    ASSERT_TRUE(snapshot.points.back().valid);
    const float nearAzimuthRad = ProjectileBodyAzimuthRad(snapshot.points.front(), aircraft.yawRad);
    const float farAzimuthRad = ProjectileBodyAzimuthRad(snapshot.points.back(), aircraft.yawRad);
    EXPECT_LT(farAzimuthRad, nearAzimuthRad);
    EXPECT_GT(std::fabs(farAzimuthRad), std::fabs(nearAzimuthRad) + 0.01f);
}

TEST(GunProjectileSimulationTests, InvalidAtmosphereAndWindNeverPublishNonFiniteValues)
{
    hud_main::GunProjectileSimulation projectiles;
    const hud_main::GunLaunchState aircraft {};
    hud_main::EnvironmentControls environment;
    environment.windSpeedKts = std::numeric_limits<float>::infinity();
    environment.windDirectionRad = std::numeric_limits<float>::quiet_NaN();
    environment.pressureHpa = -1.0f;
    environment.outsideAirTemperatureKelvin = std::numeric_limits<float>::quiet_NaN();
    projectiles.Reset(aircraft, environment);
    projectiles.Step(aircraft, environment);

    const hud::GunTrajectoryInputSample snapshot = projectiles.BuildSnapshot(aircraft.positionNedMeters);
    for (const hud::GunTrajectoryPointNed& point : snapshot.points)
    {
        EXPECT_TRUE(std::isfinite(point.northMeters));
        EXPECT_TRUE(std::isfinite(point.eastMeters));
        EXPECT_TRUE(std::isfinite(point.downMeters));
        EXPECT_TRUE(std::isfinite(point.ageSeconds));
    }
}

TEST(HudSimulationTests, EegsFunnelRespondsToCurrentAttitudeAndBallisticTrajectory)
{
    HudInputSample stableInput;
    stableInput.weapon.masterMode = HudMasterMode::AirToAir;
    stableInput.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    stableInput.weapon.gunMode = HudGunMode::Eegs;
    stableInput.weapon.masterArm = true;
    stableInput.weapon.gunRoundsRemaining = 510;
    stableInput.target.rangeMeters = 900.0f;
    PopulateStraightGunTrajectory(stableInput);

    HudInputSample maneuveringInput = stableInput;
    maneuveringInput.aircraft.rollRad = 55.0f * kDegreesToRadians;
    for (std::size_t index = 0U; index < maneuveringInput.gunTrajectory.points.size(); ++index)
    {
        maneuveringInput.gunTrajectory.points[index].eastMeters = static_cast<float>(index * index) * 2.0f;
    }

    const hud::HudFrame stableFrame = BuildHudFrame(stableInput);
    const hud::HudFrame maneuveringFrame = BuildHudFrame(maneuveringInput);

    EXPECT_TRUE(maneuveringFrame.gun.eegsFunnelVisible);
    EXPECT_TRUE(IsFiniteHudVec(maneuveringFrame.gun.eegsFunnelPosition));
    EXPECT_TRUE(IsFiniteFunnelRail(maneuveringFrame.gun.eegsFunnelLeftControlPoints));
    EXPECT_TRUE(IsFiniteFunnelRail(maneuveringFrame.gun.eegsFunnelRightControlPoints));
    EXPECT_TRUE(std::isfinite(maneuveringFrame.gun.eegsFunnelRotationDegrees));
    EXPECT_NE(
        maneuveringFrame.gun.eegsFunnelLeftControlPoints.back().x,
        stableFrame.gun.eegsFunnelLeftControlPoints.back().x);
    EXPECT_NE(
        maneuveringFrame.gun.eegsFunnelLeftControlPoints.back().y,
        stableFrame.gun.eegsFunnelLeftControlPoints.back().y);
}

TEST(HudSimulationTests, ControllerPublishesDynamicEegsFunnelBezierRails)
{
    hud_ui::HudUi ui;
    ui.Initialize();

    HudController controller;

    HudInputSample stableInput;
    stableInput.weapon.masterMode = HudMasterMode::AirToAir;
    stableInput.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    stableInput.weapon.gunMode = HudGunMode::Eegs;
    stableInput.weapon.masterArm = true;
    stableInput.weapon.gunRoundsRemaining = 510;
    stableInput.target.rangeMeters = 900.0f;
    PopulateStraightGunTrajectory(stableInput);

    controller.Populate(ui, stableInput);
    ASSERT_TRUE(ui.HUD().eegsFunnel.GetVisible());
    EXPECT_FLOAT_EQ(ui.HUD().boresightCross.GetPosition().y, kGunBoreCrossHudY);
    EXPECT_FLOAT_EQ(ui.HUD().eegsFunnel.GetPosition().y, kGunBoreCrossHudY);
    const std::vector<mfd::Vec2> stableLeft = ui.HUD().eegsFunnel.FunnelLeft().GetControlPoints();
    const std::vector<mfd::Vec2> stableRight = ui.HUD().eegsFunnel.FunnelRight().GetControlPoints();
    ASSERT_EQ(stableLeft.size(), 5U);
    ASSERT_EQ(stableRight.size(), 5U);

    HudInputSample maneuveringInput = stableInput;
    maneuveringInput.aircraft.rollRad = 55.0f * kDegreesToRadians;
    for (std::size_t index = 0U; index < maneuveringInput.gunTrajectory.points.size(); ++index)
    {
        maneuveringInput.gunTrajectory.points[index].eastMeters = static_cast<float>(index * index) * 2.0f;
    }

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
    EXPECT_GT(
        HorizontalGap(maneuveringLeft.front(), maneuveringRight.front()),
        HorizontalGap(maneuveringLeft.back(), maneuveringRight.back()));
    EXPECT_TRUE(std::isfinite(maneuveringLeft.front().y));
    EXPECT_TRUE(std::isfinite(maneuveringRight.front().y));
}

TEST(HudSimulationTests, EegsWithLockHidesMrgsAndShowsPippers)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    input.weapon.gunMode = HudGunMode::Eegs;
    input.weapon.masterArm = true;
    input.weapon.gunRoundsRemaining = 510;
    input.weapon.targetLocked = true;
    input.target.valid = true;
    input.target.rangeMeters = 2500.0f;

    const hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_FALSE(frame.gun.mrgsVisible);
    EXPECT_FALSE(frame.gun.eegsFunnelVisible);
    EXPECT_TRUE(frame.gun.tdCircleVisible);
    EXPECT_TRUE(frame.gun.oneGPipperVisible);
    EXPECT_TRUE(frame.gun.maxGPipperVisible);
    EXPECT_TRUE(frame.gun.solutionCircleVisible);
    EXPECT_TRUE(IsFiniteHudVec(frame.gun.oneGPipperPosition));
    EXPECT_TRUE(IsFiniteHudVec(frame.gun.maxGPipperPosition));
}

TEST(HudSimulationTests, EegsTriggerSelectsFedsOrBatrByLockState)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirGun;
    input.weapon.gunMode = HudGunMode::Eegs;
    input.weapon.masterArm = true;
    input.weapon.gunRoundsRemaining = 510;
    input.weapon.triggerHeld = true;
    input.target.valid = true;

    const hud::HudFrame unlockedFrame = BuildHudFrame(input);
    EXPECT_TRUE(unlockedFrame.gun.fedsVisible);
    EXPECT_FALSE(unlockedFrame.gun.batrVisible);

    input.weapon.targetLocked = true;
    const hud::HudFrame lockedFrame = BuildHudFrame(input);
    EXPECT_FALSE(lockedFrame.gun.fedsVisible);
    EXPECT_TRUE(lockedFrame.gun.batrVisible);
}

TEST(HudSimulationTests, MissileInventoryUsesHudWeaponFamilyLabel)
{
    hud_main::MissileInventory inventory;
    inventory.aim120c = 4;
    inventory.aim9m = 2;

    EXPECT_EQ(hud_main::FormatMissileInventory(MissileType::Aim120C, inventory), "AIM-120C 4");
    EXPECT_EQ(hud_main::FormatMissileInventory(MissileType::Aim9M, inventory), "AIM-9M 2");
}

TEST(HudSimulationTests, StrafeInRangeCueUsesCallerResolvedThreshold)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToGround;
    input.weapon.weaponMode = HudWeaponMode::AirToGroundStrafe;
    input.weapon.gunMode = HudGunMode::Strafe;
    input.weapon.masterArm = true;
    input.weapon.gunRoundsRemaining = 510;
    // The caller resolves the ammunition threshold; the runtime never invents one.
    input.weapon.strafeInRangeFeet = 4000.0f;
    input.airGround.valid = true;
    input.airGround.slantRangeMeters = 3500.0f * 0.3048f;

    const hud::HudFrame inRangeFrame = BuildHudFrame(input);
    EXPECT_TRUE(inRangeFrame.gun.strafeVisible);
    EXPECT_TRUE(inRangeFrame.gun.strafeInRangeCueVisible);

    input.airGround.slantRangeMeters = 5000.0f * 0.3048f;
    const hud::HudFrame outOfRangeFrame = BuildHudFrame(input);
    EXPECT_TRUE(outOfRangeFrame.gun.strafeVisible);
    EXPECT_FALSE(outOfRangeFrame.gun.strafeInRangeCueVisible);

    // With no resolved threshold the runtime must not invent an in-range cue.
    input.weapon.strafeInRangeFeet = 0.0f;
    input.airGround.slantRangeMeters = 900.0f;
    const hud::HudFrame noThresholdFrame = BuildHudFrame(input);
    EXPECT_TRUE(noThresholdFrame.gun.strafeVisible);
    EXPECT_FALSE(noThresholdFrame.gun.strafeInRangeCueVisible);
}

TEST(HudSimulationTests, CcipPipperBombFallLineAndSolutionCueRemainFinite)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToGround;
    input.weapon.weaponMode = HudWeaponMode::AirToGroundCcip;
    input.weapon.masterArm = true;
    input.airGround.valid = true;
    input.airGround.pipperAzimuthRad = 2.0f * kDegreesToRadians;
    input.airGround.pipperDepressionRad = 6.0f * kDegreesToRadians;
    input.airGround.fallLineAzimuthRad = 1.0f * kDegreesToRadians;
    input.airGround.solutionCueValid = true;
    input.airGround.solutionCueDepressionRad = 3.0f * kDegreesToRadians;
    input.airGround.pullupAnticipationCueValid = true;
    input.airGround.pullupAnticipationCueDepressionRad = 1.0f * kDegreesToRadians;

    const hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_TRUE(frame.airGround.ccipVisible);
    EXPECT_TRUE(frame.airGround.solutionCueVisible);
    EXPECT_TRUE(frame.airGround.pullupAnticipationCueVisible);
    EXPECT_TRUE(IsFiniteHudVec(frame.airGround.ccipPipperPosition));
    EXPECT_TRUE(IsFiniteHudVec(frame.airGround.solutionCuePosition));
    EXPECT_TRUE(std::isfinite(frame.airGround.bombFallLineX));
}

TEST(HudSimulationTests, LandingGearForcesCasAndFineAltitudeScale)
{
    HudInputSample input;
    input.approach.landingGearDown = true;
    input.approach.landingModeActive = true;

    const hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_TRUE(frame.approach.landingVisible);
    EXPECT_TRUE(frame.approach.forceCalibratedAirspeed);
    EXPECT_TRUE(frame.approach.fineAltitudeScale);
    EXPECT_TRUE(frame.approach.minusTwoPointFivePitchLineVisible);
    EXPECT_TRUE(IsFiniteHudVec(frame.approach.minusTwoPointFivePitchLinePosition));
}

TEST(HudSimulationTests, LandingDeclutterMasksIlsAndRollIndicator)
{
    HudInputSample input;
    input.approach.landingModeActive = true;
    input.approach.landingDeclutterActive = true;
    input.ils.powered = true;
    input.ils.selected = true;
    input.ils.signalValid = true;
    input.ils.commandSteeringActive = true;

    const hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_TRUE(frame.approach.declutterActive);
    EXPECT_FALSE(frame.approach.rollIndicatorVisible);
    EXPECT_FALSE(frame.ils.barsVisible);
    EXPECT_FALSE(frame.ils.commandSteeringVisible);
}

TEST(HudSimulationTests, IlsVisibilityAndDeviationClampAreFinite)
{
    HudInputSample input;
    input.ils.powered = true;
    input.ils.selected = true;
    input.ils.signalValid = true;
    input.ils.commandSteeringActive = true;
    input.ils.localizerDeviationDots = std::numeric_limits<float>::infinity();
    input.ils.glideslopeDeviationDots = -8.0f;

    const hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_TRUE(frame.ils.barsVisible);
    EXPECT_TRUE(frame.ils.commandSteeringVisible);
    EXPECT_FLOAT_EQ(frame.ils.localizerDeviationDots, 0.0f);
    EXPECT_FLOAT_EQ(frame.ils.glideslopeDeviationDots, -2.5f);
    EXPECT_TRUE(IsFiniteHudVec(frame.ils.localizerBarPosition));
    EXPECT_TRUE(IsFiniteHudVec(frame.ils.glideslopeBarPosition));

    input.ils.powered = false;
    const hud::HudFrame offFrame = BuildHudFrame(input);
    EXPECT_FALSE(offFrame.ils.barsVisible);
}

TEST(HudSimulationTests, WSteeringCueIsElevenMilsBelowBoresightWhenFpmUnavailable)
{
    HudInputSample input;
    input.approach.flightPathMarkerAvailable = false;
    input.ils.powered = true;
    input.ils.selected = true;
    input.ils.signalValid = true;

    const hud::HudFrame frame = BuildHudFrame(input);

    EXPECT_TRUE(frame.ils.wSteeringVisible);
    EXPECT_NEAR(frame.ils.wSteeringPosition.x, 0.0f, 1.0e-5f);
    EXPECT_NEAR(
        frame.ils.wSteeringPosition.y,
        kGunBoreCrossHudY - 11.0f * kHudUnitsPerMil,
        1.0e-4f);
}

TEST(HudSimulationTests, ImpactMissileIsRetainedBrieflyThenRemoved)
{
    HudSimulation simulation;
    SimulationControls controls;
    controls.pilot.masterMode = HudMasterMode::AirToAir;
    simulation.SetSimulationControls(controls);

    ASSERT_TRUE(simulation.FireSelectedMissile());
    ASSERT_EQ(simulation.MissileShots().size(), 1U);

    const int stepsUntilRetentionExpired =
        static_cast<int>(std::ceil((simulation.MissileShots().front().timeRemainingSeconds + 8.4f) / 0.080f));
    StepMany(simulation, controls, stepsUntilRetentionExpired);

    EXPECT_TRUE(simulation.MissileShots().empty());
}

TEST(HudSimulationTests, DefaultEnvironmentKeepsZeroWindStandardAtmosphereBehavior)
{
    HudSimulation simulation;
    SimulationControls controls;
    StepMany(simulation, controls, 25);

    const hud::AircraftInputSample& aircraft = simulation.Inputs().aircraft;
    const float groundSpeedMps = std::sqrt(
        aircraft.northSpeedMps * aircraft.northSpeedMps +
        aircraft.eastSpeedMps * aircraft.eastSpeedMps +
        aircraft.downSpeedMps * aircraft.downSpeedMps);
    // The default environment is a standard atmosphere at sea level.
    const float speedOfSoundMps = hud_main::ComputeSpeedOfSoundMps(288.15f);
    EXPECT_NEAR(speedOfSoundMps, 340.29f, 0.5f);
    // With zero wind the published ground velocity equals the air velocity, so
    // the Mach published to the HUD stays consistent with the legacy TAS/a0 model.
    EXPECT_NEAR(aircraft.mach, groundSpeedMps / speedOfSoundMps, 1.0e-3f);
    EXPECT_GT(aircraft.mach, 0.3f);
}

TEST(HudSimulationTests, WindShiftsGroundVelocityWithoutChangingAirspeed)
{
    HudSimulation calmSimulation;
    HudSimulation windySimulation;
    SimulationControls calmControls;
    SimulationControls windyControls;
    // 40 knots FROM the West (270 degrees): the air mass moves toward the East.
    windyControls.environment.windSpeedKts = 40.0f;
    windyControls.environment.windDirectionRad = 270.0f * kDegreesToRadians;

    StepMany(calmSimulation, calmControls, 25);
    StepMany(windySimulation, windyControls, 25);

    const hud::AircraftInputSample& calm = calmSimulation.Inputs().aircraft;
    const hud::AircraftInputSample& windy = windySimulation.Inputs().aircraft;
    const float expectedEastWindMps = 40.0f * 0.514444444f;
    // The heading stays North in both runs, so the wind shows up as a pure
    // eastward ground-velocity offset while the air-relative state is identical.
    EXPECT_NEAR(windy.eastSpeedMps - calm.eastSpeedMps, expectedEastWindMps, 0.2f);
    EXPECT_NEAR(windy.northSpeedMps, calm.northSpeedMps, 0.2f);
    EXPECT_FLOAT_EQ(windy.mach, calm.mach);
}

TEST(HudSimulationTests, TerrainElevationControlDrivesRadioAltitude)
{
    HudSimulation defaultSimulation;
    HudSimulation raisedSimulation;
    SimulationControls defaultControls;
    SimulationControls raisedControls;
    raisedControls.environment.terrainElevationMeters = 1000.0f;

    StepMany(defaultSimulation, defaultControls, 25);
    StepMany(raisedSimulation, raisedControls, 25);

    const float defaultRadioAltitude = defaultSimulation.Inputs().aircraft.radioAltitudeMeters;
    const float raisedRadioAltitude = raisedSimulation.Inputs().aircraft.radioAltitudeMeters;
    // Both runs fly the same trajectory over the same undulation, so the radar
    // altitude drops by exactly the terrain-base difference (default is 128 m).
    EXPECT_NEAR(defaultRadioAltitude - raisedRadioAltitude, 1000.0f - 128.0f, 0.5f);
}

TEST(HudSimulationTests, TurbulenceStaysDeterministicBoundedAndFinite)
{
    HudSimulation firstSimulation;
    HudSimulation secondSimulation;
    HudSimulation calmSimulation;
    SimulationControls turbulentControls;
    turbulentControls.environment.turbulenceIntensity = 1.0f;
    SimulationControls calmControls;

    StepMany(firstSimulation, turbulentControls, 40);
    StepMany(secondSimulation, turbulentControls, 40);
    StepMany(calmSimulation, calmControls, 40);

    const hud::AircraftInputSample& first = firstSimulation.Inputs().aircraft;
    const hud::AircraftInputSample& second = secondSimulation.Inputs().aircraft;
    const hud::AircraftInputSample& calm = calmSimulation.Inputs().aircraft;
    // No random source: two identical runs produce the exact same state.
    EXPECT_FLOAT_EQ(first.northSpeedMps, second.northSpeedMps);
    EXPECT_FLOAT_EQ(first.eastSpeedMps, second.eastSpeedMps);
    EXPECT_FLOAT_EQ(first.downSpeedMps, second.downSpeedMps);
    EXPECT_FLOAT_EQ(first.altitudeMeters, second.altitudeMeters);
    // The gusts stay a strictly bounded perturbation around the calm trajectory.
    EXPECT_LT(std::fabs(first.eastSpeedMps - calm.eastSpeedMps), 10.0f);
    EXPECT_LT(std::fabs(first.downSpeedMps - calm.downSpeedMps), 10.0f);
    EXPECT_TRUE(std::isfinite(first.northSpeedMps));
    EXPECT_TRUE(std::isfinite(first.altitudeMeters));
    EXPECT_TRUE(std::isfinite(first.specificEnergyRateMps));
}

TEST(HudProjectionTests, PitchLadderUsesThirtyDegreeVerticalFov)
{
    EXPECT_FLOAT_EQ(HudPitchLadderVerticalFovDegrees(), 30.0f);

    // 2.0 useful units over 30 degrees gives 0.0666667 units per degree.
    EXPECT_NEAR(ProjectPitchOffsetToHud(5.0f).y, 0.333333f, 1.0e-4f);
    EXPECT_NEAR(ProjectPitchOffsetToHud(10.0f).y, 0.666667f, 1.0e-4f);
    EXPECT_NEAR(ProjectPitchOffsetToHud(15.0f).y, 1.0f, 1.0e-4f);
    EXPECT_NEAR(ProjectPitchOffsetToHud(0.0f).y, 0.0f, 1.0e-6f);
    // The ladder must sit on the horizon at level flight, then move one bar down
    // per five degrees of nose-up pitch.
    EXPECT_NEAR(ProjectPitchOffsetToHud(-5.0f).y, -0.333333f, 1.0e-4f);
}

TEST(HudProjectionTests, PitchZeroPlacesLadderAtCenterAndPlusFiveMovesOneBar)
{
    HudInputSample level;
    level.aircraft.pitchRad = 0.0f;
    level.aircraft.rollRad = 0.0f;
    level.aircraft.northSpeedMps = 200.0f;
    level.aircraft.eastSpeedMps = 0.0f;
    level.aircraft.downSpeedMps = 0.0f;

    const hud::HudFrame levelFrame = BuildHudFrame(level);
    EXPECT_NEAR(levelFrame.attitude.ladderPosition.x, 0.0f, 1.0e-5f);
    EXPECT_NEAR(levelFrame.attitude.ladderPosition.y, 0.0f, 1.0e-5f);

    HudInputSample climbing = level;
    climbing.aircraft.pitchRad = 5.0f * kDegreesToRadians;
    const hud::HudFrame climbingFrame = BuildHudFrame(climbing);
    // A +5 degree pitch drives the ladder down by exactly one 5-degree bar.
    EXPECT_NEAR(climbingFrame.attitude.ladderPosition.y, -0.333333f, 1.0e-3f);
}

TEST(HudProjectionTests, ConformalProjectionCenterEdgesAndOutsideFov)
{
    const hud::HudAngularProjection projection = HudConformalProjection();
    ASSERT_FLOAT_EQ(projection.horizontalFovDeg, 30.0f);
    ASSERT_FLOAT_EQ(projection.verticalFovDeg, 30.0f);

    const hud::ProjectedHudPoint center = ProjectBoresightAngularOffsetToHud(0.0f, 0.0f);
    EXPECT_NEAR(center.position.x, 0.0f, 1.0e-5f);
    EXPECT_NEAR(center.position.y, 0.0f, 1.0e-5f);
    EXPECT_TRUE(center.insideFov);

    const float halfHorizontalRad = (projection.horizontalFovDeg * 0.5f) * kDegreesToRadians;
    const float halfVerticalRad = (projection.verticalFovDeg * 0.5f) * kDegreesToRadians;

    const hud::ProjectedHudPoint rightEdge = ProjectBoresightAngularOffsetToHud(halfHorizontalRad, 0.0f);
    EXPECT_NEAR(rightEdge.position.x, projection.halfWidthUnits, 1.0e-3f);

    const hud::ProjectedHudPoint topEdge = ProjectBoresightAngularOffsetToHud(0.0f, halfVerticalRad);
    EXPECT_NEAR(topEdge.position.y, projection.halfHeightUnits, 1.0e-3f);

    // Beyond the field of view the projector must report the symbol as outside,
    // and must clamp rather than run off the page.
    const hud::ProjectedHudPoint outsideAzimuth =
        ProjectBoresightAngularOffsetToHud(halfHorizontalRad + 4.0f * kDegreesToRadians, 0.0f);
    EXPECT_FALSE(outsideAzimuth.insideFov);
    EXPECT_TRUE(outsideAzimuth.limited);
    EXPECT_LE(std::fabs(outsideAzimuth.position.x), projection.halfWidthUnits + 1.0e-5f);

    const hud::ProjectedHudPoint outsideElevation =
        ProjectBoresightAngularOffsetToHud(0.0f, halfVerticalRad + 4.0f * kDegreesToRadians);
    EXPECT_FALSE(outsideElevation.insideFov);
    EXPECT_LE(std::fabs(outsideElevation.position.y), projection.halfHeightUnits + 1.0e-5f);
}

TEST(HudProjectionTests, TargetOutsideFovIsClampedAndLimitXIsPublished)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirMissile;
    input.weapon.simulateMode = true;
    input.target.valid = true;

    input.target.azimuthRad = 3.0f * kDegreesToRadians;
    const hud::HudFrame insideFrame = BuildHudFrame(input);
    EXPECT_TRUE(insideFrame.weapon.targetVisible);
    EXPECT_FALSE(insideFrame.weapon.targetLimited);
    EXPECT_FALSE(insideFrame.weapon.missileLimitXVisible);

    const hud::HudAngularProjection projection = HudConformalProjection();
    input.target.azimuthRad = 24.0f * kDegreesToRadians;
    const hud::HudFrame outsideFrame = BuildHudFrame(input);
    // The diamond is not hidden: it stays visible, clamped to the FOV edge, and a
    // geometric limit-X is published on it (BMS AIM-120 behavior).
    EXPECT_TRUE(outsideFrame.weapon.targetVisible);
    EXPECT_TRUE(outsideFrame.weapon.targetLimited);
    EXPECT_TRUE(outsideFrame.weapon.missileDiamondLimited);
    EXPECT_TRUE(outsideFrame.weapon.missileLimitXVisible);
    EXPECT_NEAR(outsideFrame.weapon.targetPosition.x, projection.halfWidthUnits, 1.0e-3f);
    EXPECT_NEAR(outsideFrame.weapon.missileLimitXPosition.x, outsideFrame.weapon.targetPosition.x, 1.0e-6f);
}

TEST(HudProjectionTests, ControllerShowsMissileDiamondAndLimitXWhenTargetOutsideFov)
{
    hud_ui::HudUi ui;
    ui.Initialize();

    HudController controller;
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirMissile;
    input.weapon.simulateMode = true;
    input.target.valid = true;
    input.target.azimuthRad = 24.0f * kDegreesToRadians;

    controller.Populate(ui, input);
    EXPECT_TRUE(ui.HUD().missileDiamond.GetVisible());
    EXPECT_TRUE(ui.HUD().missileLimitX.GetVisible());
    // The limit-X is distinct from the too-close Break-X, which stays hidden here.
    EXPECT_FALSE(ui.HUD().breakX.GetVisible());
}

TEST(HudProjectionTests, BreakXStaysTooCloseCueAndIsSeparateFromLimitX)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToAir;
    input.weapon.weaponMode = HudWeaponMode::AirToAirMissile;
    input.weapon.simulateMode = true;
    input.target.valid = true;
    // Inside AIM-120 rmin1 (1.15 NM) and simultaneously outside the HUD FOV.
    input.target.rangeMeters = 0.2f * 1852.0f;
    input.target.azimuthRad = 24.0f * kDegreesToRadians;
    // The launch zone is a caller-resolved fact; the sample armament model of
    // the main client computes it before the HUD runtime draws it.
    input.weapon.launchZone = ComputeLaunchZone(input.aircraft, input.target, MissileType::Aim120C);

    const hud::HudFrame frame = BuildHudFrame(input);
    EXPECT_TRUE(frame.weapon.launchZone.tooClose);
    // Break-X depends on too-close, not on the field of view.
    EXPECT_TRUE(frame.weapon.breakXVisible);
    // Limit-X is the separate geometric cue; both can be active at once.
    EXPECT_TRUE(frame.weapon.missileLimitXVisible);
}

TEST(HudProjectionTests, CcipPipperOutsideFovStaysVisibleWithLimitX)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToGround;
    input.weapon.weaponMode = HudWeaponMode::AirToGroundCcip;
    input.weapon.masterArm = true;
    input.airGround.valid = true;
    input.airGround.pipperDepressionRad = 24.0f * kDegreesToRadians;

    const hud::HudFrame frame = BuildHudFrame(input);
    EXPECT_TRUE(frame.airGround.ccipVisible);
    EXPECT_TRUE(frame.airGround.ccipPipperLimited);
    EXPECT_TRUE(frame.airGround.ccipLimitXVisible);
    EXPECT_NEAR(frame.airGround.ccipPipperPosition.y, -HudConformalProjection().halfHeightUnits, 1.0e-3f);
    EXPECT_TRUE(IsFiniteHudVec(frame.airGround.ccipPipperPosition));
    EXPECT_NEAR(frame.airGround.ccipLimitXPosition.y, frame.airGround.ccipPipperPosition.y, 1.0e-6f);
}

TEST(HudProjectionTests, CcipSolutionCueAndPuacClampedNotHiddenOutsideFov)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToGround;
    input.weapon.weaponMode = HudWeaponMode::AirToGroundCcip;
    input.weapon.masterArm = true;
    input.airGround.valid = true;
    input.airGround.pipperDepressionRad = 5.0f * kDegreesToRadians;
    input.airGround.solutionCueValid = true;
    input.airGround.solutionCueDepressionRad = 24.0f * kDegreesToRadians;
    input.airGround.pullupAnticipationCueValid = true;
    input.airGround.pullupAnticipationCueDepressionRad = 26.0f * kDegreesToRadians;

    const hud::HudFrame frame = BuildHudFrame(input);
    // Cues are not silently dropped: they stay visible, clamped, and flagged.
    EXPECT_TRUE(frame.airGround.solutionCueVisible);
    EXPECT_TRUE(frame.airGround.solutionCueLimited);
    EXPECT_TRUE(frame.airGround.pullupAnticipationCueVisible);
    EXPECT_TRUE(frame.airGround.pullupAnticipationCueLimited);
    EXPECT_TRUE(IsFiniteHudVec(frame.airGround.solutionCuePosition));
    EXPECT_TRUE(IsFiniteHudVec(frame.airGround.pullupAnticipationCuePosition));
}

TEST(HudProjectionTests, StrafePipperOutsideFovStaysVisibleAndLimited)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToGround;
    input.weapon.weaponMode = HudWeaponMode::AirToGroundStrafe;
    input.weapon.gunMode = HudGunMode::Strafe;
    input.weapon.masterArm = true;
    input.weapon.gunRoundsRemaining = 510;
    input.airGround.valid = true;
    input.airGround.slantRangeMeters = 900.0f;
    input.airGround.pipperDepressionRad = 24.0f * kDegreesToRadians;

    const hud::HudFrame frame = BuildHudFrame(input);
    EXPECT_TRUE(frame.gun.strafeVisible);
    EXPECT_TRUE(frame.gun.strafePipperLimited);
    EXPECT_TRUE(IsFiniteHudVec(frame.gun.strafePipperPosition));
}

TEST(HudProjectionTests, CcipPipperAndBombFallLineShareTheConformalScale)
{
    HudInputSample input;
    input.weapon.masterMode = HudMasterMode::AirToGround;
    input.weapon.weaponMode = HudWeaponMode::AirToGroundCcip;
    input.weapon.masterArm = true;
    input.airGround.valid = true;
    input.airGround.pipperAzimuthRad = 0.0f;
    input.airGround.pipperDepressionRad = 6.0f * kDegreesToRadians;
    input.airGround.fallLineAzimuthRad = 4.0f * kDegreesToRadians;

    const hud::HudFrame frame = BuildHudFrame(input);
    ASSERT_TRUE(frame.airGround.ccipVisible);

    // A 6 degree depression is a downward elevation on the shared vertical scale.
    EXPECT_NEAR(frame.airGround.ccipPipperPosition.y, ProjectPitchOffsetToHud(-6.0f).y, 1.0e-4f);
    // The bomb fall line X uses the same horizontal scale as any conformal symbol.
    const hud::ProjectedHudPoint fallLine =
        ProjectBoresightAngularOffsetToHud(4.0f * kDegreesToRadians, 0.0f);
    EXPECT_NEAR(frame.airGround.bombFallLineX, fallLine.position.x, 1.0e-4f);
}

TEST(HudProjectionTests, NonFiniteAnglesNeverProduceNonFiniteOutput)
{
    const hud::ProjectedHudPoint point = ProjectBoresightAngularOffsetToHud(
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity());
    EXPECT_TRUE(std::isfinite(point.position.x));
    EXPECT_TRUE(std::isfinite(point.position.y));
    EXPECT_TRUE(std::isfinite(ProjectPitchOffsetToHud(std::numeric_limits<float>::quiet_NaN()).y));

    HudInputSample input;
    input.aircraft.pitchRad = std::numeric_limits<float>::quiet_NaN();
    input.target.azimuthRad = std::numeric_limits<float>::infinity();
    input.target.elevationRad = -std::numeric_limits<float>::infinity();
    const hud::HudFrame frame = BuildHudFrame(input);
    EXPECT_TRUE(IsFiniteHudVec(frame.attitude.ladderPosition));
    EXPECT_TRUE(IsFiniteHudVec(frame.weapon.targetPosition));
}

TEST(HudProjectionTests, PluggableProjectionBuildsFrameFromSemanticInputAlone)
{
    // The projection is reachable with a hand-built HudInputSample only: no sample panel
    // simulation, control panel or generated UI is required to obtain a frame.
    HudInputSample input;
    input.aircraft.pitchRad = 7.5f * kDegreesToRadians;
    input.target.valid = true;

    const hud::HudFrame frame = BuildHudFrame(input);
    EXPECT_TRUE(IsFiniteHudVec(frame.attitude.ladderPosition));
    EXPECT_NEAR(frame.attitude.ladderPosition.y, ProjectPitchOffsetToHud(-7.5f).y, 1.0e-3f);
}

TEST(HudProjectionTests, PitchLadderJsonMatchesCppAngularScale)
{
    const std::filesystem::path ladderPath =
        RepositoryRoot() / "examples" / "hud" / "assets" / "reticles" / "hud_pitch_ladder.json";
    const std::string content = ReadFileText(ladderPath);
    ASSERT_FALSE(content.empty()) << "Unable to read " << ladderPath.string();

    // Every authored bar Y must equal the C++ pitch-ladder projection for its
    // degree, so the JSON scale cannot drift away from the code. The 85 degree
    // bar also guards that the full +/-5..+/-85 ladder is still present.
    for (const int degrees : {5, 10, 15, 20, 30, 85})
    {
        const std::string positiveId = "p" + std::to_string(degrees) + "_left";
        const std::string negativeId = "n" + std::to_string(degrees) + "_left_0";
        const float expected = ProjectPitchOffsetToHud(static_cast<float>(degrees)).y;

        EXPECT_NEAR(ReadLadderBarStartY(content, positiveId), expected, 1.0e-3f)
            << "positive bar " << degrees << " degrees";
        EXPECT_NEAR(ReadLadderBarStartY(content, negativeId), -expected, 1.0e-3f)
            << "negative bar " << degrees << " degrees";
    }
}
} // namespace
