#include "mfd/ipc/windows/NamedShmMonoSlot.h"

#include <cstring>

namespace mfd::ipc::windows
{
bool NamedShmMonoSlot::Open(const NamedShmMonoSlotConfig& config, const MonoSlotRole role)
{
    config_ = config;
    role_ = role;
    ready_ = false;

    if (!shm_.OpenOrCreate(config.sharedMemoryName, sizeof(ShmPacket)))
    {
        lastError_ = shm_.LastError();
        return false;
    }

    if (!canWrite_.OpenOrCreate(config.canWriteEventName, true, true))
    {
        lastError_ = canWrite_.LastError();
        return false;
    }

    if (!hasData_.OpenOrCreate(config.hasDataEventName, true, false))
    {
        lastError_ = hasData_.LastError();
        return false;
    }

    ready_ = true;
    lastError_.clear();
    return true;
}

bool NamedShmMonoSlot::Write(const ShmPacket& packet)
{
    if (!ready_ || role_ != MonoSlotRole::Writer)
    {
        lastError_ = "SHM mono-slot is not opened in writer mode";
        return false;
    }

    if (!canWrite_.Wait(config_.timeoutMs))
    {
        lastError_ = canWrite_.LastError();
        return false;
    }

    std::memcpy(shm_.Data(), &packet, sizeof(ShmPacket));

    if (!canWrite_.Reset())
    {
        lastError_ = canWrite_.LastError();
        return false;
    }

    if (!hasData_.Set())
    {
        lastError_ = hasData_.LastError();
        return false;
    }

    return true;
}

bool NamedShmMonoSlot::Read(ShmPacket& packet)
{
    if (!ready_ || role_ != MonoSlotRole::Reader)
    {
        lastError_ = "SHM mono-slot is not opened in reader mode";
        return false;
    }

    if (!hasData_.Wait(config_.timeoutMs))
    {
        lastError_ = hasData_.LastError();
        return false;
    }

    std::memcpy(&packet, shm_.Data(), sizeof(ShmPacket));

    if (!hasData_.Reset())
    {
        lastError_ = hasData_.LastError();
        return false;
    }

    if (!canWrite_.Set())
    {
        lastError_ = canWrite_.LastError();
        return false;
    }

    return true;
}

bool NamedShmMonoSlot::IsReady() const noexcept
{
    return ready_;
}

std::string NamedShmMonoSlot::LastError() const
{
    return lastError_;
}
} // namespace mfd::ipc::windows
