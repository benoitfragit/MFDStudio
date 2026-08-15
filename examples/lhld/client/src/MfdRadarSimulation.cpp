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
constexpr float kMinNavRangeNm = 10.0f;
constexpr float kMaxNavRangeNm = 160.0f;
constexpr float kOwnshipTurnRateDegPerSecond = 0.18f;
constexpr float kSecondsPerQualityCycle = 18.0f;
constexpr float kExtrapolationStartSeconds = 14.0f;

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

// Deterministic scripted airspace: eight tracks with coherent closing/crossing
// geometry so the RWS picture stays lively without any randomness.
struct TrackSeed
{
    float rightNm;
    float forwardNm;
    float velRightNmS;
    float velForwardNmS;
    float altitudeFt;
    bool hostile;
};

constexpr TrackSeed kTrackSeeds[kMaxRadarTracks] = {
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

    for (std::size_t index = 0; index < kMaxRadarTracks; ++index)
    {
        const TrackSeed& seed = kTrackSeeds[index];
        SimTrack& track = airspace_[index];
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
    sweepFraction_ = 0.0f;
    sweepDirection_ = 1.0f;
    elapsedSeconds_ = 0.0f;
    RefreshPublishedTracks();
}

void MfdRadarSimulation::ApplyRadarControls(const RadarSettings& controls) noexcept
{
    const float preservedAntennaAzimuth = inputs_.radar.antennaAzimuthDeg;
    inputs_.radar = controls;
    inputs_.radar.scanBars = ValidRadarBars(controls.scanBars);
    inputs_.radar.antennaAzimuthDeg = preservedAntennaAzimuth;
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
    elapsedSeconds_ = std::fmod(elapsedSeconds_ + dt, 3600.0f);
    inputs_.ownship.mach = 0.72f + 0.02f * std::sin(sweepFraction_ * 1.7f);

    for (std::size_t index = 0; index < kMaxRadarTracks; ++index)
    {
        SimTrack& track = airspace_[index];
        if (!track.alive)
        {
            continue;
        }

        track.rightNm += track.velRightNmS * dt;
        track.forwardNm += track.velForwardNmS * dt;

        const float range = std::hypot(track.rightNm, track.forwardNm);
        if (range > kRespawnRangeNm || track.forwardNm < kRespawnBehindNm)
        {
            const TrackSeed& seed = kTrackSeeds[index];
            track.rightNm = seed.rightNm;
            track.forwardNm = seed.forwardNm;
            track.velRightNmS = seed.velRightNmS;
            track.velForwardNmS = seed.velForwardNmS;
        }
    }

    const float barRateScale = 1.0f / static_cast<float>(ValidRadarBars(inputs_.radar.scanBars));
    sweepFraction_ += sweepDirection_ * kSweepRatePerSecond * barRateScale * dt;
    if (sweepFraction_ > 1.0f)
    {
        sweepFraction_ = 1.0f;
        sweepDirection_ = -1.0f;
    }
    else if (sweepFraction_ < -1.0f)
    {
        sweepFraction_ = -1.0f;
        sweepDirection_ = 1.0f;
    }
    const float halfScanDeg = std::clamp(Finite(inputs_.radar.azScanDeg, 60.0f), 10.0f, 120.0f) * 0.5f;
    inputs_.radar.antennaAzimuthDeg = sweepFraction_ * halfScanDeg;

    RefreshPublishedTracks();
}

void MfdRadarSimulation::RefreshPublishedTracks() noexcept
{
    for (std::size_t index = 0; index < kMaxRadarTracks; ++index)
    {
        const SimTrack& simulatedTrack = airspace_[index];
        RadarTrack& radarTrack = inputs_.tracks[index];
        if (!simulatedTrack.alive)
        {
            radarTrack = RadarTrack {};
            radarTrack.active = false;
            continue;
        }

        const float horizontalRangeNm =
            std::max(0.001f, std::hypot(simulatedTrack.rightNm, simulatedTrack.forwardNm));
        const float altitudeDeltaNm =
            (simulatedTrack.altitudeFt - inputs_.ownship.altitudeFt) / 6076.12f;
        const float slantRangeNm = std::hypot(horizontalRangeNm, altitudeDeltaNm);
        const float bearingFromNoseDeg =
            std::atan2(simulatedTrack.rightNm, simulatedTrack.forwardNm) * kRadToDeg;
        const float elevationDeg = std::atan2(altitudeDeltaNm, horizontalRangeNm) * kRadToDeg;
        const float rangeRateNmS =
            (simulatedTrack.rightNm * simulatedTrack.velRightNmS +
             simulatedTrack.forwardNm * simulatedTrack.velForwardNmS) /
            horizontalRangeNm;
        const float speedNmS = std::hypot(simulatedTrack.velRightNmS, simulatedTrack.velForwardNmS);
        const float trackRelNoseDeg =
            std::atan2(simulatedTrack.velRightNmS, simulatedTrack.velForwardNmS) * kRadToDeg;

        float aspectDeg = 0.0f;
        if (speedNmS > 1.0e-4f)
        {
            const float losRight = -simulatedTrack.rightNm;
            const float losForward = -simulatedTrack.forwardNm;
            const float dot =
                simulatedTrack.velRightNmS * losRight + simulatedTrack.velForwardNmS * losForward;
            const float cosAspect = std::clamp(dot / (speedNmS * horizontalRangeNm), -1.0f, 1.0f);
            aspectDeg = std::acos(cosAspect) * kRadToDeg;
        }

        const float qualityPhase =
            std::fmod(elapsedSeconds_ + static_cast<float>(index) * 2.3f, kSecondsPerQualityCycle);
        radarTrack.active = true;
        radarTrack.rangeNm = slantRangeNm;
        radarTrack.azimuthDeg = bearingFromNoseDeg;
        radarTrack.elevationDeg = elevationDeg;
        radarTrack.headingDeg = Wrap360(inputs_.ownship.headingDeg + trackRelNoseDeg);
        radarTrack.closureKts = -rangeRateNmS * 3600.0f;
        radarTrack.aspectDeg = aspectDeg;
        radarTrack.speedKts = speedNmS * 3600.0f;
        radarTrack.hostile = simulatedTrack.hostile;
        radarTrack.quality = qualityPhase >= kExtrapolationStartSeconds
            ? RadarTrackQuality::Extrapolated
            : RadarTrackQuality::Measured;
    }
}

const MfdInputSample& MfdRadarSimulation::Inputs() const noexcept
{
    return inputs_;
}
} // namespace lhld
