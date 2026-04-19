/*
 * This file is part of MFDStudio.
 */
#pragma once

/**
 * @file
 * @brief Windows named event wrapper used by SHM mono-slot synchronization.
 */

#include <cstdint>
#include <string>

namespace mfd::ipc::windows
{
/** @brief Named event wrapper around Win32 CreateEvent/OpenEvent calls. */
class NamedEvent
{
public:
    NamedEvent() = default;
    ~NamedEvent();

    NamedEvent(const NamedEvent&) = delete;
    NamedEvent& operator=(const NamedEvent&) = delete;

    bool OpenOrCreate(const std::string& name, bool manualReset, bool initialState);
    bool Set();
    bool Reset();
    bool Wait(std::uint32_t timeoutMs);
    bool IsOpen() const noexcept;
    std::string LastError() const;

private:
    void* handle_ = nullptr;
    std::string lastError_ {};
};
} // namespace mfd::ipc::windows
