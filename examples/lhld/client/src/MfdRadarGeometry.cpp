/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of shared semantic LHLD radar geometry.
 */

#include "MfdRadarGeometry.h"

#include <algorithm>
#include <cmath>

namespace lhld
{
namespace
{
constexpr float kDefaultRangeScaleNm = 40.0f;
constexpr float kDefaultAzimuthScanDeg = 60.0f;
constexpr float kMinimumAzimuthScanDeg = 10.0f;
constexpr float kMaximumAzimuthScanDeg = 120.0f;
constexpr float kRadarBarSpacingDeg = 2.2f;
constexpr float kRadarBeamHalfHeightDeg = 1.4f;
constexpr float kExpandedDisplayScale = 4.0f;

float Finite(const float value, const float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float RangeScaleNm(const RadarSettings& radar) noexcept
{
    return std::max(1.0f, Finite(radar.rangeScaleNm, kDefaultRangeScaleNm));
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
} // namespace

float RadarScanHalfAngleDeg(const RadarSettings& radar) noexcept
{
    return std::clamp(
               Finite(radar.azScanDeg, kDefaultAzimuthScanDeg),
               kMinimumAzimuthScanDeg,
               kMaximumAzimuthScanDeg) *
        0.5f;
}

float RadarVerticalHalfCoverageDeg(const RadarSettings& radar) noexcept
{
    const int bars = ValidRadarBars(radar.scanBars);
    return kRadarBeamHalfHeightDeg +
        static_cast<float>(bars - 1) * kRadarBarSpacingDeg * 0.5f;
}

RadarDisplayPoint RadarCursorSensorPoint(const RadarSettings& radar) noexcept
{
    const float normalizedX = std::clamp(Finite(radar.cursorPosition.x, 0.0f), -1.0f, 1.0f);
    const float normalizedY = std::clamp(Finite(radar.cursorPosition.y, 0.0f), -1.0f, 1.0f);
    return RadarDisplayPoint {
        normalizedX * RadarScanHalfAngleDeg(radar),
        (normalizedY + 1.0f) * 0.5f * RangeScaleNm(radar)};
}

RadarDisplayPoint RadarTrackDisplayPoint(const RadarTrack& track,
                                         const RadarSettings& radar) noexcept
{
    const RadarDisplayPoint measured {
        Finite(track.azimuthDeg, 0.0f),
        Finite(track.rangeNm, 0.0f)};
    if (radar.fieldOfView != RadarFieldOfView::Expanded)
    {
        return measured;
    }

    const RadarDisplayPoint cursor = RadarCursorSensorPoint(radar);
    return RadarDisplayPoint {
        cursor.azimuthDeg + kExpandedDisplayScale * (measured.azimuthDeg - cursor.azimuthDeg),
        cursor.rangeNm + kExpandedDisplayScale * (measured.rangeNm - cursor.rangeNm)};
}

} // namespace lhld
