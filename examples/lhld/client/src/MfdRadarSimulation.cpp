/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the deterministic LHLD radar/airspace simulation.
 */

#include "MfdRadarSimulation.h"

#include <algorithm>
#include <cmath>

namespace lhld
{
namespace
{
constexpr float kRadToDeg = 57.2957795131f;
constexpr float kSweepRatePerSecond = 1.3f;
constexpr float kMaxStepSeconds = 0.1f;
constexpr float kRespawnRangeNm = 88.0f;
constexpr float kRespawnBehindNm = -8.0f;
constexpr float kFeetPerNauticalMile = 6076.12f;
constexpr float kRadarBarSpacingDeg = 2.2f;
constexpr float kRadarBeamHalfHeightDeg = 1.4f;
constexpr float kMinNavRangeNm = 10.0f;
constexpr float kMaxNavRangeNm = 160.0f;
constexpr float kOwnshipTurnRateDegPerSecond = 0.18f;

float Finite(const float value, const float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float Wrap360(float degrees) noexcept
{
    degrees = std::fmod(Finite(degrees, 0.0f), 360.0f);
    if (degrees < 0.0f)
    {
        degrees += 360.0f;
    }
    return degrees;
}

int ValidRadarBars(const int bars) noexcept
{
    if (bars <= 1)
    {
        return 1;
    }
    if (bars <= 2)
    {
        return 2;
    }
    return 4;
}

float RadarVerticalHalfCoverageDeg(const RadarSettings& radar) noexcept
{
    return kRadarBeamHalfHeightDeg +
        (static_cast<float>(ValidRadarBars(radar.scanBars) - 1) * kRadarBarSpacingDeg * 0.5f);
}

bool IsInsideRadarElevationVolume(const RadarContact& contact,
                                  const RadarSettings& radar,
                                  const OwnshipState& ownship) noexcept
{
    const float rangeFt = std::max(1.0f, Finite(contact.rangeNm, 0.0f) * kFeetPerNauticalMile);
    const float altitudeDeltaFt = Finite(contact.altitudeFt, ownship.altitudeFt) - Finite(ownship.altitudeFt, 0.0f);
    const float contactElevationDeg = std::atan2(altitudeDeltaFt, rangeFt) * kRadToDeg;
    return std::fabs(contactElevationDeg - Finite(radar.antennaElevationDeg, 0.0f)) <=
        RadarVerticalHalfCoverageDeg(radar);
}

// Deterministic scripted airspace: eight tracks with coherent closing/crossing
// geometry so the RWS picture stays lively without any randomness.
struct ContactSeed
{
    float rightNm;
    float forwardNm;
    float velRightNmS;
    float velForwardNmS;
    float altitudeFt;
    bool hostile;
};

constexpr ContactSeed kContactSeeds[kMaxContacts] = {
    {-8.0f, 55.0f, 0.010f, -0.200f, 24000.0f, true},
    {6.0f, 40.0f, -0.020f, -0.160f, 18000.0f, true},
    {12.0f, 62.0f, -0.050f, -0.100f, 27000.0f, true},
    {-15.0f, 48.0f, 0.060f, -0.080f, 15000.0f, true},
    {2.0f, 30.0f, 0.000f, -0.140f, 21000.0f, true},
    {-4.0f, 70.0f, 0.020f, -0.180f, 30000.0f, false},
    {18.0f, 35.0f, -0.090f, -0.050f, 12000.0f, true},
    {-20.0f, 58.0f, 0.100f, -0.060f, 26000.0f, true},
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false},
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false},
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false},
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false}};
} // namespace

MfdRadarSimulation::MfdRadarSimulation()
{
    Reset();
}

void MfdRadarSimulation::Reset() noexcept
{
    inputs_ = MfdInputSample {};
    inputs_.masterMode = MasterMode::AirToAir;
    inputs_.activePage = MfdPage::Radar;
    inputs_.ownship.headingDeg = 15.0f;
    inputs_.ownship.speedKts = 420.0f;
    inputs_.ownship.mach = 0.72f;
    inputs_.ownship.altitudeFt = 22000.0f;

    for (std::size_t index = 0; index < kMaxContacts; ++index)
    {
        const ContactSeed& seed = kContactSeeds[index];
        SimContact& track = airspace_[index];
        track.alive = index < 8U;
        track.rightNm = seed.rightNm;
        track.forwardNm = seed.forwardNm;
        track.velRightNmS = seed.velRightNmS;
        track.velForwardNmS = seed.velForwardNmS;
        track.altitudeFt = seed.altitudeFt;
        track.hostile = seed.hostile;
    }

    inputs_.nav.currentSteerpoint = 4;
    inputs_.nav.waypointCount = 6;
    inputs_.nav.rangeScaleNm = 80.0f;
    inputs_.nav.bullseyeBearingDeg = 250.0f;
    inputs_.nav.bullseyeRangeNm = 42.0f;
    const float bearings[kSteerpointCount] = {15.0f, 45.0f, 80.0f, 120.0f, 200.0f, 300.0f};
    const float ranges[kSteerpointCount] = {12.0f, 28.0f, 40.0f, 55.0f, 62.0f, 74.0f};
    for (std::size_t index = 0; index < kSteerpointCount; ++index)
    {
        inputs_.nav.steerpoints[index].number = static_cast<int>(index) + 1;
        inputs_.nav.steerpoints[index].bearingDeg = bearings[index];
        inputs_.nav.steerpoints[index].rangeNm = ranges[index];
    }

    inputs_.stores = StoresState {};
    inputs_.radar = RadarSettings {};
    sweep_ = 0.0f;
    sweepDirection_ = 1.0f;
    RefreshPublishedContacts();
}

void MfdRadarSimulation::ApplyRadarControls(const RadarSettings& controls) noexcept
{
    const float preservedSweep = inputs_.radar.scanSweepNorm;
    inputs_.radar = controls;
    inputs_.radar.scanBars = ValidRadarBars(controls.scanBars);
    inputs_.radar.scanSweepNorm = preservedSweep;
}

void MfdRadarSimulation::SetMasterMode(const MasterMode mode) noexcept
{
    inputs_.masterMode = mode;
}

void MfdRadarSimulation::SetStores(const StoresState& stores) noexcept
{
    inputs_.stores = stores;
}

void MfdRadarSimulation::SetNavState(const NavState& nav) noexcept
{
    inputs_.nav = nav;
    inputs_.nav.waypointCount = std::clamp(nav.waypointCount, 1, static_cast<int>(kSteerpointCount));
    inputs_.nav.currentSteerpoint = std::clamp(nav.currentSteerpoint, 1, inputs_.nav.waypointCount);
    inputs_.nav.rangeScaleNm = std::clamp(Finite(nav.rangeScaleNm, 80.0f), kMinNavRangeNm, kMaxNavRangeNm);
    inputs_.nav.bullseyeBearingDeg = Wrap360(nav.bullseyeBearingDeg);
    inputs_.nav.bullseyeRangeNm = std::clamp(Finite(nav.bullseyeRangeNm, 42.0f), 0.0f, kMaxNavRangeNm);

    for (std::size_t index = 0; index < kSteerpointCount; ++index)
    {
        Steerpoint& steerpoint = inputs_.nav.steerpoints[index];
        steerpoint.number = static_cast<int>(index) + 1;
        steerpoint.bearingDeg = Wrap360(steerpoint.bearingDeg);
        steerpoint.rangeNm = std::clamp(Finite(steerpoint.rangeNm, 0.0f), 0.0f, kMaxNavRangeNm);
    }
}

void MfdRadarSimulation::Step(const float deltaSeconds) noexcept
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f)
    {
        return;
    }
    const float dt = std::min(deltaSeconds, kMaxStepSeconds);

    inputs_.ownship.headingDeg = Wrap360(inputs_.ownship.headingDeg + kOwnshipTurnRateDegPerSecond * dt);
    inputs_.ownship.mach = 0.72f + 0.02f * std::sin(sweep_ * 1.7f);

    for (std::size_t index = 0; index < kMaxContacts; ++index)
    {
        SimContact& track = airspace_[index];
        if (!track.alive)
        {
            continue;
        }

        track.rightNm += track.velRightNmS * dt;
        track.forwardNm += track.velForwardNmS * dt;

        const float range = std::hypot(track.rightNm, track.forwardNm);
        if (range > kRespawnRangeNm || track.forwardNm < kRespawnBehindNm)
        {
            const ContactSeed& seed = kContactSeeds[index];
            track.rightNm = seed.rightNm;
            track.forwardNm = seed.forwardNm;
            track.velRightNmS = seed.velRightNmS;
            track.velForwardNmS = seed.velForwardNmS;
        }
    }

    const float barRateScale = 1.0f / static_cast<float>(ValidRadarBars(inputs_.radar.scanBars));
    sweep_ += sweepDirection_ * kSweepRatePerSecond * barRateScale * dt;
    if (sweep_ > 1.0f)
    {
        sweep_ = 1.0f;
        sweepDirection_ = -1.0f;
    }
    else if (sweep_ < -1.0f)
    {
        sweep_ = -1.0f;
        sweepDirection_ = 1.0f;
    }
    inputs_.radar.scanSweepNorm = sweep_;

    RefreshPublishedContacts();
}

void MfdRadarSimulation::RefreshPublishedContacts() noexcept
{
    for (std::size_t index = 0; index < kMaxContacts; ++index)
    {
        const SimContact& track = airspace_[index];
        RadarContact& contact = inputs_.contacts[index];
        if (!track.alive)
        {
            contact = RadarContact {};
            contact.active = false;
            continue;
        }

        const float range = std::max(0.001f, std::hypot(track.rightNm, track.forwardNm));
        const float bearingFromNoseDeg = std::atan2(track.rightNm, track.forwardNm) * kRadToDeg;
        const float rangeRateNmS = (track.rightNm * track.velRightNmS + track.forwardNm * track.velForwardNmS) / range;
        const float speedNmS = std::hypot(track.velRightNmS, track.velForwardNmS);
        const float trackRelNoseDeg = std::atan2(track.velRightNmS, track.velForwardNmS) * kRadToDeg;

        float aspectDeg = 0.0f;
        if (speedNmS > 1.0e-4f)
        {
            const float losRight = -track.rightNm;
            const float losForward = -track.forwardNm;
            const float dot = track.velRightNmS * losRight + track.velForwardNmS * losForward;
            const float cosAspect = std::clamp(dot / (speedNmS * range), -1.0f, 1.0f);
            aspectDeg = std::acos(cosAspect) * kRadToDeg;
        }

        contact.active = true;
        contact.azimuthNorm = bearingFromNoseDeg / 60.0f;
        contact.rangeNm = range;
        contact.headingDeg = Wrap360(inputs_.ownship.headingDeg + trackRelNoseDeg);
        contact.altitudeFt = track.altitudeFt;
        contact.closureKts = -rangeRateNmS * 3600.0f;
        contact.aspectDeg = aspectDeg;
        contact.speedKts = speedNmS * 3600.0f;
        contact.hostile = track.hostile;
    }
}

int MfdRadarSimulation::NearestContactToCursor() const noexcept
{
    const float halfScanDeg = std::clamp(Finite(inputs_.radar.azScanDeg, 60.0f), 10.0f, 120.0f) * 0.5f;
    const float rangeScaleNm = std::max(1.0f, Finite(inputs_.radar.rangeScaleNm, 40.0f));
    const float cursorBearingDeg = std::clamp(inputs_.radar.cursorAz, -1.0f, 1.0f) * halfScanDeg;
    const float cursorRangeNm = std::clamp(inputs_.radar.cursorRange, 0.0f, 1.0f) * rangeScaleNm;

    int nearest = -1;
    float best = 0.18f;
    for (std::size_t index = 0; index < kMaxContacts; ++index)
    {
        const RadarContact& contact = inputs_.contacts[index];
        if (!contact.active)
        {
            continue;
        }

        const float bearingDeg = contact.azimuthNorm * 60.0f;
        if (std::fabs(bearingDeg) > halfScanDeg || contact.rangeNm > rangeScaleNm ||
            !IsInsideRadarElevationVolume(contact, inputs_.radar, inputs_.ownship))
        {
            continue;
        }

        const float dAz = (bearingDeg - cursorBearingDeg) / halfScanDeg;
        const float dRange = (contact.rangeNm - cursorRangeNm) / rangeScaleNm;
        const float distance = std::hypot(dAz, dRange);
        if (distance < best)
        {
            best = distance;
            nearest = static_cast<int>(index);
        }
    }

    return nearest;
}

const MfdInputSample& MfdRadarSimulation::Inputs() const noexcept
{
    return inputs_;
}
} // namespace lhld
