/*
 * This file is part of MFDStudio.
 */
#pragma once

/**
 * @file
 * @brief Validation helpers for fixed-size SHM payloads and packets.
 */

#include <cstddef>
#include <string>
#include "mfd/MfdExport.h"
#include "mfd/ipc/ShmPacket.h"

namespace mfd
{
/**
 * @brief Validates an SHM packet envelope.
 * @param packet Packet to validate.
 * @param error Optional output string receiving validation failure detail.
 * @return `true` when the packet is valid.
 */
MFD_API bool ValidateShmPacket(const ShmPacket& packet, std::string* error = nullptr);

/**
 * @brief Validates a radar frame payload loaded from SHM.
 * @param frame Frame to validate.
 * @param error Optional output string receiving validation failure detail.
 * @return `true` when the payload is valid.
 */
MFD_API bool ValidateRadarFrame(const RadarFrame& frame, std::string* error = nullptr);
} // namespace mfd
