/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "HudSpatialTransform.h"
#include "hud/HudProjection.h"
#include "hud_main/HudSimulation.h"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegreesToRadians = kPi / 180.0f;

hud::detail::NedVector BodyForwardNed(const float yawRad, const float pitchRad) noexcept
{
    return hud::detail::NedVector {std::cos(yawRad) * std::cos(pitchRad), std::sin(yawRad) * std::cos(pitchRad),
                                   -std::sin(pitchRad)};
}

float NormalizeRadiansPi(float value) noexcept
{
    value = std::fmod(value + kPi, 2.0f * kPi);
    if (value < 0.0f)
    {
        value += 2.0f * kPi;
    }
    return value - kPi;
}
} // namespace

TEST(HudSpatialTransformTests, IdentityKeepsNedComponentsInBodyAxes)
{
    const hud::AircraftInputSample aircraft;
    const hud::detail::BodyVector body =
        hud::detail::RotateNedToBody(aircraft, hud::detail::NedVector {10.0f, -3.0f, 2.0f});

    EXPECT_FLOAT_EQ(body.forward, 10.0f);
    EXPECT_FLOAT_EQ(body.right, -3.0f);
    EXPECT_FLOAT_EQ(body.down, 2.0f);
}

TEST(HudSpatialTransformTests, KnownNinetyDegreeAttitudesResolveBodyForward)
{
    hud::AircraftInputSample yawed;
    yawed.yawRad = 90.0f * kDegreesToRadians;
    const hud::detail::BodyVector yawBody =
        hud::detail::RotateNedToBody(yawed, hud::detail::NedVector {0.0f, 20.0f, 0.0f});
    EXPECT_NEAR(yawBody.forward, 20.0f, 1.0e-5f);
    EXPECT_NEAR(yawBody.right, 0.0f, 1.0e-5f);

    hud::AircraftInputSample pitched;
    pitched.pitchRad = 90.0f * kDegreesToRadians;
    const hud::detail::BodyVector pitchBody =
        hud::detail::RotateNedToBody(pitched, hud::detail::NedVector {0.0f, 0.0f, -20.0f});
    EXPECT_NEAR(pitchBody.forward, 20.0f, 1.0e-5f);
    EXPECT_NEAR(pitchBody.down, 0.0f, 1.0e-5f);
}

TEST(HudSpatialTransformTests, EquivalentEulerRepresentationsProduceSameBodyVector)
{
    hud::AircraftInputSample first;
    first.yawRad = 12.0f * kDegreesToRadians;
    first.pitchRad = 100.0f * kDegreesToRadians;
    first.rollRad = -7.0f * kDegreesToRadians;
    hud::AircraftInputSample second;
    second.yawRad = -168.0f * kDegreesToRadians;
    second.pitchRad = 80.0f * kDegreesToRadians;
    second.rollRad = 173.0f * kDegreesToRadians;
    const hud::detail::NedVector vector {40.0f, -12.0f, 8.0f};

    const hud::detail::BodyVector firstBody = hud::detail::RotateNedToBody(first, vector);
    const hud::detail::BodyVector secondBody = hud::detail::RotateNedToBody(second, vector);

    EXPECT_NEAR(firstBody.forward, secondBody.forward, 1.0e-4f);
    EXPECT_NEAR(firstBody.right, secondBody.right, 1.0e-4f);
    EXPECT_NEAR(firstBody.down, secondBody.down, 1.0e-4f);
}

TEST(HudSpatialTransformTests, DirectionRemainsContinuousAroundVerticalAndPiBoundaries)
{
    constexpr float epsilonRad = 0.001f;
    for (const float boundaryRad : {0.5f * kPi, kPi})
    {
        hud::AircraftInputSample before;
        before.pitchRad = boundaryRad - epsilonRad;
        hud::AircraftInputSample after;
        after.pitchRad = boundaryRad + epsilonRad;
        const hud::detail::NedVector velocityBefore = BodyForwardNed(0.0f, before.pitchRad);
        const hud::detail::NedVector velocityAfter = BodyForwardNed(0.0f, after.pitchRad);

        const hud::detail::BodyAngularDirection first =
            hud::detail::ResolveBodyAngularDirection(before, velocityBefore, 0.1f);
        const hud::detail::BodyAngularDirection second =
            hud::detail::ResolveBodyAngularDirection(after, velocityAfter, 0.1f);

        ASSERT_TRUE(first.valid);
        ASSERT_TRUE(second.valid);
        EXPECT_NEAR(first.azimuthRad, second.azimuthRad, 1.0e-4f);
        EXPECT_NEAR(first.elevationRad, second.elevationRad, 1.0e-4f);
    }
}

TEST(HudFpmProjectionTests, ForwardVelocityPlacesFpmAtCenter)
{
    hud::HudInputSample input;
    input.aircraft.northSpeedMps = 200.0f;

    const hud::HudFrame frame = hud::BuildHudFrame(input);

    EXPECT_TRUE(hud::detail::ResolveAircraftVelocityDirection(input.aircraft).valid);
    EXPECT_FALSE(frame.attitude.fpmLimited);
    EXPECT_NEAR(frame.attitude.fpmPosition.x, 0.0f, 1.0e-6f);
    EXPECT_NEAR(frame.attitude.fpmPosition.y, 0.0f, 1.0e-6f);
}

TEST(HudFpmProjectionTests, ClimbDescentAndRightDriftUseVelocityDirection)
{
    hud::HudInputSample climbing;
    climbing.aircraft.northSpeedMps = 200.0f;
    climbing.aircraft.downSpeedMps = -10.0f;
    hud::HudInputSample descending = climbing;
    descending.aircraft.downSpeedMps = 10.0f;
    hud::HudInputSample drifting = climbing;
    drifting.aircraft.downSpeedMps = 0.0f;
    drifting.aircraft.eastSpeedMps = 10.0f;

    EXPECT_GT(hud::BuildHudFrame(climbing).attitude.fpmPosition.y, 0.0f);
    EXPECT_LT(hud::BuildHudFrame(descending).attitude.fpmPosition.y, 0.0f);
    EXPECT_GT(hud::BuildHudFrame(drifting).attitude.fpmPosition.x, 0.0f);
}

TEST(HudFpmProjectionTests, PureRollDoesNotMoveVelocityAlignedWithNose)
{
    for (const float rollDegrees : {0.0f, 45.0f, 90.0f, 179.9f, -179.9f})
    {
        hud::HudInputSample input;
        input.aircraft.rollRad = rollDegrees * kDegreesToRadians;
        input.aircraft.northSpeedMps = 200.0f;

        const hud::HudFrame frame = hud::BuildHudFrame(input);

        ASSERT_TRUE(hud::detail::ResolveAircraftVelocityDirection(input.aircraft).valid);
        EXPECT_NEAR(frame.attitude.fpmPosition.x, 0.0f, 1.0e-5f);
        EXPECT_NEAR(frame.attitude.fpmPosition.y, 0.0f, 1.0e-5f);
    }
}

TEST(HudFpmProjectionTests, EquivalentAttitudesAroundPiProduceSameFpm)
{
    hud::HudInputSample first;
    first.aircraft.pitchRad = 179.9f * kDegreesToRadians;
    const hud::detail::NedVector forward = BodyForwardNed(0.0f, first.aircraft.pitchRad);
    first.aircraft.northSpeedMps = forward.north * 200.0f;
    first.aircraft.eastSpeedMps = forward.east * 200.0f;
    first.aircraft.downSpeedMps = forward.down * 200.0f;
    hud::HudInputSample second = first;
    second.aircraft.pitchRad = -180.1f * kDegreesToRadians;

    const hud::HudFrame firstFrame = hud::BuildHudFrame(first);
    const hud::HudFrame secondFrame = hud::BuildHudFrame(second);

    ASSERT_TRUE(hud::detail::ResolveAircraftVelocityDirection(first.aircraft).valid);
    ASSERT_TRUE(hud::detail::ResolveAircraftVelocityDirection(second.aircraft).valid);
    EXPECT_NEAR(firstFrame.attitude.fpmPosition.x, secondFrame.attitude.fpmPosition.x, 1.0e-5f);
    EXPECT_NEAR(firstFrame.attitude.fpmPosition.y, secondFrame.attitude.fpmPosition.y, 1.0e-5f);
}

TEST(HudFpmProjectionTests, RearNullAndNonFinitePhysicalInputsHideFpmSafely)
{
    hud::HudInputSample rear;
    rear.aircraft.northSpeedMps = -200.0f;
    hud::HudInputSample zero;
    hud::HudInputSample nonFinite;
    nonFinite.aircraft.northSpeedMps = std::numeric_limits<float>::quiet_NaN();
    hud::HudInputSample nonFiniteAttitude;
    nonFiniteAttitude.aircraft.northSpeedMps = 100.0f;
    nonFiniteAttitude.aircraft.pitchRad = std::numeric_limits<float>::infinity();

    for (const hud::HudInputSample& input : {rear, zero, nonFinite, nonFiniteAttitude})
    {
        const hud::HudFrame frame = hud::BuildHudFrame(input);
        EXPECT_FALSE(hud::detail::ResolveAircraftVelocityDirection(input.aircraft).valid);
        EXPECT_FALSE(frame.attitude.fpmLimited);
        EXPECT_TRUE(std::isfinite(frame.attitude.fpmPosition.x));
        EXPECT_TRUE(std::isfinite(frame.attitude.fpmPosition.y));
    }
}

TEST(HudSimulationPhysicsTests, LoopReversesVelocityWithoutAttitudeOrFpmDiscontinuity)
{
    hud_main::HudSimulation simulation;
    hud_main::SimulationControls controls;
    controls.pilot.pitchCommand = 1.0f;
    simulation.SetSimulationControls(controls);

    float accumulatedPitchRadians = 0.0f;
    float previousPitchRadians = simulation.Inputs().aircraft.pitchRad;
    float previousFpmElevationRadians = 0.0f;
    bool previousFpmValid = false;
    bool horizontalVelocityReversed = false;
    bool attitudeBecameInverted = false;
    int visibleFpmFrameCount = 0;
    int hiddenFpmFrameCount = 0;
    float maximumVelocityDeltaMps = 0.0f;
    float maximumFpmDeltaRadians = 0.0f;
    hud::detail::NedVector previousVelocity {simulation.Inputs().aircraft.northSpeedMps,
                                             simulation.Inputs().aircraft.eastSpeedMps,
                                             simulation.Inputs().aircraft.downSpeedMps};

    for (int tick = 0; tick < 2500 && accumulatedPitchRadians < 2.0f * kPi; ++tick)
    {
        simulation.Step();
        const hud::AircraftInputSample& aircraft = simulation.Inputs().aircraft;
        accumulatedPitchRadians += std::fabs(NormalizeRadiansPi(aircraft.pitchRad - previousPitchRadians));
        previousPitchRadians = aircraft.pitchRad;
        horizontalVelocityReversed = horizontalVelocityReversed || aircraft.northSpeedMps < -1.0f;
        attitudeBecameInverted = attitudeBecameInverted || simulation.BuildHudFrame().attitude.inverted;

        const float northDelta = aircraft.northSpeedMps - previousVelocity.north;
        const float eastDelta = aircraft.eastSpeedMps - previousVelocity.east;
        const float downDelta = aircraft.downSpeedMps - previousVelocity.down;
        maximumVelocityDeltaMps =
            std::max(maximumVelocityDeltaMps,
                     std::sqrt(northDelta * northDelta + eastDelta * eastDelta + downDelta * downDelta));
        previousVelocity =
            hud::detail::NedVector {aircraft.northSpeedMps, aircraft.eastSpeedMps, aircraft.downSpeedMps};

        const hud::detail::BodyAngularDirection fpm =
            hud::detail::ResolveBodyAngularDirection(aircraft, previousVelocity, 0.1f);
        if (fpm.valid && previousFpmValid)
        {
            maximumFpmDeltaRadians = std::max(
                maximumFpmDeltaRadians, std::fabs(NormalizeRadiansPi(fpm.elevationRad - previousFpmElevationRadians)));
        }
        visibleFpmFrameCount += fpm.valid ? 1 : 0;
        hiddenFpmFrameCount += fpm.valid ? 0 : 1;
        previousFpmElevationRadians = fpm.elevationRad;
        previousFpmValid = fpm.valid;

        ASSERT_TRUE(std::isfinite(aircraft.pitchRad));
        ASSERT_TRUE(std::isfinite(aircraft.rollRad));
        ASSERT_TRUE(std::isfinite(aircraft.northSpeedMps));
        ASSERT_TRUE(std::isfinite(aircraft.eastSpeedMps));
        ASSERT_TRUE(std::isfinite(aircraft.downSpeedMps));
    }

    EXPECT_GE(accumulatedPitchRadians, 2.0f * kPi);
    EXPECT_TRUE(horizontalVelocityReversed);
    EXPECT_TRUE(attitudeBecameInverted);
    EXPECT_GT(visibleFpmFrameCount, 0);
    EXPECT_GT(hiddenFpmFrameCount, 0);
    EXPECT_LT(maximumVelocityDeltaMps, 5.0f);
    EXPECT_LT(maximumFpmDeltaRadians, 5.0f * kDegreesToRadians);
}

TEST(HudSimulationPhysicsTests, HalfLoopThenRollKeepsPhysicalFpmContinuous)
{
    hud_main::HudSimulation simulation;
    hud_main::SimulationControls controls;
    controls.pilot.pitchCommand = 1.0f;
    controls.pilot.throttle = 0.95f;
    controls.pilot.afterburnerRequested = true;
    simulation.SetSimulationControls(controls);

    float accumulatedPitchRadians = 0.0f;
    float previousPitchRadians = simulation.Inputs().aircraft.pitchRad;
    for (int tick = 0; tick < 1500 && accumulatedPitchRadians < kPi; ++tick)
    {
        simulation.Step();
        const float pitchRadians = simulation.Inputs().aircraft.pitchRad;
        accumulatedPitchRadians += std::fabs(NormalizeRadiansPi(pitchRadians - previousPitchRadians));
        previousPitchRadians = pitchRadians;
    }
    ASSERT_GE(accumulatedPitchRadians, kPi);

    controls.pilot.pitchCommand = 0.20f;
    controls.pilot.rollCommand = 1.0f;
    simulation.SetSimulationControls(controls);
    const hud::AircraftInputSample& initialAircraft = simulation.Inputs().aircraft;
    hud::detail::BodyAngularDirection previousDirection = hud::detail::ResolveBodyAngularDirection(
        initialAircraft,
        hud::detail::NedVector {initialAircraft.northSpeedMps, initialAircraft.eastSpeedMps,
                                initialAircraft.downSpeedMps},
        0.1f);
    ASSERT_TRUE(previousDirection.valid);

    float maximumAngularDeltaRadians = 0.0f;
    for (int tick = 0; tick < 500; ++tick)
    {
        simulation.Step();
        const hud::AircraftInputSample& aircraft = simulation.Inputs().aircraft;
        const hud::detail::BodyAngularDirection direction = hud::detail::ResolveBodyAngularDirection(
            aircraft, hud::detail::NedVector {aircraft.northSpeedMps, aircraft.eastSpeedMps, aircraft.downSpeedMps},
            0.1f);
        ASSERT_TRUE(direction.valid);
        const float azimuthDelta = NormalizeRadiansPi(direction.azimuthRad - previousDirection.azimuthRad);
        const float elevationDelta = NormalizeRadiansPi(direction.elevationRad - previousDirection.elevationRad);
        maximumAngularDeltaRadians = std::max(maximumAngularDeltaRadians, std::hypot(azimuthDelta, elevationDelta));
        previousDirection = direction;

        const hud::HudFrame frame = simulation.BuildHudFrame();
        ASSERT_TRUE(hud::detail::ResolveAircraftVelocityDirection(aircraft).valid);
        ASSERT_TRUE(std::isfinite(frame.attitude.fpmPosition.x));
        ASSERT_TRUE(std::isfinite(frame.attitude.fpmPosition.y));
    }

    EXPECT_LT(maximumAngularDeltaRadians, 5.0f * kDegreesToRadians);
}
