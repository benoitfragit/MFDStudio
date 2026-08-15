/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Shared semantic radar geometry for the LHLD source and projection.
 */

#include "MfdTypes.h"

namespace lhld
{
/** @brief One radar measurement after applying the selected display FOV. */
struct RadarDisplayPoint
{
    /** @brief Display-relative azimuth in degrees. */
    float azimuthDeg = 0.0f;
    /** @brief Display-relative range in nautical miles. */
    float rangeNm = 0.0f;
};

/**
 * @brief Returns the sanitized half-width of the selected azimuth scan.
 * @param radar Semantic radar controls.
 * @return Half scan angle in degrees.
 */
float RadarScanHalfAngleDeg(const RadarSettings& radar) noexcept;

/**
 * @brief Returns the vertical half-coverage of the selected bar scan.
 * @param radar Semantic radar controls.
 * @return Half coverage in degrees around antenna elevation.
 */
float RadarVerticalHalfCoverageDeg(const RadarSettings& radar) noexcept;

/**
 * @brief Converts the normalized ACQ cursor into sensor-native coordinates.
 * @param radar Semantic radar controls and normalized cursor.
 * @return Cursor azimuth on the fixed -60/+60 FCR video and range in NM.
 */
RadarDisplayPoint RadarCursorSensorPoint(const RadarSettings& radar) noexcept;

/**
 * @brief Applies the documented 4:1 EXP transform about the ACQ cursor.
 * @param track Sensor-native radar track.
 * @param radar Current radar field of view and cursor.
 * @return Track azimuth/range used only for display projection.
 * @note The source measurement is never modified. In NORM the values are
 * returned unchanged.
 */
RadarDisplayPoint RadarTrackDisplayPoint(const RadarTrack& track,
                                         const RadarSettings& radar) noexcept;
} // namespace lhld
