/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorLaunchOptions.h"

/**
 * @file
 * @brief Command-line option parser for the editor executable.
 */

#include <string_view>

namespace editor
{
namespace
{
constexpr std::string_view kHelpOption = "--help";
constexpr std::string_view kShortHelpOption = "-h";
constexpr std::string_view kAssetDirectoryOption = "--asset-directory";

bool IsAssetDirectoryOption(const std::string_view argument) noexcept
{
    return argument == kAssetDirectoryOption;
}

bool StartsWithAssetDirectoryAssignment(const std::string_view argument) noexcept
{
    return (argument.size() > kAssetDirectoryOption.size() &&
            argument.compare(0, kAssetDirectoryOption.size(), kAssetDirectoryOption) == 0 &&
            argument[kAssetDirectoryOption.size()] == '=');
}

std::string_view AssignmentValue(const std::string_view argument) noexcept
{
    const std::size_t equals = argument.find('=');
    return equals == std::string_view::npos ? std::string_view {} : argument.substr(equals + 1U);
}
} // namespace

EditorLaunchOptionsParseResult ParseEditorLaunchOptions(const int argc, char* const argv[])
{
    EditorLaunchOptionsParseResult result;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument = argv != nullptr && argv[index] != nullptr ? std::string_view {argv[index]}
                                                                                    : std::string_view {};
        if (argument == kHelpOption || argument == kShortHelpOption)
        {
            result.options.showHelp = true;
            continue;
        }

        if (IsAssetDirectoryOption(argument))
        {
            if (index + 1 >= argc || argv[index + 1] == nullptr || std::string_view {argv[index + 1]}.empty())
            {
                result.error = "missing value for --asset-directory.";
                return result;
            }

            result.options.assetDirectory = std::filesystem::path(argv[++index]).lexically_normal();
            continue;
        }

        if (StartsWithAssetDirectoryAssignment(argument))
        {
            const std::string_view value = AssignmentValue(argument);
            if (value.empty())
            {
                result.error = "missing value for --asset-directory.";
                return result;
            }

            result.options.assetDirectory = std::filesystem::path(value).lexically_normal();
            continue;
        }

        result.error = "unknown option: " + std::string(argument) + ".";
        return result;
    }

    return result;
}
} // namespace editor
