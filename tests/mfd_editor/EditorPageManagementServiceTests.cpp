/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the page-management planning service.
 */

#include "EditorPageManagementService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

namespace
{
class ScopedTempDir
{
public:
    ScopedTempDir()
    {
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                std::filesystem::path("mfd_page_management_tests_" + std::to_string(ticks));
        std::filesystem::create_directories(path_);
    }

    ~ScopedTempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void TouchFile(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    ASSERT_TRUE(output.is_open()) << path.string();
    output << "{}";
}

void AddPage(mfd::LoadedWindowConfiguration& loaded,
             editor::EditorFileLayout& files,
             const std::filesystem::path& pageFile,
             const std::string& name,
             const bool defaultPage)
{
    mfd::PageDefinition page;
    page.name = name;
    page.normalizedName = mfd::NormalizePageName(name);
    page.title = name;
    page.defaultPage = defaultPage;
    loaded.document.pages.push_back(page);
    files.pageFiles.push_back(pageFile);
}

std::pair<mfd::LoadedWindowConfiguration, editor::EditorFileLayout> MakeThreePageFixture(const std::filesystem::path& root)
{
    mfd::LoadedWindowConfiguration loaded;
    editor::EditorFileLayout files;

    loaded.window.sourceFile = root / "windows" / "demo.json";
    TouchFile(loaded.window.sourceFile);

    const std::filesystem::path alphaFile = root / "pages" / "alpha.json";
    const std::filesystem::path bravoFile = root / "pages" / "bravo.json";
    const std::filesystem::path charlieFile = root / "pages" / "charlie.json";
    TouchFile(alphaFile);
    TouchFile(bravoFile);
    TouchFile(charlieFile);

    AddPage(loaded, files, alphaFile, "Alpha", true);
    AddPage(loaded, files, bravoFile, "Bravo", false);
    AddPage(loaded, files, charlieFile, "Charlie", false);
    loaded.window.pageFiles = files.pageFiles;
    return {loaded, files};
}
} // namespace

TEST(PageManagementServiceTests, RemovePlanRemovesNonDefaultPageWithoutDeletingItsFile)
{
    ScopedTempDir tempDir;
    auto [loaded, files] = MakeThreePageFixture(tempDir.Path());
    const mfd::LoadedWindowConfiguration loadedBeforePlan = loaded;
    const editor::EditorFileLayout filesBeforePlan = files;

    editor::PageManagementService service;
    editor::PageRemoveRequest removeRequest;
    removeRequest.pageIndex = 1;
    const editor::PageRemovePlan plan = service.BuildRemovePlan(loaded, files, removeRequest);

    EXPECT_EQ(loaded.document.pages.size(), loadedBeforePlan.document.pages.size());
    EXPECT_EQ(files.pageFiles.size(), filesBeforePlan.pageFiles.size());
    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_EQ(plan.pageName, "Bravo");
    EXPECT_EQ(plan.nextSelectedPageIndex, 1);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, &error)) << error;
    ASSERT_TRUE(error.empty());
    ASSERT_EQ(loaded.document.pages.size(), 2U);
    EXPECT_EQ(loaded.document.pages[0].name, "Alpha");
    EXPECT_EQ(loaded.document.pages[1].name, "Charlie");
    EXPECT_TRUE(loaded.document.pages[0].defaultPage);
    EXPECT_TRUE(files.removedPageFiles.empty());
    ASSERT_EQ(files.pageFiles.size(), 2U);
    EXPECT_EQ(loaded.window.pageFiles, files.pageFiles);
}

TEST(PageManagementServiceTests, RemovePlanRequiresReplacementWhenDeletingDefaultPage)
{
    ScopedTempDir tempDir;
    auto [loaded, files] = MakeThreePageFixture(tempDir.Path());

    editor::PageManagementService service;

    editor::PageRemoveRequest invalidRequest;
    invalidRequest.pageIndex = 0;
    const editor::PageRemovePlan invalidPlan = service.BuildRemovePlan(loaded, files, invalidRequest);
    EXPECT_FALSE(invalidPlan.canExecute);
    EXPECT_NE(invalidPlan.error.find("replacement default page"), std::string::npos);

    editor::PageRemoveRequest validRequest;
    validRequest.pageIndex = 0;
    validRequest.replacementPageIndex = 2;
    const editor::PageRemovePlan validPlan = service.BuildRemovePlan(loaded, files, validRequest);
    ASSERT_TRUE(validPlan.canExecute) << validPlan.error;
    EXPECT_EQ(validPlan.replacementPageName, "Charlie");
    EXPECT_EQ(validPlan.nextSelectedPageIndex, 1);

    std::string error;
    ASSERT_TRUE(service.Execute(validPlan, loaded, files, &error)) << error;
    ASSERT_EQ(loaded.document.pages.size(), 2U);
    EXPECT_FALSE(loaded.document.pages[0].defaultPage);
    EXPECT_TRUE(loaded.document.pages[1].defaultPage);
    EXPECT_EQ(loaded.document.pages[1].name, "Charlie");
}

TEST(PageManagementServiceTests, RemovePlanRefusesToLeaveWindowWithoutPages)
{
    ScopedTempDir tempDir;
    mfd::LoadedWindowConfiguration loaded;
    editor::EditorFileLayout files;
    loaded.window.sourceFile = tempDir.Path() / "windows" / "demo.json";
    TouchFile(loaded.window.sourceFile);

    const std::filesystem::path pageFile = tempDir.Path() / "pages" / "alpha.json";
    TouchFile(pageFile);
    AddPage(loaded, files, pageFile, "Alpha", true);
    loaded.window.pageFiles = files.pageFiles;

    editor::PageManagementService service;
    editor::PageRemoveRequest removeRequest;
    removeRequest.pageIndex = 0;
    const editor::PageRemovePlan plan = service.BuildRemovePlan(loaded, files, removeRequest);

    EXPECT_FALSE(plan.canExecute);
    EXPECT_NE(plan.error.find("at least one page"), std::string::npos);
}

TEST(PageManagementServiceTests, DeletePlanMarksExistingAssetForRemovalOnSave)
{
    ScopedTempDir tempDir;
    auto [loaded, files] = MakeThreePageFixture(tempDir.Path());
    const std::filesystem::path assetsRoot = tempDir.Path();

    editor::PageManagementService service;
    editor::PageDeleteRequest deleteRequest;
    deleteRequest.pageIndex = 1;
    deleteRequest.assetsRoot = assetsRoot;
    const editor::PageDeletePlan plan = service.BuildDeletePlan(loaded, files, deleteRequest);

    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_EQ(plan.pageName, "Bravo");
    EXPECT_FALSE(plan.outsideAssetsRoot);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, &error)) << error;
    ASSERT_EQ(loaded.document.pages.size(), 2U);
    ASSERT_EQ(files.removedPageFiles.size(), 1U);
    EXPECT_EQ(files.removedPageFiles.front().filename().string(), "bravo.json");
    ASSERT_EQ(files.pageFiles.size(), 2U);
    EXPECT_EQ(loaded.window.pageFiles, files.pageFiles);
}

TEST(PageManagementServiceTests, DeletePlanRefusesOutsideAssetsRootWithoutAdvancedConfirmation)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path outsideRoot = tempDir.Path() / "external";

    mfd::LoadedWindowConfiguration loaded;
    editor::EditorFileLayout files;
    loaded.window.sourceFile = assetsRoot / "windows" / "demo.json";
    TouchFile(loaded.window.sourceFile);

    const std::filesystem::path alphaFile = assetsRoot / "pages" / "alpha.json";
    const std::filesystem::path rogueFile = outsideRoot / "rogue.json";
    TouchFile(alphaFile);
    TouchFile(rogueFile);
    AddPage(loaded, files, alphaFile, "Alpha", true);
    AddPage(loaded, files, rogueFile, "Rogue", false);
    loaded.window.pageFiles = files.pageFiles;

    editor::PageManagementService service;
    editor::PageDeleteRequest refusedRequest;
    refusedRequest.pageIndex = 1;
    refusedRequest.assetsRoot = assetsRoot;
    const editor::PageDeletePlan refusedPlan = service.BuildDeletePlan(loaded, files, refusedRequest);
    EXPECT_FALSE(refusedPlan.canExecute);
    EXPECT_TRUE(refusedPlan.outsideAssetsRoot);
    EXPECT_TRUE(refusedPlan.requiresAdvancedConfirmation);

    editor::PageDeleteRequest allowedRequest;
    allowedRequest.pageIndex = 1;
    allowedRequest.assetsRoot = assetsRoot;
    allowedRequest.allowOutsideAssetsRoot = true;
    const editor::PageDeletePlan allowedPlan = service.BuildDeletePlan(loaded, files, allowedRequest);
    ASSERT_TRUE(allowedPlan.canExecute) << allowedPlan.error;

    std::string error;
    ASSERT_TRUE(service.Execute(allowedPlan, loaded, files, &error)) << error;
    ASSERT_EQ(files.removedPageFiles.size(), 1U);
    EXPECT_EQ(files.removedPageFiles.front(), std::filesystem::absolute(rogueFile).lexically_normal());
}
