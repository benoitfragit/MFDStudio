/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Dedicated cockpit-demo entry point selecting the composite cockpit window configuration.
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
/**
 * @brief Runs the generic demo application against the cockpit showcase configuration.
 * @return Process exit code returned by the demo shell.
 */
int RunDemo()
{
    mfd::ApplicationConfig config;
    config.windowFile = "assets/windows/demo_pages_cockpit.json";

    mfd::MfdApplication application(config);
    return application.Run();
}

/**
 * @brief Reports a fatal startup/runtime error on stderr and, on Windows, in a modal dialog box.
 * @param message Human-readable fatal error text.
 */
void ReportFatalError(const std::string& message)
{
    std::cerr << "Fatal error: " << message << '\n';

#if defined(_WIN32)
    ::MessageBoxA(nullptr, message.c_str(), "MFD Cockpit Demo - Fatal Error", MB_ICONERROR | MB_OK);
#endif
}
} // namespace

/**
 * @brief Launches the cockpit showcase and surfaces any fatal exception to the user.
 * @return Process exit code.
 */
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
