/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "hud_main/HudAircraftDynamics.h"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegreesToRadians = kPi / 180.0;
constexpr double kStepSeconds = 0.02;

double QuaternionNorm(const hud_main::Quaterniond& value) noexcept
{
    return std::sqrt(value.w * value.w + value.x * value.x + value.y * value.y + value.z * value.z);
}

double Dot(const hud_main::Vec3d& first, const hud_main::Vec3d& second) noexcept
{
    return first.x * second.x + first.y * second.y + first.z * second.z;
}
} // namespace

TEST(HudAircraftDynamicsTests, QuaternionRotationsRoundTripAcrossCombinedAttitude)
{
    const hud_main::EulerAnglesRad attitude {37.0 * kDegreesToRadians, 91.0 * kDegreesToRadians,
                                             -28.0 * kDegreesToRadians};
    const hud_main::Quaterniond quaternion = hud_main::QuaternionFromEuler321(attitude);
    const hud_main::Vec3d body {123.0, -17.0, 42.0};

    const hud_main::Vec3d ned = hud_main::RotateBodyToNed(quaternion, body);
    const hud_main::Vec3d roundTrip = hud_main::RotateNedToBody(quaternion, ned);

    EXPECT_NEAR(roundTrip.x, body.x, 1.0e-9);
    EXPECT_NEAR(roundTrip.y, body.y, 1.0e-9);
    EXPECT_NEAR(roundTrip.z, body.z, 1.0e-9);
}

TEST(HudAircraftDynamicsTests, EquivalentEulerBranchesProduceTheSameOrientation)
{
    const hud_main::EulerAnglesRad beyondVertical {12.0 * kDegreesToRadians, 100.0 * kDegreesToRadians,
                                                   -7.0 * kDegreesToRadians};
    const hud_main::EulerAnglesRad canonical {-168.0 * kDegreesToRadians, 80.0 * kDegreesToRadians,
                                              173.0 * kDegreesToRadians};
    const hud_main::Vec3d body {1.0, 2.0, 3.0};

    const hud_main::Vec3d first = hud_main::RotateBodyToNed(hud_main::QuaternionFromEuler321(beyondVertical), body);
    const hud_main::Vec3d second = hud_main::RotateBodyToNed(hud_main::QuaternionFromEuler321(canonical), body);

    EXPECT_NEAR(first.x, second.x, 1.0e-9);
    EXPECT_NEAR(first.y, second.y, 1.0e-9);
    EXPECT_NEAR(first.z, second.z, 1.0e-9);
}

TEST(HudAircraftDynamicsTests, ForceFreeStateKeepsVelocity)
{
    hud_main::AircraftDynamicsConfig config;
    config.wingAreaSquareMeters = 0.0;
    config.maximumDryThrustNewtons = 0.0;
    config.maximumAfterburnerThrustNewtons = 0.0;
    hud_main::HudAircraftDynamics dynamics(config);
    hud_main::AircraftDynamicState state;
    state.velocityNedMps = hud_main::Vec3d {140.0, -12.0, 4.0};
    dynamics.Reset(state);
    hud_main::AircraftEnvironmentInput environment;
    environment.gravityMps2 = 0.0;

    for (int tick = 0; tick < 500; ++tick)
    {
        dynamics.Step(hud_main::AircraftControlInput {}, environment, kStepSeconds);
    }

    EXPECT_NEAR(dynamics.State().velocityNedMps.x, state.velocityNedMps.x, 1.0e-9);
    EXPECT_NEAR(dynamics.State().velocityNedMps.y, state.velocityNedMps.y, 1.0e-9);
    EXPECT_NEAR(dynamics.State().velocityNedMps.z, state.velocityNedMps.z, 1.0e-9);
}

TEST(HudAircraftDynamicsTests, ConstantBodyThrustAcceleratesAlongBodyForward)
{
    hud_main::AircraftDynamicsConfig config;
    config.massKg = 1000.0;
    config.wingAreaSquareMeters = 0.0;
    config.maximumDryThrustNewtons = 1000.0;
    config.throttleResponseRatePerSecond = 1000.0;
    hud_main::HudAircraftDynamics dynamics(config);
    hud_main::AircraftDynamicState state;
    state.throttleRatio = 1.0;
    dynamics.Reset(state);
    hud_main::AircraftControlInput controls;
    controls.throttleRatio = 1.0;
    hud_main::AircraftEnvironmentInput environment;
    environment.gravityMps2 = 0.0;

    for (int tick = 0; tick < 50; ++tick)
    {
        dynamics.Step(controls, environment, kStepSeconds);
    }

    EXPECT_NEAR(dynamics.State().velocityNedMps.x, 1.0, 1.0e-9);
    EXPECT_NEAR(dynamics.State().velocityNedMps.y, 0.0, 1.0e-9);
    EXPECT_NEAR(dynamics.State().velocityNedMps.z, 0.0, 1.0e-9);
}

TEST(HudAircraftDynamicsTests, BodyThrustFollowsPitchedAttitudeInNed)
{
    hud_main::AircraftDynamicsConfig config;
    config.massKg = 1000.0;
    config.wingAreaSquareMeters = 0.0;
    config.maximumDryThrustNewtons = 1000.0;
    config.throttleResponseRatePerSecond = 1000.0;
    hud_main::HudAircraftDynamics dynamics(config);
    hud_main::AircraftDynamicState state;
    state.attitudeBodyToNed =
        hud_main::QuaternionFromEuler321(hud_main::EulerAnglesRad {0.0, 90.0 * kDegreesToRadians, 0.0});
    state.throttleRatio = 1.0;
    dynamics.Reset(state);
    hud_main::AircraftControlInput controls;
    controls.throttleRatio = 1.0;
    hud_main::AircraftEnvironmentInput environment;
    environment.gravityMps2 = 0.0;

    for (int tick = 0; tick < 50; ++tick)
    {
        dynamics.Step(controls, environment, kStepSeconds);
    }

    EXPECT_NEAR(dynamics.State().velocityNedMps.x, 0.0, 1.0e-9);
    EXPECT_NEAR(dynamics.State().velocityNedMps.y, 0.0, 1.0e-9);
    EXPECT_NEAR(dynamics.State().velocityNedMps.z, -1.0, 1.0e-9);
}

TEST(HudAircraftDynamicsTests, GravityAcceleratesAlongNedDown)
{
    hud_main::AircraftDynamicsConfig config;
    config.wingAreaSquareMeters = 0.0;
    config.maximumDryThrustNewtons = 0.0;
    hud_main::HudAircraftDynamics dynamics(config);
    hud_main::AircraftEnvironmentInput environment;

    for (int tick = 0; tick < 50; ++tick)
    {
        dynamics.Step(hud_main::AircraftControlInput {}, environment, kStepSeconds);
    }

    EXPECT_NEAR(dynamics.State().velocityNedMps.x, 0.0, 1.0e-9);
    EXPECT_NEAR(dynamics.State().velocityNedMps.y, 0.0, 1.0e-9);
    EXPECT_NEAR(dynamics.State().velocityNedMps.z, environment.gravityMps2, 1.0e-9);
}

TEST(HudAircraftDynamicsTests, AerodynamicForceOpposesAirVelocityThroughDrag)
{
    hud_main::HudAircraftDynamics dynamics;
    hud_main::AircraftDynamicState state;
    state.velocityNedMps = hud_main::Vec3d {180.0, 25.0, 12.0};
    dynamics.Reset(state);
    hud_main::AircraftEnvironmentInput environment;

    dynamics.Step(hud_main::AircraftControlInput {}, environment, kStepSeconds);

    EXPECT_LT(Dot(dynamics.Telemetry().aerodynamicForceBodyNewtons, dynamics.Telemetry().airVelocityBodyMps), 0.0);
    EXPECT_GT(dynamics.Telemetry().dragCoefficient, 0.0);
}

TEST(HudAircraftDynamicsTests, PositiveAngleOfAttackProducesUpwardBodyForce)
{
    hud_main::AircraftDynamicsConfig config;
    config.maximumDryThrustNewtons = 0.0;
    config.maximumAfterburnerThrustNewtons = 0.0;
    hud_main::HudAircraftDynamics dynamics(config);
    hud_main::AircraftDynamicState state;
    state.velocityNedMps = hud_main::Vec3d {180.0, 0.0, 18.0};
    dynamics.Reset(state);
    hud_main::AircraftEnvironmentInput environment;
    environment.gravityMps2 = 0.0;

    dynamics.Step(hud_main::AircraftControlInput {}, environment, kStepSeconds);

    EXPECT_GT(dynamics.Telemetry().angleOfAttackRad, 0.0);
    EXPECT_GT(dynamics.Telemetry().liftCoefficient, 0.0);
    EXPECT_LT(dynamics.Telemetry().aerodynamicForceBodyNewtons.z, 0.0);
}

TEST(HudAircraftDynamicsTests, MatchingWindAndGroundVelocityProduceZeroAirspeed)
{
    hud_main::HudAircraftDynamics dynamics;
    hud_main::AircraftDynamicState state;
    state.velocityNedMps = hud_main::Vec3d {30.0, -8.0, 1.0};
    dynamics.Reset(state);
    hud_main::AircraftEnvironmentInput environment;
    environment.windVelocityNedMps = state.velocityNedMps;
    environment.gravityMps2 = 0.0;

    dynamics.Step(hud_main::AircraftControlInput {}, environment, kStepSeconds);

    EXPECT_NEAR(dynamics.Telemetry().trueAirspeedMps, 0.0, 1.0e-12);
    EXPECT_NEAR(hud_main::VectorLength(dynamics.Telemetry().aerodynamicForceBodyNewtons), 0.0, 1.0e-12);
}

TEST(HudAircraftDynamicsTests, LongMomentIntegrationKeepsQuaternionNormalizedAndStateFinite)
{
    hud_main::HudAircraftDynamics dynamics;
    hud_main::AircraftDynamicState state;
    state.positionNedMeters = hud_main::Vec3d {0.0, 0.0, -6000.0};
    state.velocityNedMps = hud_main::Vec3d {230.0, 0.0, 0.0};
    dynamics.Reset(state);
    hud_main::AircraftControlInput controls;
    controls.pitchCommand = 0.45;
    controls.rollCommand = 0.35;
    controls.yawCommand = -0.12;
    controls.throttleRatio = 0.85;
    hud_main::AircraftEnvironmentInput environment;

    for (int tick = 0; tick < 20000; ++tick)
    {
        dynamics.Step(controls, environment, kStepSeconds);
        ASSERT_TRUE(hud_main::IsFinite(dynamics.State().positionNedMeters));
        ASSERT_TRUE(hud_main::IsFinite(dynamics.State().velocityNedMps));
        ASSERT_TRUE(hud_main::IsFinite(dynamics.State().attitudeBodyToNed));
        ASSERT_TRUE(hud_main::IsFinite(dynamics.State().angularVelocityBodyRadPerSecond));
        ASSERT_NEAR(QuaternionNorm(dynamics.State().attitudeBodyToNed), 1.0, 1.0e-10);
    }
}

TEST(HudAircraftDynamicsTests, NonFiniteInputsCannotPoisonThePhysicalState)
{
    hud_main::HudAircraftDynamics dynamics;
    hud_main::AircraftDynamicState state;
    state.velocityNedMps = hud_main::Vec3d {210.0, 0.0, 0.0};
    dynamics.Reset(state);
    hud_main::AircraftControlInput controls;
    controls.pitchCommand = std::numeric_limits<double>::quiet_NaN();
    controls.rollCommand = std::numeric_limits<double>::infinity();
    controls.yawCommand = -std::numeric_limits<double>::infinity();
    controls.throttleRatio = std::numeric_limits<double>::quiet_NaN();
    hud_main::AircraftEnvironmentInput environment;
    environment.windVelocityNedMps.x = std::numeric_limits<double>::quiet_NaN();
    environment.airDensityKgPerM3 = std::numeric_limits<double>::infinity();
    environment.gravityMps2 = std::numeric_limits<double>::quiet_NaN();

    dynamics.Step(controls, environment, kStepSeconds);

    EXPECT_TRUE(hud_main::IsFinite(dynamics.State().positionNedMeters));
    EXPECT_TRUE(hud_main::IsFinite(dynamics.State().velocityNedMps));
    EXPECT_TRUE(hud_main::IsFinite(dynamics.State().attitudeBodyToNed));
    EXPECT_TRUE(hud_main::IsFinite(dynamics.State().angularVelocityBodyRadPerSecond));
    EXPECT_NEAR(QuaternionNorm(dynamics.State().attitudeBodyToNed), 1.0, 1.0e-12);

    const hud_main::AircraftDynamicState stateAfterValidStep = dynamics.State();
    dynamics.Step(controls, environment, std::numeric_limits<double>::quiet_NaN());
    EXPECT_DOUBLE_EQ(dynamics.State().positionNedMeters.x, stateAfterValidStep.positionNedMeters.x);
    EXPECT_DOUBLE_EQ(dynamics.State().velocityNedMps.x, stateAfterValidStep.velocityNedMps.x);
}
