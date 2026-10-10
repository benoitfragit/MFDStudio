#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "mfd/model/ExternalStreamTypes.h"

namespace mfd::stream::detail
{
constexpr std::uint32_t kProtocolMagic = 0x4D464453;
constexpr std::uint32_t kProtocolVersion = 1;
constexpr std::size_t kSlotCount = 3;
constexpr std::uint32_t kNoSlot = 3;
/** @brief Wire state; CPU mutex protects all fields, GPU queries protect pixel ownership. */
enum class SlotState : std::uint32_t { Free, Writing, Pending, Ready, Reading };
// Only fixed-width integers. Explicit packing and size checks enforce the same layout on x86/x64.
#pragma pack(push, 8)
struct WireSlot
{
    std::uint64_t handle = 0;
    std::uint64_t frame = 0;
    std::uint64_t epoch = 0;
    std::uint32_t state = 0;
    std::uint32_t reserved = 0;
};
struct WireHeader
{
    std::uint32_t magic = kProtocolMagic;
    std::uint32_t version = kProtocolVersion;
    std::uint32_t bytes = 0;
    std::uint32_t hostPid = 0;
    std::uint64_t session = 0;
    std::uint64_t generation = 0;
    std::uint64_t epoch = 1;
    std::uint64_t hostHeartbeat = 0;
    std::uint64_t producerHeartbeat = 0;
    std::uint32_t producerPid = 0;
    std::uint32_t requestedWidth = 0;
    std::uint32_t requestedHeight = 0;
    std::uint32_t requestedFormat = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t format = 0;
    std::uint32_t adapterLow = 0;
    std::int32_t adapterHigh = 0;
    std::uint32_t result = 0;
    std::uint64_t requestSerial = 0;
    WireSlot slots[kSlotCount] {};
};
#pragma pack(pop)
static_assert(sizeof(WireSlot) == 32, "Cross-bitness slot contract");
static_assert(offsetof(WireHeader, slots) == 104, "Cross-bitness header contract");
static_assert(sizeof(WireHeader) == 200, "Cross-bitness mapping contract");
static_assert(std::is_standard_layout_v<WireHeader> && std::is_trivially_copyable_v<WireHeader>,
              "Wire metadata must be a plain byte-copyable layout");
bool ValidHeader(const WireHeader& header) noexcept;
std::uint32_t FindWritable(const WireHeader& header) noexcept;
std::uint32_t FindLatest(const WireHeader& header) noexcept;
void InvalidateReady(WireHeader& header) noexcept;
} // namespace mfd::stream::detail
