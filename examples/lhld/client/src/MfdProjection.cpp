/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the stateless LHLD projection layer.
 */

#include "MfdProjection.h"
#include "MfdRadarGeometry.h"

#include <algorithm>
#include <cmath>

namespace lhld
{
namespace
{
// B-scope geometry authored in radar.json: ownship at the bottom, azimuth
// spread horizontally, range vertically.
constexpr float kScopeHalfWidth = 0.46f;
constexpr float kScopeTop = 0.75f;
constexpr float kScopeBottom = -0.75f;
constexpr float kScopeHeight = kScopeTop - kScopeBottom;
constexpr float kFcrDisplayHalfAngleDeg = 60.0f;
constexpr float kMaxElevationDeg = 60.0f;
constexpr float kFeetPerNauticalMile = 6076.12f;
constexpr float kCompassRadius = 0.72f;
constexpr float kDegToRad = 0.01745329252f;
constexpr float kRadToDeg = 57.2957795131f;

float Clamp(const float value, const float low, const float high) noexcept
{
    return std::max(low, std::min(value, high));
}

// Radar/nav inputs are semantic and should be finite; this guard keeps a single
// non-finite value from propagating a NaN into published UI coordinates.
float Finite(const float value, const float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float WrapDegrees(float degrees) noexcept
{
    degrees = Finite(degrees, 0.0f);
    degrees = std::fmod(degrees, 360.0f);
    if (degrees > 180.0f)
    {
        degrees -= 360.0f;
    }
    else if (degrees < -180.0f)
    {
        degrees += 360.0f;
    }
    return degrees;
}

float RangeFraction(const float rangeNm, const float rangeScaleNm) noexcept
{
    const float scale = std::max(1.0f, Finite(rangeScaleNm, 40.0f));
    return Finite(rangeNm, 0.0f) / scale;
}

float DerivedAltitudeThousandsFt(const RadarTrack& track, const OwnshipState& ownship) noexcept
{
    const float rangeFt = std::max(0.0f, Finite(track.rangeNm, 0.0f)) * kFeetPerNauticalMile;
    const float elevationRad = Finite(track.elevationDeg, 0.0f) * kDegToRad;
    const float altitudeFt = Finite(ownship.altitudeFt, 0.0f) + rangeFt * std::sin(elevationRad);
    return altitudeFt / 1000.0f;
}

float SearchAltitudeThousandsFt(const OwnshipState& ownship,
                                const float rangeNm,
                                const float elevationDeg) noexcept
{
    const float rangeFt = std::max(0.0f, Finite(rangeNm, 0.0f)) * kFeetPerNauticalMile;
    const float altitudeFt =
        Finite(ownship.altitudeFt, 0.0f) + rangeFt * std::sin(Finite(elevationDeg, 0.0f) * kDegToRad);
    return altitudeFt / 1000.0f;
}

bool ComputeInterceptSteeringAngleDeg(const RadarTrack& track,
                                      const OwnshipState& ownship,
                                      float& steeringAngleDeg) noexcept
{
    const float rangeNm = std::max(0.0f, Finite(track.rangeNm, 0.0f));
    const float azimuthRad = Finite(track.azimuthDeg, 0.0f) * kDegToRad;
    const float relativeHeadingRad =
        WrapDegrees(Finite(track.headingDeg, 0.0f) - Finite(ownship.headingDeg, 0.0f)) * kDegToRad;
    const float ownshipSpeedNmPerSecond =
        std::max(1.0f, Finite(ownship.speedKts, 1.0f)) / 3600.0f;
    const float targetSpeedNmPerSecond =
        std::max(0.0f, Finite(track.speedKts, 0.0f)) / 3600.0f;

    const float rangeX = rangeNm * std::sin(azimuthRad);
    const float rangeY = rangeNm * std::cos(azimuthRad);
    const float targetVelocityX = targetSpeedNmPerSecond * std::sin(relativeHeadingRad);
    const float targetVelocityY = targetSpeedNmPerSecond * std::cos(relativeHeadingRad);
    const float quadraticA = targetSpeedNmPerSecond * targetSpeedNmPerSecond -
        ownshipSpeedNmPerSecond * ownshipSpeedNmPerSecond;
    const float quadraticB = 2.0f * (rangeX * targetVelocityX + rangeY * targetVelocityY);
    const float quadraticC = rangeNm * rangeNm;

    float interceptSeconds = -1.0f;
    if (std::fabs(quadraticA) < 1.0e-8f)
    {
        if (std::fabs(quadraticB) > 1.0e-8f)
        {
            interceptSeconds = -quadraticC / quadraticB;
        }
    }
    else
    {
        const float discriminant = quadraticB * quadraticB - 4.0f * quadraticA * quadraticC;
        if (discriminant >= 0.0f && std::isfinite(discriminant))
        {
            const float root = std::sqrt(discriminant);
            const float firstSeconds = (-quadraticB - root) / (2.0f * quadraticA);
            const float secondSeconds = (-quadraticB + root) / (2.0f * quadraticA);
            if (firstSeconds > 0.0f && secondSeconds > 0.0f)
            {
                interceptSeconds = std::min(firstSeconds, secondSeconds);
            }
            else
            {
                interceptSeconds = std::max(firstSeconds, secondSeconds);
            }
        }
    }

    if (!(interceptSeconds > 0.0f) || !std::isfinite(interceptSeconds))
    {
        return false;
    }

    const float interceptX = rangeX + targetVelocityX * interceptSeconds;
    const float interceptY = rangeY + targetVelocityY * interceptSeconds;
    steeringAngleDeg = std::atan2(interceptX, interceptY) * kRadToDeg;
    return std::isfinite(steeringAngleDeg);
}

// Maps a relative bearing/range onto the heading-up HSD compass rose.
NavSymbolView ProjectPolarSymbol(const float bearingDeg,
                                 const float rangeNm,
                                 const float headingDeg,
                                 const float rangeScaleNm) noexcept
{
    NavSymbolView view;
    const float fraction = RangeFraction(rangeNm, rangeScaleNm);
    if (fraction < 0.0f || fraction > 1.0f)
    {
        return view;
    }

    const float relativeDeg = WrapDegrees(Finite(bearingDeg, 0.0f) - Finite(headingDeg, 0.0f));
    const float radius = fraction * kCompassRadius;
    const float angle = relativeDeg * kDegToRad;
    view.position = MfdVec2 {radius * std::sin(angle), radius * std::cos(angle)};
    view.visible = true;
    return view;
}

float Distance(const MfdVec2 start, const MfdVec2 end) noexcept
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    return std::sqrt(dx * dx + dy * dy);
}

float AngleDegrees(const MfdVec2 start, const MfdVec2 end) noexcept
{
    return std::atan2(end.y - start.y, end.x - start.x) * kRadToDeg;
}

FlightPlanLegView BuildFlightPlanLeg(const NavSymbolView& start, const NavSymbolView& end) noexcept
{
    FlightPlanLegView leg;
    if (!start.visible || !end.visible)
    {
        return leg;
    }

    leg.position = MfdVec2 {
        (start.position.x + end.position.x) * 0.5f,
        (start.position.y + end.position.y) * 0.5f};
    leg.rotationDegrees = AngleDegrees(start.position, end.position);
    leg.length = Distance(start.position, end.position);
    leg.visible = leg.length > 0.001f;
    return leg;
}
} // namespace

RadarTrackView ProjectRadarTrack(const RadarTrack& track,
                                 const RadarSettings& radar,
                                 const OwnshipState& ownship) noexcept
{
    RadarTrackView view;
    if (!track.active)
    {
        return view;
    }

    const RadarDisplayPoint displayPoint = RadarTrackDisplayPoint(track, radar);
    const float fraction = RangeFraction(displayPoint.rangeNm, radar.rangeScaleNm);

    view.position = MfdVec2 {
        Clamp(displayPoint.azimuthDeg / kFcrDisplayHalfAngleDeg, -1.0f, 1.0f) * kScopeHalfWidth,
        kScopeBottom + Clamp(fraction, 0.0f, 1.0f) * kScopeHeight};
    view.rotationDegrees = WrapDegrees(track.aspectDeg);
    view.altitudeThousandsFt = DerivedAltitudeThousandsFt(track, ownship);
    view.hostile = track.hostile;
    view.extrapolated = track.quality == RadarTrackQuality::Extrapolated;
    view.state = track.state;
    view.visible = true;
    return view;
}

MfdVec2 StationSelectPosition(const int station) noexcept
{
    // Centers matching the authored pylon anchors in sms_stations.json.
    static constexpr MfdVec2 kStations[kStationCount] = {
        {-0.48f, -0.13f},
        {-0.33f, -0.02f},
        {-0.19f, 0.06f},
        {-0.07f, 0.12f},
        {0.00f, -0.20f},
        {0.07f, 0.12f},
        {0.19f, 0.06f},
        {0.33f, -0.02f},
        {0.48f, -0.13f}};

    if (station < 1 || station > static_cast<int>(kStationCount))
    {
        return MfdVec2 {0.0f, 0.0f};
    }
    return kStations[static_cast<std::size_t>(station - 1)];
}

MfdFrame BuildMfdFrame(const MfdInputSample& input) noexcept
{
    MfdFrame frame;

    // FCR (Radar) page.
    RadarFrame& radar = frame.radar;
    radar.radarPresentationVisible = input.radar.operatingState == RadarOperatingState::Operating;
    for (std::size_t index = 0; index < kMaxRadarTracks; ++index)
    {
        radar.tracks[index] = ProjectRadarTrack(input.tracks[index], input.radar, input.ownship);
    }

    const bool singleTargetTrack = input.radar.submode == RadarSubmode::Stt;
    radar.scanLineVisible = radar.radarPresentationVisible && !singleTargetTrack;
    const float halfScanDeg = RadarScanHalfAngleDeg(input.radar);
    radar.scanLineX =
        Clamp(Finite(input.radar.antennaAzimuthDeg, 0.0f) / kFcrDisplayHalfAngleDeg, -1.0f, 1.0f) *
        kScopeHalfWidth;
    radar.azimuthLimitsVisible = radar.radarPresentationVisible && !singleTargetTrack &&
        halfScanDeg < kFcrDisplayHalfAngleDeg &&
        !(input.radar.submode == RadarSubmode::Tws &&
          input.radar.fieldOfView == RadarFieldOfView::Expanded);
    radar.azimuthLimitsCenterX =
        Clamp(Finite(input.radar.scanCenterAzimuthDeg, 0.0f) / kFcrDisplayHalfAngleDeg, -1.0f, 1.0f) *
        kScopeHalfWidth;
    radar.azimuthLimitsHalfWidth =
        Clamp(halfScanDeg / kFcrDisplayHalfAngleDeg, 0.0f, 1.0f) * kScopeHalfWidth;
    radar.elevationCaretY =
        Clamp(Finite(input.radar.antennaElevationDeg, 0.0f) / kMaxElevationDeg, -1.0f, 1.0f) * kScopeTop;
    radar.cursorPosition = MfdVec2 {
        Clamp(Finite(input.radar.cursorPosition.x, 0.0f), -1.0f, 1.0f) * kScopeHalfWidth,
        Clamp(Finite(input.radar.cursorPosition.y, 0.0f), -1.0f, 1.0f) * kScopeTop};
    radar.expandedReferenceVisible = radar.radarPresentationVisible &&
        input.radar.fieldOfView == RadarFieldOfView::Expanded && !singleTargetTrack;
    radar.expandedReferencePosition = radar.cursorPosition;
    const float cursorRangeFraction =
        (Clamp(Finite(input.radar.cursorPosition.y, 0.0f), -1.0f, 1.0f) + 1.0f) * 0.5f;
    const float cursorRangeNm = cursorRangeFraction * std::max(1.0f, Finite(input.radar.rangeScaleNm, 40.0f));
    const float elevationCenterDeg =
        Clamp(Finite(input.radar.antennaElevationDeg, 0.0f), -kMaxElevationDeg, kMaxElevationDeg);
    const float elevationHalfCoverageDeg = RadarVerticalHalfCoverageDeg(input.radar);
    radar.cursorMaximumAltitudeThousandsFt = SearchAltitudeThousandsFt(
        input.ownship, cursorRangeNm, elevationCenterDeg + elevationHalfCoverageDeg);
    radar.cursorMinimumAltitudeThousandsFt = SearchAltitudeThousandsFt(
        input.ownship, cursorRangeNm, elevationCenterDeg - elevationHalfCoverageDeg);

    for (std::size_t index = 0; index < kMaxRadarTracks; ++index)
    {
        const RadarTrackView& view = radar.tracks[index];
        const bool designated = view.state == RadarTrackState::SystemTarget ||
            view.state == RadarTrackState::SingleTargetTrack;
        if (!view.visible || !designated)
        {
            continue;
        }

        radar.buggedVisible = true;
        radar.designatedTrackIndex = static_cast<int>(index);
        radar.buggedTrack = view;
        radar.buggedTrack.rotationDegrees =
            WrapDegrees(Finite(input.tracks[index].aspectDeg, 0.0f));
        radar.datablockVisible = true;

        float steeringAngleDeg = 0.0f;
        const RadarTrack& buggedTrack = input.tracks[index];
        if (view.state == RadarTrackState::SingleTargetTrack &&
            ComputeInterceptSteeringAngleDeg(buggedTrack, input.ownship, steeringAngleDeg) &&
            std::fabs(steeringAngleDeg) <= kFcrDisplayHalfAngleDeg)
        {
            radar.sttInterceptVisible = true;
            radar.sttInterceptPosition = MfdVec2 {
                Clamp(steeringAngleDeg / kFcrDisplayHalfAngleDeg, -1.0f, 1.0f) * kScopeHalfWidth,
                radar.buggedTrack.position.y};
        }
        break;
    }

    // HSD (NAV) page.
    NavFrame& nav = frame.nav;
    nav.compassRotationDegrees = WrapDegrees(-input.ownship.headingDeg);
    const std::size_t waypointCount =
        static_cast<std::size_t>(std::clamp(input.nav.waypointCount, 1, static_cast<int>(kSteerpointCount)));
    for (std::size_t index = 0; index < waypointCount; ++index)
    {
        const Steerpoint& steerpoint = input.nav.steerpoints[index];
        nav.steerpoints[index] = ProjectPolarSymbol(
            steerpoint.bearingDeg, steerpoint.rangeNm, input.ownship.headingDeg, input.nav.rangeScaleNm);
    }
    for (std::size_t index = 0; index + 1U < waypointCount; ++index)
    {
        nav.flightPlanLegs[index] = BuildFlightPlanLeg(nav.steerpoints[index], nav.steerpoints[index + 1U]);
    }
    nav.bullseye = ProjectPolarSymbol(
        input.nav.bullseyeBearingDeg, input.nav.bullseyeRangeNm, input.ownship.headingDeg, input.nav.rangeScaleNm);

    if (input.nav.declutterActive)
    {
        for (FlightPlanLegView& leg : nav.flightPlanLegs)
        {
            leg.visible = false;
        }
        nav.bullseye.visible = false;
    }

    return frame;
}
} // namespace lhld
