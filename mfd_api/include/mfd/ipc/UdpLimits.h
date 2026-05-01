/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Shared UDP payload size limits used by public transports.
 */

#include <cstddef>

namespace mfd
{
/** @brief Smallest supported UDP payload size in bytes. */
inline constexpr std::size_t kUdpMinPayloadBytes = 64U;
/** @brief Default UDP payload size in bytes. */
inline constexpr std::size_t kUdpDefaultPayloadBytes = 4096U;
/** @brief Largest UDP payload size in bytes for one datagram payload. */
inline constexpr std::size_t kUdpMaxPayloadBytes = 65507U;
} // namespace mfd
