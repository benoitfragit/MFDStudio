/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Documented FCR control rotaries and mode-dependent scan patterns.
 */

#include "MfdTypes.h"

namespace lhld
{
/** @brief Direction applied to the FCR range rotary. */
enum class RadarRangeStep
{
    /** @brief Select the next lower range without wrapping. */
    Decrease,
    /** @brief Select the next higher range without wrapping. */
    Increase
};

/** @brief Effective antenna search volume resolved by the radar source. */
struct RadarScanVolume
{
    /** @brief Total left-to-right scan width in degrees. */
    float azimuthWidthDeg = 120.0f;
    /** @brief Number of elevation bars. */
    int bars = 4;
};

/**
 * @brief Returns the next azimuth selection in the documented wide-to-narrow rotary.
 * @param current Current pilot selection.
 * @return Next selection, wrapping from narrow to wide.
 */
RadarAzimuthScan NextRadarAzimuthScan(RadarAzimuthScan current) noexcept;

/**
 * @brief Returns the next supported RWS bar count.
 * @param current Current requested bar count.
 * @return 1, 2 or 4 bars, wrapping after 4.
 */
int NextRadarBarCount(int current) noexcept;

/**
 * @brief Resolves pilot intent into the mode-supported effective scan volume.
 * @param submode Current radar submode.
 * @param azimuthScan Pilot-selected azimuth family.
 * @param requestedBars Pilot-selected RWS bar count.
 * @return Effective azimuth width and bars published by the radar.
 * @note TWS is constrained to A6/2B, A2/3B and A1/4B.
 */
RadarScanVolume ResolveRadarScanVolume(RadarSubmode submode,
                                       RadarAzimuthScan azimuthScan,
                                       int requestedBars) noexcept;

/**
 * @brief Normalizes a range value to the nearest documented FCR range detent.
 * @param rangeScaleNm Requested range scale in nautical miles.
 * @return One of 5, 10, 20, 40, 80 or 160 NM.
 */
float NormalizeRadarRangeScale(float rangeScaleNm) noexcept;

/**
 * @brief Steps the documented FCR range rotary without endpoint wrapping.
 * @param currentScaleNm Current range scale in nautical miles.
 * @param direction Requested increment or decrement.
 * @return The selected documented range detent.
 */
float StepRadarRangeScale(float currentScaleNm, RadarRangeStep direction) noexcept;

/**
 * @brief Returns the FCR azimuth-width digit for an effective scan volume.
 * @param azimuthWidthDeg Total scan width in degrees.
 * @return Static mnemonic string: 6, 3, 2 or 1.
 */
const char* RadarAzimuthMnemonic(float azimuthWidthDeg) noexcept;

/**
 * @brief Returns the documented IFF mnemonic.
 * @param mode Current IFF interrogation mode.
 * @return Static MFD mnemonic string.
 */
const char* RadarIffMnemonic(RadarIffMode mode) noexcept;
} // namespace lhld
