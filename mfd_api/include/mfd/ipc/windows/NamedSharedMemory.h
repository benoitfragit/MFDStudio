/*
 * This file is part of MFDStudio.
 */
#pragma once

/**
 * @file
 * @brief Windows named shared-memory mapping wrapper used by SHM mono-slot transport.
 */

#include <cstddef>
#include <string>

namespace mfd::ipc::windows
{
/** @brief Named shared-memory wrapper around Win32 file mappings. */
class NamedSharedMemory
{
public:
    NamedSharedMemory() = default;
    ~NamedSharedMemory();

    NamedSharedMemory(const NamedSharedMemory&) = delete;
    NamedSharedMemory& operator=(const NamedSharedMemory&) = delete;

    bool OpenOrCreate(const std::string& name, std::size_t bytes);
    void* Data() noexcept;
    const void* Data() const noexcept;
    std::size_t Size() const noexcept;
    bool IsOpen() const noexcept;
    std::string LastError() const;

private:
    void* mapping_ = nullptr;
    void* data_ = nullptr;
    std::size_t size_ = 0U;
    std::string lastError_ {};
};
} // namespace mfd::ipc::windows
