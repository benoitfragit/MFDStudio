/*
 * This file is part of MFDStudio.
 */
#pragma once

/**
 * @file
 * @brief Mono-slot SHM endpoint synchronized through named events.
 */

#include <cstdint>
#include <string>

#include "mfd/ipc/ShmPacket.h"
#include "mfd/ipc/windows/NamedEvent.h"
#include "mfd/ipc/windows/NamedSharedMemory.h"

namespace mfd::ipc::windows
{
/** @brief Producer or consumer role for one SHM mono-slot endpoint. */
enum class MonoSlotRole
{
    Writer,
    Reader
};

/** @brief Configuration for one SHM mono-slot endpoint. */
struct NamedShmMonoSlotConfig
{
    std::string sharedMemoryName;
    std::string canWriteEventName;
    std::string hasDataEventName;
    std::uint32_t timeoutMs = 5U;
};

/** @brief Fixed-size SHM mono-slot endpoint. */
class NamedShmMonoSlot
{
public:
    NamedShmMonoSlot() = default;

    bool Open(const NamedShmMonoSlotConfig& config, MonoSlotRole role);
    bool Write(const ShmPacket& packet);
    bool Read(ShmPacket& packet);
    bool IsReady() const noexcept;
    std::string LastError() const;

private:
    NamedSharedMemory shm_ {};
    NamedEvent canWrite_ {};
    NamedEvent hasData_ {};
    NamedShmMonoSlotConfig config_ {};
    MonoSlotRole role_ = MonoSlotRole::Writer;
    bool ready_ = false;
    std::string lastError_ {};
};
} // namespace mfd::ipc::windows
