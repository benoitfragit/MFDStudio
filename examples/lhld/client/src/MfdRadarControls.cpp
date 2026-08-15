/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of documented FCR control conventions.
 */

#include "MfdRadarControls.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace lhld
{
namespace
{
constexpr std::array<float, 6> kRadarRangeScalesNm {{5.0f, 10.0f, 20.0f, 40.0f, 80.0f, 160.0f}};

std::size_t NearestRangeIndex(const float rangeScaleNm) noexcept
{
    if (!std::isfinite(rangeScaleNm))
    {
        return 3U;
    }

    std::size_t nearestIndex = 0U;
    float nearestDistance = std::fabs(rangeScaleNm - kRadarRangeScalesNm[0]);
    for (std::size_t index = 1U; index < kRadarRangeScalesNm.size(); ++index)
    {
        const float distance = std::fabs(rangeScaleNm - kRadarRangeScalesNm[index]);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestIndex = index;
        }
    }
    return nearestIndex;
}

int NormalizeRwsBars(const int bars) noexcept
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

RadarAzimuthScan NextRadarAzimuthScan(const RadarAzimuthScan current) noexcept
{
    switch (current)
    {
    case RadarAzimuthScan::Wide:
        return RadarAzimuthScan::Medium;
    case RadarAzimuthScan::Medium:
        return RadarAzimuthScan::Narrow;
    case RadarAzimuthScan::Narrow:
        return RadarAzimuthScan::Wide;
    }
    return RadarAzimuthScan::Wide;
}

int NextRadarBarCount(const int current) noexcept
{
    const int bars = NormalizeRwsBars(current);
    if (bars == 1)
    {
        return 2;
    }
    if (bars == 2)
    {
        return 4;
    }
    return 1;
}

RadarScanVolume ResolveRadarScanVolume(const RadarSubmode submode,
                                       const RadarAzimuthScan azimuthScan,
                                       const int requestedBars) noexcept
{
    if (submode == RadarSubmode::Tws)
    {
        switch (azimuthScan)
        {
        case RadarAzimuthScan::Wide:
            return RadarScanVolume {120.0f, 2};
        case RadarAzimuthScan::Medium:
            return RadarScanVolume {50.0f, 3};
        case RadarAzimuthScan::Narrow:
            return RadarScanVolume {20.0f, 4};
        }
    }

    float widthDeg = 120.0f;
    if (azimuthScan == RadarAzimuthScan::Medium)
    {
        widthDeg = 60.0f;
    }
    else if (azimuthScan == RadarAzimuthScan::Narrow)
    {
        widthDeg = 20.0f;
    }
    return RadarScanVolume {widthDeg, NormalizeRwsBars(requestedBars)};
}

float NormalizeRadarRangeScale(const float rangeScaleNm) noexcept
{
    return kRadarRangeScalesNm[NearestRangeIndex(rangeScaleNm)];
}

float StepRadarRangeScale(const float currentScaleNm, const RadarRangeStep direction) noexcept
{
    std::size_t index = NearestRangeIndex(currentScaleNm);
    if (direction == RadarRangeStep::Decrease && index > 0U)
    {
        --index;
    }
    else if (direction == RadarRangeStep::Increase && index + 1U < kRadarRangeScalesNm.size())
    {
        ++index;
    }
    return kRadarRangeScalesNm[index];
}

const char* RadarAzimuthMnemonic(const float azimuthWidthDeg) noexcept
{
    if (!std::isfinite(azimuthWidthDeg) || azimuthWidthDeg >= 90.0f)
    {
        return "6";
    }
    if (azimuthWidthDeg >= 55.0f)
    {
        return "3";
    }
    if (azimuthWidthDeg >= 35.0f)
    {
        return "2";
    }
    return "1";
}

const char* RadarIffMnemonic(const RadarIffMode mode) noexcept
{
    switch (mode)
    {
    case RadarIffMode::Combined:
        return "M+";
    case RadarIffMode::Mode1:
        return "M1";
    case RadarIffMode::Mode2:
        return "M2";
    case RadarIffMode::Mode3:
        return "M3";
    case RadarIffMode::Mode4:
        return "M4";
    case RadarIffMode::Off:
        return "OFF";
    }
    return "M+";
}
} // namespace lhld
