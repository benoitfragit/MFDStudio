/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Entry point of the MFD editor executable.
 */

#include <exception>
#include <iostream>
#include <string>
#include <string_view>

#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
#endif

namespace
{
void PrintUsage(const std::string_view applicationName)
{
    const std::string label = applicationName.empty() ? "mfd_editor" : std::string(applicationName);
    std::cout << "Usage:\n";
    std::cout << "  " << label << '\n';
    std::cout << "  " << label << " --help\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h\n";
    std::cout << "      Print this command-line help.\n";
}
} // namespace

int main(int argc, char** argv)
{
    const std::string applicationName =
        (argc > 0 && argv != nullptr && argv[0] != nullptr) ? std::string(argv[0]) : std::string("mfd_editor");

    if (argc > 1)
    {
        const std::string_view argument = argv[1] != nullptr ? std::string_view {argv[1]} : std::string_view {};
        if (argc == 2 && (argument == "--help" || argument == "-h"))
        {
            PrintUsage(applicationName);
            return 0;
        }

        std::cerr << "mfd_editor command-line error: unknown option.\n";
        PrintUsage(applicationName);
        return 1;
    }

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
    catch (const std::exception& exception)
    {
        std::cerr << "mfd_editor fatal error: " << exception.what() << '\n';
        return 1;
    }
}
