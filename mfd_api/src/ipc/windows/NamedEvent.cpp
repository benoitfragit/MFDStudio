#include "mfd/ipc/windows/NamedEvent.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace mfd::ipc::windows
{
NamedEvent::~NamedEvent()
{
#ifdef _WIN32
    if (handle_ != nullptr)
    {
        ::CloseHandle(static_cast<HANDLE>(handle_));
    }
#endif
}

bool NamedEvent::OpenOrCreate(const std::string& name, const bool manualReset, const bool initialState)
{
#ifdef _WIN32
    if (handle_ != nullptr)
    {
        ::CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = nullptr;
    }

    HANDLE eventHandle = ::CreateEventA(nullptr,
                                        manualReset ? TRUE : FALSE,
                                        initialState ? TRUE : FALSE,
                                        name.c_str());
    if (eventHandle == nullptr)
    {
        lastError_ = "CreateEventA failed";
        return false;
    }

    handle_ = eventHandle;
    lastError_.clear();
    return true;
#else
    (void)name;
    (void)manualReset;
    (void)initialState;
    lastError_ = "Named events are only supported on Windows";
    return false;
#endif
}

bool NamedEvent::Set()
{
#ifdef _WIN32
    if (handle_ == nullptr || ::SetEvent(static_cast<HANDLE>(handle_)) == 0)
    {
        lastError_ = "SetEvent failed";
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool NamedEvent::Reset()
{
#ifdef _WIN32
    if (handle_ == nullptr || ::ResetEvent(static_cast<HANDLE>(handle_)) == 0)
    {
        lastError_ = "ResetEvent failed";
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool NamedEvent::Wait(const std::uint32_t timeoutMs)
{
#ifdef _WIN32
    if (handle_ == nullptr)
    {
        lastError_ = "Event handle is null";
        return false;
    }

    const DWORD waitResult = ::WaitForSingleObject(static_cast<HANDLE>(handle_), static_cast<DWORD>(timeoutMs));
    if (waitResult == WAIT_OBJECT_0)
    {
        return true;
    }

    if (waitResult == WAIT_TIMEOUT)
    {
        lastError_ = "WaitForSingleObject timeout";
    }
    else
    {
        lastError_ = "WaitForSingleObject failed";
    }

    return false;
#else
    (void)timeoutMs;
    return false;
#endif
}

bool NamedEvent::IsOpen() const noexcept
{
    return handle_ != nullptr;
}

std::string NamedEvent::LastError() const
{
    return lastError_;
}
} // namespace mfd::ipc::windows
