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

#include <algorithm>
#include <cmath>

namespace lhld
{
namespace
{
// B-scope geometry authored in radar.json: ownship at the bottom, azimuth
// spread horizontally, range vertically.
constexpr float kScopeHalfWidth = 0.70f;
constexpr float kScopeTop = 0.75f;
constexpr float kScopeBottom = -0.75f;
constexpr float kScopeHeight = kScopeTop - kScopeBottom;
constexpr float kMaxScanHalfAngleDeg = 60.0f;
constexpr float kMaxElevationDeg = 30.0f;
constexpr float kFeetPerNauticalMile = 6076.12f;
constexpr float kRadarBarSpacingDeg = 2.2f;
constexpr float kRadarBeamHalfHeightDeg = 1.4f;
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

float ElevationAngleDeg(const RadarContact& contact, const OwnshipState& ownship) noexcept
{
    const float rangeFt = std::max(1.0f, Finite(contact.rangeNm, 0.0f) * kFeetPerNauticalMile);
    const float altitudeDeltaFt = Finite(contact.altitudeFt, ownship.altitudeFt) - Finite(ownship.altitudeFt, 0.0f);
    return std::atan2(altitudeDeltaFt, rangeFt) * kRadToDeg;
}

float RadarVerticalHalfCoverageDeg(const RadarSettings& radar) noexcept
{
    const int bars = std::clamp(radar.scanBars, 1, 4);
    return kRadarBeamHalfHeightDeg + (static_cast<float>(bars - 1) * kRadarBarSpacingDeg * 0.5f);
}

bool IsInsideRadarElevationVolume(const RadarContact& contact,
                                  const RadarSettings& radar,
                                  const OwnshipState& ownship) noexcept
{
    const float contactElevationDeg = ElevationAngleDeg(contact, ownship);
    const float antennaCenterDeg =
        Clamp(Finite(radar.antennaElevationDeg, 0.0f), -kMaxElevationDeg, kMaxElevationDeg);
    return std::fabs(contactElevationDeg - antennaCenterDeg) <= RadarVerticalHalfCoverageDeg(radar);
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

RadarContactView ProjectRadarContact(const RadarContact& contact,
                                     const RadarSettings& radar,
                                     const OwnshipState& ownship) noexcept
{
    RadarContactView view;
    if (!contact.active)
    {
        return view;
    }

    const float halfScanDeg =
        Clamp(Finite(radar.azScanDeg, 60.0f), 10.0f, kMaxScanHalfAngleDeg * 2.0f) * 0.5f;
    const float bearingDeg = Finite(contact.azimuthNorm, 0.0f) * kMaxScanHalfAngleDeg;
    if (std::fabs(bearingDeg) > halfScanDeg)
    {
        return view;
    }

    const float fraction = RangeFraction(contact.rangeNm, radar.rangeScaleNm);
    if (fraction < 0.0f || fraction > 1.0f)
    {
        return view;
    }

    if (!IsInsideRadarElevationVolume(contact, radar, ownship))
    {
        return view;
    }

    view.position = MfdVec2 {
        Clamp(bearingDeg / halfScanDeg, -1.0f, 1.0f) * kScopeHalfWidth,
        kScopeBottom + Clamp(fraction, 0.0f, 1.0f) * kScopeHeight};
    view.rotationDegrees = WrapDegrees(contact.headingDeg - ownship.headingDeg);
    view.hostile = contact.hostile;
    view.visible = true;
    return view;
}

RadarContactView ProjectBuggedTarget(const RadarContact& contact,
                                     const RadarSettings& radar,
                                     const OwnshipState& ownship) noexcept
{
    RadarContactView view;
    if (!contact.active)
    {
        return view;
    }

    const float halfScanDeg =
        Clamp(Finite(radar.azScanDeg, 60.0f), 10.0f, kMaxScanHalfAngleDeg * 2.0f) * 0.5f;
    const float bearingDeg = Finite(contact.azimuthNorm, 0.0f) * kMaxScanHalfAngleDeg;
    const float fraction = RangeFraction(contact.rangeNm, radar.rangeScaleNm);

    // Single-target-track keeps the antenna locked on the target, so the symbol
    // stays on the scope even outside the search volume or range scale: the
    // position is clamped instead of the contact being filtered out.
    view.position = MfdVec2 {
        Clamp(bearingDeg / halfScanDeg, -1.0f, 1.0f) * kScopeHalfWidth,
        kScopeBottom + Clamp(fraction, 0.0f, 1.0f) * kScopeHeight};
    view.rotationDegrees = WrapDegrees(contact.headingDeg - ownship.headingDeg);
    view.hostile = contact.hostile;
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
    for (std::size_t index = 0; index < kMaxContacts; ++index)
    {
        radar.contacts[index] = ProjectRadarContact(input.contacts[index], input.radar, input.ownship);
    }

    const bool singleTargetTrack = input.radar.submode == RadarSubmode::Stt;
    radar.scanLineVisible = !singleTargetTrack;
    radar.scanLineX = Clamp(Finite(input.radar.scanSweepNorm, 0.0f), -1.0f, 1.0f) * kScopeHalfWidth;
    radar.elevationCaretY =
        Clamp(Finite(input.radar.antennaElevationDeg, 0.0f) / kMaxElevationDeg, -1.0f, 1.0f) * kScopeTop;
    radar.cursorPosition = MfdVec2 {
        Clamp(Finite(input.radar.cursorAz, 0.0f), -1.0f, 1.0f) * kScopeHalfWidth,
        kScopeBottom + Clamp(Finite(input.radar.cursorRange, 0.0f), 0.0f, 1.0f) * kScopeHeight};

    const int bugged = input.radar.buggedContact;
    if (bugged >= 0 && bugged < static_cast<int>(kMaxContacts))
    {
        const RadarContactView view =
            ProjectBuggedTarget(input.contacts[static_cast<std::size_t>(bugged)], input.radar, input.ownship);
        if (view.visible)
        {
            radar.buggedVisible = true;
            radar.buggedPosition = view.position;
            radar.buggedAspectRotationDegrees =
                WrapDegrees(Finite(input.contacts[static_cast<std::size_t>(bugged)].aspectDeg, 0.0f));
            radar.datablockVisible = true;
        }
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
