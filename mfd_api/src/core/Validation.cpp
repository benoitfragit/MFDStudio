/*
 * This file is part of MFDStudio.
 */
/**
 * @file
 * @brief Implementation for Validation helpers.
 */

#include "mfd/core/Validation.h"

namespace mfd
{
bool ValidateShmPacket(const ShmPacket& packet, std::string* error)
{
    if (packet.magic != kShmPacketMagic)
    {
        if (error != nullptr)
        {
            *error = "Invalid SHM packet magic";
        }
        return false;
    }

    if (packet.version != kShmPacketVersion)
    {
        if (error != nullptr)
        {
            *error = "Unsupported SHM packet version";
        }
        return false;
    }

    if (packet.payloadSize > kShmPayloadBytes)
    {
        if (error != nullptr)
        {
            *error = "SHM payload size exceeds fixed slot capacity";
        }
        return false;
    }

    return true;
}

bool ValidateRadarFrame(const RadarFrame& frame, std::string* error)
{
    if (frame.count > 256U)
    {
        if (error != nullptr)
        {
            *error = "RadarFrame.count exceeds fixed track capacity";
        }
        return false;
    }

    return true;
}
} // namespace mfd
