#include "StreamProtocol.h"
#include <limits>

namespace mfd::stream::detail
{
bool ValidHeader(const WireHeader& header) noexcept
{
    if (header.magic != kProtocolMagic || header.version != kProtocolVersion ||
        header.bytes != sizeof(WireHeader) || header.session == 0 || header.epoch == 0 ||
        header.result > static_cast<std::uint32_t>(StreamResult::Error)) return false;
    for (const WireSlot& slot : header.slots)
        if (slot.state > static_cast<std::uint32_t>(SlotState::Reading)) return false;
    return true;
}
std::uint32_t FindWritable(const WireHeader& header) noexcept
{
    std::uint32_t oldest = kNoSlot;
    std::uint64_t frame = std::numeric_limits<std::uint64_t>::max();
    for (std::uint32_t index = 0; index < kSlotCount; ++index)
    {
        const WireSlot& slot = header.slots[index];
        if (slot.state == static_cast<std::uint32_t>(SlotState::Free)) return index;
        if (slot.state == static_cast<std::uint32_t>(SlotState::Ready) && slot.frame < frame)
        { oldest = index; frame = slot.frame; }
    }
    return oldest;
}
std::uint32_t FindLatest(const WireHeader& header) noexcept
{
    std::uint32_t latest = kNoSlot;
    std::uint64_t frame = 0;
    for (std::uint32_t index = 0; index < kSlotCount; ++index)
    {
        const WireSlot& slot = header.slots[index];
        if (slot.state == static_cast<std::uint32_t>(SlotState::Ready) &&
            slot.epoch == header.epoch && slot.frame > frame)
        { latest = index; frame = slot.frame; }
    }
    return latest;
}
void InvalidateReady(WireHeader& header) noexcept
{
    for (WireSlot& slot : header.slots)
        if (slot.state == static_cast<std::uint32_t>(SlotState::Ready))
            slot.state = static_cast<std::uint32_t>(SlotState::Free);
}
} // namespace mfd::stream::detail
