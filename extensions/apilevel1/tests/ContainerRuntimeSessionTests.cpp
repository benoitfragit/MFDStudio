/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/** @file @brief End-to-end runtime-session loading coverage for a LEVEL 1 Window. */

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

TEST(ContainerRuntimeSessionTests, LoadsMixedLevelWindowWithCompatibleExtension)
{
    ASSERT_TRUE(std::filesystem::exists(
        std::filesystem::current_path() / "mfd_containers_api1.dll"));

    mfd::runtime_api::RuntimeSession session;
    std::string error;
    ASSERT_TRUE(session.LoadWindowFile(
        RepositoryRoot() / "examples/apilevel1/assets/windows/containers_window.json", error)) << error;

    ASSERT_EQ(session.Pages().size(), 2U);
    EXPECT_EQ(session.ActivePageName(), "Containers");
    EXPECT_TRUE(session.SetActivePage("Legacy"));
    EXPECT_EQ(session.ActivePageName(), "Legacy");
    EXPECT_TRUE(session.SetActivePage("Containers"));
    EXPECT_EQ(session.ActivePageName(), "Containers");
}
