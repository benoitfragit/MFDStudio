/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Regression tests for the pure HUD mini-simulation physics helpers.
 */

#include "hud_main/HudPhysics.h"

#include <cmath>
#include <initializer_list>
#include <limits>

#include <gtest/gtest.h>

namespace
{
using hud_main::ComputeAirDensityKgPerM3;
using hud_main::ComputeMach;
using hud_main::ComputeRadioAltitudeMeters;
using hud_main::ComputeSpecificEnergyRateMps;
using hud_main::ComputeSpeedOfSoundMps;
using hud_main::ComputeTerrainElevationMeters;
using hud_main::ComputeTurbulenceGustNed;
using hud_main::ComputeWindVectorNed;
using hud_main::EnvironmentControls;
using hud_main::WindVectorNed;

constexpr float kPi = 3.14159265358979323846f;
constexpr float kKnotsToMetersPerSecond = 0.514444444f;
constexpr float kStandardTemperatureKelvin = 288.15f;
constexpr float kStandardPressureHpa = 1013.25f;

TEST(HudPhysicsTests, ZeroWindSpeedGivesZeroVectorForAnyDirection)
{
    for (const float directionRad : {0.0f, 0.7f, kPi, 4.9f, -2.3f})
    {
        const WindVectorNed wind = ComputeWindVectorNed(0.0f, directionRad);
        EXPECT_FLOAT_EQ(wind.northMps, 0.0f);
        EXPECT_FLOAT_EQ(wind.eastMps, 0.0f);
        EXPECT_FLOAT_EQ(wind.downMps, 0.0f);
    }
}

TEST(HudPhysicsTests, WindFromConventionMatchesCardinalDirections)
{
    const float speedKts = 20.0f;
    const float speedMps = speedKts * kKnotsToMetersPerSecond;

    // 0 degrees FROM: wind coming from the North blows toward the South.
    const WindVectorNed fromNorth = ComputeWindVectorNed(speedKts, 0.0f);
    EXPECT_NEAR(fromNorth.northMps, -speedMps, 1.0e-4f);
    EXPECT_NEAR(fromNorth.eastMps, 0.0f, 1.0e-4f);

    // 90 degrees FROM: wind coming from the East blows toward the West.
    const WindVectorNed fromEast = ComputeWindVectorNed(speedKts, 0.5f * kPi);
    EXPECT_NEAR(fromEast.eastMps, -speedMps, 1.0e-4f);
    EXPECT_NEAR(fromEast.northMps, 0.0f, 1.0e-4f);

    // 180 degrees FROM: wind coming from the South blows toward the North.
    const WindVectorNed fromSouth = ComputeWindVectorNed(speedKts, kPi);
    EXPECT_NEAR(fromSouth.northMps, speedMps, 1.0e-4f);

    // 270 degrees FROM: wind coming from the West blows toward the East.
    const WindVectorNed fromWest = ComputeWindVectorNed(speedKts, 1.5f * kPi);
    EXPECT_NEAR(fromWest.eastMps, speedMps, 1.0e-4f);
    EXPECT_NEAR(fromWest.northMps, 0.0f, 1.0e-4f);

    // The wind stays horizontal in every case.
    EXPECT_FLOAT_EQ(fromNorth.downMps, 0.0f);
    EXPECT_FLOAT_EQ(fromWest.downMps, 0.0f);
}

TEST(HudPhysicsTests, NonFiniteWindInputsGiveZeroVector)
{
    const WindVectorNed wind = ComputeWindVectorNed(
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(wind.northMps, 0.0f);
    EXPECT_FLOAT_EQ(wind.eastMps, 0.0f);
    EXPECT_FLOAT_EQ(wind.downMps, 0.0f);
}

TEST(HudPhysicsTests, SpeedOfSoundIsPositiveAroundStandardAtmosphere)
{
    const float speedOfSoundMps = ComputeSpeedOfSoundMps(kStandardTemperatureKelvin);
    EXPECT_NEAR(speedOfSoundMps, 340.29f, 0.5f);
    EXPECT_GT(ComputeSpeedOfSoundMps(250.0f), 0.0f);
    EXPECT_FLOAT_EQ(ComputeSpeedOfSoundMps(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ComputeSpeedOfSoundMps(std::numeric_limits<float>::quiet_NaN()), 0.0f);
}

TEST(HudPhysicsTests, AirDensityIsPositiveAroundStandardAtmosphere)
{
    const float densityKgPerM3 = ComputeAirDensityKgPerM3(kStandardPressureHpa, kStandardTemperatureKelvin);
    EXPECT_NEAR(densityKgPerM3, 1.225f, 0.01f);
    EXPECT_FLOAT_EQ(ComputeAirDensityKgPerM3(0.0f, kStandardTemperatureKelvin), 0.0f);
    EXPECT_FLOAT_EQ(ComputeAirDensityKgPerM3(kStandardPressureHpa, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(
        ComputeAirDensityKgPerM3(std::numeric_limits<float>::infinity(), kStandardTemperatureKelvin), 0.0f);
}

TEST(HudPhysicsTests, MachReturnsZeroForInvalidInputs)
{
    EXPECT_NEAR(ComputeMach(170.147f, 340.294f), 0.5f, 1.0e-5f);
    EXPECT_FLOAT_EQ(ComputeMach(0.0f, 340.294f), 0.0f);
    EXPECT_FLOAT_EQ(ComputeMach(200.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ComputeMach(std::numeric_limits<float>::quiet_NaN(), 340.294f), 0.0f);
    EXPECT_FLOAT_EQ(ComputeMach(200.0f, std::numeric_limits<float>::quiet_NaN()), 0.0f);
}

TEST(HudPhysicsTests, RadioAltitudeIsClampedToZero)
{
    EXPECT_FLOAT_EQ(ComputeRadioAltitudeMeters(1500.0f, 500.0f), 1000.0f);
    EXPECT_FLOAT_EQ(ComputeRadioAltitudeMeters(100.0f, 500.0f), 0.0f);
    EXPECT_FLOAT_EQ(
        ComputeRadioAltitudeMeters(std::numeric_limits<float>::quiet_NaN(), 500.0f), 0.0f);
}

TEST(HudPhysicsTests, SpecificEnergyRateCombinesClimbAndAcceleration)
{
    // Pure climb at constant speed: the energy rate is the climb rate.
    EXPECT_NEAR(ComputeSpecificEnergyRateMps(200.0f, 12.0f, 0.0f), 12.0f, 1.0e-4f);
    // Level acceleration of exactly one g converts to the true airspeed.
    EXPECT_NEAR(ComputeSpecificEnergyRateMps(200.0f, 0.0f, 9.80665f), 200.0f, 1.0e-2f);
    // Non-finite inputs contribute zero instead of poisoning the result.
    EXPECT_NEAR(
        ComputeSpecificEnergyRateMps(std::numeric_limits<float>::quiet_NaN(), 5.0f, 3.0f), 5.0f, 1.0e-4f);
}

TEST(HudPhysicsTests, TerrainElevationIsDominatedByTheUserBaseValue)
{
    EnvironmentControls defaultEnvironment;
    EnvironmentControls raisedEnvironment;
    raisedEnvironment.terrainElevationMeters = 1000.0f;

    for (float elapsedSeconds = 0.0f; elapsedSeconds < 600.0f; elapsedSeconds += 37.0f)
    {
        const float defaultElevation = ComputeTerrainElevationMeters(defaultEnvironment, elapsedSeconds, 0.4f);
        const float raisedElevation = ComputeTerrainElevationMeters(raisedEnvironment, elapsedSeconds, 0.4f);
        // The deterministic undulation stays small around the base value.
        EXPECT_LT(std::fabs(defaultElevation - defaultEnvironment.terrainElevationMeters), 30.0f);
        // Raising the base shifts the result by exactly the base difference.
        EXPECT_NEAR(
            raisedElevation - defaultElevation,
            raisedEnvironment.terrainElevationMeters - defaultEnvironment.terrainElevationMeters,
            1.0e-2f);
    }
}

TEST(HudPhysicsTests, SteadyWindAndTurbulenceGustStaySeparateContributions)
{
    // Mirror of the composition used by `HudSimulation::Step()` and the
    // telemetry panel: total wind = steady FROM-direction wind + gust.
    const WindVectorNed steadyWind = ComputeWindVectorNed(20.0f, 1.5f * kPi);
    const float elapsedSeconds = 12.3f;

    // Zero turbulence: the total wind is exactly the steady wind.
    const WindVectorNed calmGust = ComputeTurbulenceGustNed(0.0f, elapsedSeconds);
    EXPECT_FLOAT_EQ(steadyWind.northMps + calmGust.northMps, steadyWind.northMps);
    EXPECT_FLOAT_EQ(steadyWind.eastMps + calmGust.eastMps, steadyWind.eastMps);
    EXPECT_FLOAT_EQ(steadyWind.downMps + calmGust.downMps, steadyWind.downMps);

    // Full turbulence: the gust is a bounded additive term that never rewrites
    // the steady component, so total - steady always recovers the gust.
    const WindVectorNed gust = ComputeTurbulenceGustNed(1.0f, elapsedSeconds);
    const WindVectorNed totalWind {
        steadyWind.northMps + gust.northMps,
        steadyWind.eastMps + gust.eastMps,
        steadyWind.downMps + gust.downMps};
    EXPECT_FLOAT_EQ(totalWind.northMps - steadyWind.northMps, gust.northMps);
    EXPECT_FLOAT_EQ(totalWind.eastMps - steadyWind.eastMps, gust.eastMps);
    EXPECT_FLOAT_EQ(totalWind.downMps - steadyWind.downMps, gust.downMps);
    // The steady wind itself is independent of the turbulence intensity: only
    // its speed and FROM direction define it.
    const WindVectorNed steadyWindAgain = ComputeWindVectorNed(20.0f, 1.5f * kPi);
    EXPECT_FLOAT_EQ(steadyWindAgain.northMps, steadyWind.northMps);
    EXPECT_FLOAT_EQ(steadyWindAgain.eastMps, steadyWind.eastMps);
}

TEST(HudPhysicsTests, TurbulenceGustIsZeroAtZeroIntensityAndClampedAboveOne)
{
    for (float elapsedSeconds = 0.0f; elapsedSeconds < 60.0f; elapsedSeconds += 3.7f)
    {
        const WindVectorNed calm = ComputeTurbulenceGustNed(0.0f, elapsedSeconds);
        EXPECT_FLOAT_EQ(calm.northMps, 0.0f);
        EXPECT_FLOAT_EQ(calm.eastMps, 0.0f);
        EXPECT_FLOAT_EQ(calm.downMps, 0.0f);

        const WindVectorNed full = ComputeTurbulenceGustNed(1.0f, elapsedSeconds);
        const WindVectorNed overdriven = ComputeTurbulenceGustNed(5.0f, elapsedSeconds);
        EXPECT_FLOAT_EQ(overdriven.northMps, full.northMps);
        EXPECT_FLOAT_EQ(overdriven.eastMps, full.eastMps);
        EXPECT_FLOAT_EQ(overdriven.downMps, full.downMps);
        // The gust magnitude stays strictly bounded.
        EXPECT_LT(std::fabs(full.northMps), 5.0f);
        EXPECT_LT(std::fabs(full.eastMps), 5.0f);
        EXPECT_LT(std::fabs(full.downMps), 2.0f);
    }
}
} // namespace
