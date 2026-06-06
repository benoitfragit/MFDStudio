/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests validating mfd_editor command-line parsing.
 */

#include "EditorLaunchOptions.h"

#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{
editor::EditorLaunchOptionsParseResult Parse(std::initializer_list<const char*> arguments)
{
    std::vector<std::string> argumentStorage;
    argumentStorage.reserve(arguments.size());

    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (const char* argument : arguments)
    {
        argumentStorage.emplace_back(argument);
        argv.push_back(argumentStorage.back().data());
    }

    return editor::ParseEditorLaunchOptions(static_cast<int>(argv.size()), argv.data());
}
} // namespace

TEST(EditorLaunchOptionsTests, AcceptsHelpOption)
{
    const editor::EditorLaunchOptionsParseResult result = Parse({"mfd_editor", "--help"});

    EXPECT_TRUE(result.error.empty());
    EXPECT_TRUE(result.options.showHelp);
}

TEST(EditorLaunchOptionsTests, ParsesAssetDirectorySeparatedValue)
{
    const editor::EditorLaunchOptionsParseResult result =
        Parse({"mfd_editor", "--asset-directory", "D:/MFD/assets"});

    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.options.assetDirectory, std::filesystem::path("D:/MFD/assets").lexically_normal());
}

TEST(EditorLaunchOptionsTests, ParsesAssetDirectoryAssignment)
{
    const editor::EditorLaunchOptionsParseResult result =
        Parse({"mfd_editor", "--asset-directory=D:/MFD/assets"});

    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.options.assetDirectory, std::filesystem::path("D:/MFD/assets").lexically_normal());
}

TEST(EditorLaunchOptionsTests, RejectsMissingAssetDirectoryValue)
{
    const editor::EditorLaunchOptionsParseResult result = Parse({"mfd_editor", "--asset-directory"});

    EXPECT_FALSE(result.error.empty());
}

TEST(EditorLaunchOptionsTests, RejectsUnknownOption)
{
    const editor::EditorLaunchOptionsParseResult result = Parse({"mfd_editor", "--unknown"});

    EXPECT_FALSE(result.error.empty());
}
