/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "MfdApplication.h"

#include <exception>
#include <iostream>
#include <string>

#if defined(_WIN32)
#    include <windows.h>
#endif

namespace
{
int RunDemo()
{
    mfd::ApplicationConfig config;
    config.windowFile = "assets/windows/demo_pages.json";

    mfd::MfdApplication application(config);
    return application.Run();
}

void ReportFatalError(const std::string& message)
{
    std::cerr << "Fatal error: " << message << '\n';

#if defined(_WIN32)
    ::MessageBoxA(nullptr, message.c_str(), "MFD Demo - Fatal Error", MB_ICONERROR | MB_OK);
#endif
}
} // namespace

int main()
{
#if defined(_WIN32)
    if (::IsDebuggerPresent() != FALSE)
    {
        return RunDemo();
    }
#endif

    try
    {
        return RunDemo();
    }
    catch (const std::exception& exception)
    {
        ReportFatalError(exception.what());
        return 1;
    }
}
