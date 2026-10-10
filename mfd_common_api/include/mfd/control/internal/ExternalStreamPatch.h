#pragma once
#include "mfd/control/CommandTypes.h"

namespace mfd::detail
{
/** @brief Combines stream controls chronologically without losing a Stop barrier.
 * @param destination Receives the combined fields; may alias either input.
 * @param older Previously queued control fields.
 * @param newer Latest control fields, which override older playback.
 */
inline void MergeExternalStreamControl(PrimitivePatch& destination,
                                       const PrimitivePatch& older, const PrimitivePatch& newer)
{
    auto stopToken = older.streamStopToken;
    if (older.streamPlayback == StreamPlayback::Stop && !stopToken) stopToken = 0;
    if (newer.streamStopToken) stopToken = newer.streamStopToken;
    if (newer.streamPlayback == StreamPlayback::Stop && !newer.streamStopToken) stopToken = 0;
    const auto playback = newer.streamPlayback ? newer.streamPlayback : older.streamPlayback;
    destination.streamStopToken = stopToken;
    destination.streamPlayback = playback;
}
}
