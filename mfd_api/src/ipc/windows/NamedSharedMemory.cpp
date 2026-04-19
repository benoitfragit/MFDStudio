#include "mfd/ipc/windows/NamedSharedMemory.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace mfd::ipc::windows
{
NamedSharedMemory::~NamedSharedMemory()
{
#ifdef _WIN32
    if (data_ != nullptr)
    {
        ::UnmapViewOfFile(data_);
    }

    if (mapping_ != nullptr)
    {
        ::CloseHandle(static_cast<HANDLE>(mapping_));
    }
#endif
}

bool NamedSharedMemory::OpenOrCreate(const std::string& name, const std::size_t bytes)
{
#ifdef _WIN32
    if (bytes == 0U)
    {
        lastError_ = "Shared memory byte size must be > 0";
        return false;
    }

    if (data_ != nullptr)
    {
        ::UnmapViewOfFile(data_);
        data_ = nullptr;
    }

    if (mapping_ != nullptr)
    {
        ::CloseHandle(static_cast<HANDLE>(mapping_));
        mapping_ = nullptr;
    }

    const DWORD bytesLow = static_cast<DWORD>(bytes & 0xFFFFFFFFULL);
    const DWORD bytesHigh = static_cast<DWORD>((bytes >> 32U) & 0xFFFFFFFFULL);
    HANDLE mapping = ::CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, bytesHigh, bytesLow, name.c_str());
    if (mapping == nullptr)
    {
        lastError_ = "CreateFileMappingA failed";
        return false;
    }

    void* view = ::MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    if (view == nullptr)
    {
        ::CloseHandle(mapping);
        lastError_ = "MapViewOfFile failed";
        return false;
    }

    mapping_ = mapping;
    data_ = view;
    size_ = bytes;
    lastError_.clear();
    return true;
#else
    (void)name;
    (void)bytes;
    lastError_ = "Named shared memory is only supported on Windows";
    return false;
#endif
}

void* NamedSharedMemory::Data() noexcept
{
    return data_;
}

const void* NamedSharedMemory::Data() const noexcept
{
    return data_;
}

std::size_t NamedSharedMemory::Size() const noexcept
{
    return size_;
}

bool NamedSharedMemory::IsOpen() const noexcept
{
    return data_ != nullptr;
}

std::string NamedSharedMemory::LastError() const
{
    return lastError_;
}
} // namespace mfd::ipc::windows
