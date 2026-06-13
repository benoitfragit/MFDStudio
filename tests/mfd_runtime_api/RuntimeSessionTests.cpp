/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for the public runtime session API.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "mfd/runtime_api/RuntimeSession.h"

namespace
{
std::filesystem::path RepositoryRoot()
{
    const std::filesystem::path testFile = std::filesystem::path(__FILE__).lexically_normal();
    return testFile.parent_path().parent_path().parent_path();
}
} // namespace

TEST(RuntimeSessionTests, LoadWindowFileExposesPublicWindowInfoAndPages)
{
    mfd::runtime_api::RuntimeSession session;
    std::string error;

    ASSERT_TRUE(session.LoadWindowFile(RepositoryRoot() / "assets/windows/demo_pages_minimal.json", error)) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_FALSE(session.WindowFile().empty());

    const mfd::runtime_api::RuntimeWindowInfo windowInfo = session.WindowInfo();
    EXPECT_FALSE(windowInfo.title.empty());
    EXPECT_GT(windowInfo.width, 0);
    EXPECT_GT(windowInfo.height, 0);

    const std::vector<mfd::runtime_api::RuntimePageInfo> pages = session.Pages();
    ASSERT_FALSE(pages.empty());
    EXPECT_FALSE(session.ActivePageName().empty());
}

TEST(RuntimeSessionTests, ReloadPreservesCurrentActivePageWhenItStillExists)
{
    mfd::runtime_api::RuntimeSession session;
    std::string error;

    ASSERT_TRUE(session.LoadWindowFile(RepositoryRoot() / "assets/windows/demo_pages_minimal.json", error)) << error;

    const std::vector<mfd::runtime_api::RuntimePageInfo> pages = session.Pages();
    if (pages.size() < 2U)
    {
        GTEST_SKIP() << "The demo_pages_minimal fixture does not expose a secondary page.";
    }

    ASSERT_TRUE(session.SetActivePage(pages[1].name));
    ASSERT_EQ(session.ActivePageName(), pages[1].name);

    ASSERT_TRUE(session.Reload(error)) << error;
    EXPECT_EQ(session.ActivePageName(), pages[1].name);
}
