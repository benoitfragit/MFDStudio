/*
 * This file is part of MFDStudio.
 */
#pragma once

/**
 * @file
 * @brief Fixed-size POD payloads used by the named shared-memory IPC transport.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace mfd
{
/** @brief Magic number identifying MFD SHM packets. */
constexpr std::uint32_t kShmPacketMagic = 0x4D464453U; // "MFDS"
/** @brief Protocol version for SHM packet envelopes. */
constexpr std::uint16_t kShmPacketVersion = 1U;
/** @brief Maximum fixed payload size stored in one mono-slot packet. */
constexpr std::size_t kShmPayloadBytes = 65536U;

/**
 * @brief Fixed-size packet envelope exchanged in shared memory.
 */
struct ShmPacket
{
    std::uint32_t magic = kShmPacketMagic;
    std::uint16_t version = kShmPacketVersion;
    std::uint16_t reserved = 0U;
    std::uint32_t payloadSize = 0U;
    std::array<std::byte, kShmPayloadBytes> payload {};
};

/** @brief Example fixed-size radar track payload for generated business models. */
struct RadarTrack
{
    std::uint32_t id = 0U;
    float x = 0.0F;
    float y = 0.0F;
    float heading = 0.0F;
    char label[16] {};
};

/** @brief Example fixed-size radar frame payload for generated business models. */
struct RadarFrame
{
    std::uint32_t count = 0U;
    RadarTrack tracks[256] {};
};

static_assert(std::is_trivially_copyable<ShmPacket>::value, "ShmPacket must be trivially copyable");
static_assert(std::is_standard_layout<ShmPacket>::value, "ShmPacket must be standard layout");
static_assert(std::is_trivially_copyable<RadarTrack>::value, "RadarTrack must be trivially copyable");
static_assert(std::is_standard_layout<RadarTrack>::value, "RadarTrack must be standard layout");
static_assert(std::is_trivially_copyable<RadarFrame>::value, "RadarFrame must be trivially copyable");
static_assert(std::is_standard_layout<RadarFrame>::value, "RadarFrame must be standard layout");
} // namespace mfd
