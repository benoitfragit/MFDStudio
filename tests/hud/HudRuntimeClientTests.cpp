/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Link-level tests consuming hud_runtime through its exported surface.
 *
 * This binary links the hud_runtime shared library instead of compiling its
 * sources, so every check below also proves that the public symbols are
 * exported and imported correctly across the DLL/shared-library boundary and
 * that no ImGui, DX11 or main-client dependency is required to use the HUD.
 */

#include "hud/HudProjection.h"
#include "hud/HudRuntimeClient.h"

#include <cmath>
#include <string>
#include <utility>

#include <gtest/gtest.h>

namespace
{
constexpr float kDegreesToRadians = 0.017453292519943295f;

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

TEST(HudRuntimeLibraryTests, FunnelControlPointsAreComputedFromGenericInputSample)
{
    // A generic EEGS sample: aircraft/target physics plus resolved avionics
    // state, without any main-client armament type. The runtime must still
    // build the five-point Bezier rails on its own.
    hud::HudInputSample input;
    input.weapon.masterMode = hud::HudMasterMode::AirToAir;
    input.weapon.weaponMode = hud::HudWeaponMode::AirToAirGun;
    input.weapon.gunMode = hud::HudGunMode::Eegs;
    input.weapon.masterArm = true;
    input.weapon.gunRoundsRemaining = 510;
    input.weapon.targetWingspanMeters = 11.0f;
    input.weapon.targetAccelerationMps2 = 20.0f;
    input.aircraft.rollRad = 30.0f * kDegreesToRadians;
    input.aircraft.normalLoadFactor = 4.0f;
    input.target.rangeMeters = 900.0f;
    input.target.azimuthRad = 3.0f * kDegreesToRadians;

    const hud::HudFrame frame = hud::BuildHudFrame(input);

    EXPECT_TRUE(frame.gun.eegsFunnelVisible);
    EXPECT_TRUE(IsFiniteFunnelRail(frame.gun.eegsFunnelLeftControlPoints));
    EXPECT_TRUE(IsFiniteFunnelRail(frame.gun.eegsFunnelRightControlPoints));
    // The rails widen from the far end to the near end of the funnel.
    EXPECT_LT(
        std::fabs(frame.gun.eegsFunnelRightControlPoints.front().x -
                  frame.gun.eegsFunnelLeftControlPoints.front().x),
        std::fabs(frame.gun.eegsFunnelRightControlPoints.back().x -
                  frame.gun.eegsFunnelLeftControlPoints.back().x));
}

TEST(HudRuntimeLibraryTests, WeaponFactsArePassedThroughNotComputed)
{
    hud::HudInputSample input;
    input.weapon.masterMode = hud::HudMasterMode::AirToAir;
    input.weapon.weaponMode = hud::HudWeaponMode::AirToAirMissile;
    input.weapon.simulateMode = true;
    input.target.valid = true;
    input.weapon.selectedWeaponQuantity = 2;
    input.weapon.missileDiamondScale = 0.5f;
    input.weapon.selectedMissileTimeOfFlightSeconds = 12.5f;
    input.weapon.launchZone.rmax1Nm = 20.0f;
    input.weapon.launchZone.rmax2Nm = 14.0f;
    input.weapon.launchZone.rmin2Nm = 2.0f;
    input.weapon.launchZone.rmin1Nm = 1.0f;
    input.weapon.launchZone.tooClose = true;

    const hud::HudFrame frame = hud::BuildHudFrame(input);

    EXPECT_FLOAT_EQ(frame.weapon.missileDiamondScale, 0.5f);
    EXPECT_FLOAT_EQ(frame.weapon.selectedMissileTimeOfFlightSeconds, 12.5f);
    EXPECT_FLOAT_EQ(frame.weapon.launchZone.rmax1Nm, 20.0f);
    EXPECT_TRUE(frame.weapon.launchZone.tooClose);
    EXPECT_TRUE(frame.weapon.breakXVisible);
    EXPECT_TRUE(frame.weapon.rangeCueVisible);

    // Depleting the caller-resolved quantity hides the range cue.
    input.weapon.selectedWeaponQuantity = 0;
    const hud::HudFrame depletedFrame = hud::BuildHudFrame(input);
    EXPECT_FALSE(depletedFrame.weapon.rangeCueVisible);
}

TEST(HudRuntimeLibraryTests, DefaultInputSampleIsNeutral)
{
    // The public runtime contract must default to a grounded, target-free,
    // unarmed sample so an integrator embedding the DLL sees no scene state.
    hud::HudInputSample input;

    EXPECT_FALSE(input.target.valid);
    EXPECT_FALSE(input.weapon.masterArm);
    EXPECT_EQ(input.weapon.selectedWeaponQuantity, 0);
    EXPECT_EQ(input.weapon.gunRoundsRemaining, 0);
    EXPECT_FLOAT_EQ(input.weapon.targetWingspanMeters, 0.0f);
    EXPECT_FLOAT_EQ(input.weapon.strafeInRangeFeet, 0.0f);
    EXPECT_FLOAT_EQ(input.aircraft.altitudeMeters, 0.0f);
    EXPECT_FLOAT_EQ(input.aircraft.pitchRad, 0.0f);

    // A neutral sample must not publish any weapon or target symbology.
    const hud::HudFrame frame = hud::BuildHudFrame(input);
    EXPECT_FALSE(frame.weapon.airToAirVisible);
    EXPECT_FALSE(frame.weapon.targetVisible);
    EXPECT_FALSE(frame.gun.airToAirGunVisible);
    EXPECT_FALSE(frame.gun.strafeVisible);
    EXPECT_FALSE(frame.airGround.ccipVisible);
}

TEST(HudRuntimeLibraryTests, StrafeInRangeThresholdIsCallerResolved)
{
    hud::HudInputSample input;
    input.weapon.masterMode = hud::HudMasterMode::AirToGround;
    input.weapon.weaponMode = hud::HudWeaponMode::AirToGroundStrafe;
    input.weapon.gunMode = hud::HudGunMode::Strafe;
    input.weapon.masterArm = true;
    input.weapon.gunRoundsRemaining = 510;
    input.airGround.valid = true;
    input.airGround.slantRangeMeters = 900.0f;

    // No resolved threshold: the runtime must not invent an in-range cue.
    input.weapon.strafeInRangeFeet = 0.0f;
    const hud::HudFrame noThresholdFrame = hud::BuildHudFrame(input);
    EXPECT_TRUE(noThresholdFrame.gun.strafeVisible);
    EXPECT_FALSE(noThresholdFrame.gun.strafeInRangeCueVisible);

    // Range at or below the caller threshold shows the cue.
    input.weapon.strafeInRangeFeet = 12000.0f;
    const hud::HudFrame inRangeFrame = hud::BuildHudFrame(input);
    EXPECT_TRUE(inRangeFrame.gun.strafeInRangeCueVisible);

    // Range beyond the caller threshold hides the cue.
    input.weapon.strafeInRangeFeet = 1000.0f;
    const hud::HudFrame outOfRangeFrame = hud::BuildHudFrame(input);
    EXPECT_FALSE(outOfRangeFrame.gun.strafeInRangeCueVisible);
}

TEST(HudRuntimeClientTests, PublishWithoutInitializeFailsGracefully)
{
    hud::HudRuntimeClient client;
    hud::HudInputSample input;
    std::string error;

    EXPECT_FALSE(client.Publish(input, error));
    EXPECT_FALSE(error.empty());

    // Shutdown on a never-initialized client must be a safe no-op.
    client.Shutdown();
}

TEST(HudRuntimeClientTests, InitializeFailsWithReadableErrorForMissingAssetRoot)
{
    hud::HudRuntimeClient client;
    hud::HudRuntimeConfig config;
    config.assetRoot = "nonexistent_hud_asset_root";
    std::string error;

    EXPECT_FALSE(client.Initialize(config, error));
    EXPECT_FALSE(error.empty());

    // A failed initialization must leave the client unable to publish.
    hud::HudInputSample input;
    error.clear();
    EXPECT_FALSE(client.Publish(input, error));
    EXPECT_FALSE(error.empty());
}

TEST(HudRuntimeClientTests, MovedFromClientStaysSafeAndMovedToClientKeepsState)
{
    hud::HudRuntimeClient source;
    hud::HudRuntimeClient destination(std::move(source));

    hud::HudInputSample input;
    std::string error;
    EXPECT_FALSE(destination.Publish(input, error));
    EXPECT_FALSE(error.empty());

    // The moved-from client must not crash and must report a readable error.
    error.clear();
    EXPECT_FALSE(source.Publish(input, error));
    EXPECT_FALSE(error.empty());
    source.Shutdown();
}
} // namespace
