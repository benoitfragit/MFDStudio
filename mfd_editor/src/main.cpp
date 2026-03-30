/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

#include <exception>

#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
#endif

int main()
{
#if defined(_WIN32)
    if (::IsDebuggerPresent() != 0)
    {
        EditorApplication application;
        return application.Run();
    }
#endif

    try
    {
        EditorApplication application;
        return application.Run();
    }
    catch (const std::exception&)
    {
        return 1;
    }
}
