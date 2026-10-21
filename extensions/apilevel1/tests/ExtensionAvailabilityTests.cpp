/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Runtime-session coverage with no LEVEL 1 extension DLL staged.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "mfd/runtime_api/RuntimeSession.h"

namespace
{
std::filesystem::path RepositoryRoot()
{
    const std::filesystem::path testFile = std::filesystem::path(__FILE__).lexically_normal();
    return testFile.parent_path().parent_path().parent_path().parent_path();
}
} // namespace

TEST(ExtensionAvailabilityTests, LevelZeroLoadsWithoutLevelOneRuntimeDll)
{
    ASSERT_FALSE(std::filesystem::exists(
        std::filesystem::current_path() / "mfd_containers_api1.dll"));

    mfd::runtime_api::RuntimeSession session;
    std::string error;

    EXPECT_TRUE(session.LoadWindowFile(
        RepositoryRoot() / "examples/demo/assets/windows/demo_window.json", error)) << error;
}

TEST(ExtensionAvailabilityTests, LevelOneFailsClearlyWithoutRuntimeDll)
{
    ASSERT_FALSE(std::filesystem::exists(
        std::filesystem::current_path() / "mfd_containers_api1.dll"));

    mfd::runtime_api::RuntimeSession session;
    std::string error;

    EXPECT_FALSE(session.LoadWindowFile(
        RepositoryRoot() / "examples/apilevel1/assets/windows/containers_window.json", error)) << error;
    EXPECT_NE(error.find("API LEVEL 1 requires a compatible mfd_containers_api1.dll"),
              std::string::npos) << error;
}
