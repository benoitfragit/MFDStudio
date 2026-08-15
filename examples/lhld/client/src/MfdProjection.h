/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Stateless projection from semantic MFD state to page symbology geometry.
 */

#include "MfdTypes.h"

namespace lhld
{
/**
 * @addtogroup lhld_integration
 * @{
 */

/**
 * @brief Builds one projected MFD frame from a semantic input sample.
 * @param input Complete semantic MFD state for one frame.
 * @return Projected FCR and HSD geometry consumed by @ref MfdController.
 * @note Stateless. Non-finite inputs are defensively clamped to safe values.
 */
MfdFrame BuildMfdFrame(const MfdInputSample& input) noexcept;

/**
 * @brief Projects one radar track into B-scope MFD coordinates.
 * @param track Sensor-native radar track.
 * @param radar Operator radar controls (azimuth, bars, elevation and range).
 * @param ownship Ownship state used for relative motion and derived altitude.
 * @return Projected track view; @ref RadarTrackView::visible is false when the
 * track falls outside the current scan volume or range scale.
 */
RadarTrackView ProjectRadarTrack(const RadarTrack& track,
                                 const RadarSettings& radar,
                                 const OwnshipState& ownship) noexcept;

/**
 * @brief Projects a bugged or single-target-track target.
 * @param track Sensor-native radar track currently designated.
 * @param radar Operator radar controls (scan width and range scale).
 * @param ownship Ownship state (for relative motion rotation).
 * @return Projected view; visible whenever the track is active, with the
 * position clamped into the scope. Unlike @ref ProjectRadarTrack, a lock
 * outside the RWS/TWS search volume or beyond the range scale is kept on the
 * display rather than filtered out, because the antenna stays on the target.
 */
RadarTrackView ProjectBuggedTrack(const RadarTrack& track,
                                  const RadarSettings& radar,
                                  const OwnshipState& ownship) noexcept;

/**
 * @brief Returns the fixed SMS select-box center for one 1-based station.
 * @param station Station index in [1, kStationCount]; other values return the
 * page center.
 * @return Select-box position in MFD space matching the authored stores diagram.
 */
MfdVec2 StationSelectPosition(int station) noexcept;

/** @} */
} // namespace lhld
