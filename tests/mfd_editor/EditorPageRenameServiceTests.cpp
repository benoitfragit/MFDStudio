/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the global page-rename planning service.
 */

#include "EditorPageRenameService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace
{
using json = nlohmann::json;

class ScopedTempDir
{
public:
    ScopedTempDir()
    {
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                std::filesystem::path("mfd_page_rename_tests_" + std::to_string(ticks));
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

void WriteTextFile(const std::filesystem::path& path, const std::string_view content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(stream.is_open()) << path.string();
    stream << content;
}

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    EXPECT_TRUE(stream.is_open()) << path.string();
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
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
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});

    loaded.document.pages.push_back(page);
    files.pageFiles.push_back(pageFile);
}

std::pair<mfd::LoadedWindowConfiguration, editor::EditorFileLayout> MakeLoadedWindow(
    const std::filesystem::path& windowFile,
    const std::vector<std::tuple<std::filesystem::path, std::string, bool>>& pages)
{
    mfd::LoadedWindowConfiguration loaded;
    editor::EditorFileLayout files;

    loaded.window.sourceFile = windowFile;
    for (const auto& [pageFile, pageName, defaultPage] : pages)
    {
        AddPage(loaded, files, pageFile, pageName, defaultPage);
    }

    loaded.window.pageFiles = files.pageFiles;
    return {loaded, files};
}

std::vector<std::filesystem::path> ChangedFiles(
    const std::unordered_map<std::string, std::string>& before,
    const std::vector<std::filesystem::path>& files)
{
    std::vector<std::filesystem::path> changed;
    for (const std::filesystem::path& file : files)
    {
        const std::string key = std::filesystem::absolute(file).lexically_normal().generic_string();
        const auto iterator = before.find(key);
        const std::string after = ReadTextFile(file);
        if (iterator == before.end() || iterator->second != after)
        {
            changed.push_back(std::filesystem::absolute(file).lexically_normal());
        }
    }

    std::sort(changed.begin(), changed.end());
    return changed;
}
} // namespace

TEST(PageRenameServiceTests, RenamesPageReferencedByOneWindowAndKeepsWrapperCompatibility)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path windowFile = assetRoot / "windows" / "demo.json";
    const std::filesystem::path homeFile = assetRoot / "pages" / "home.json";
    const std::filesystem::path radarFile = assetRoot / "pages" / "radar.json";

    WriteTextFile(windowFile,
                  R"json({
  "pages": [
    "../pages/home.json",
    "../pages/radar.json"
  ],
  "defaultPage": "Home"
})json");
    WriteTextFile(homeFile, R"json({"name":"Home","staticReticles":[]})json");
    WriteTextFile(radarFile,
                  R"json({
  "page": {
    "id": "Radar",
    "staticReticles": []
  }
})json");

    auto [loaded, files] = MakeLoadedWindow(
        windowFile,
        {
            {homeFile, "Home", true},
            {radarFile, "Radar", false},
        });

    editor::PageRenameService service;
    const editor::RenamePagePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenamePageRequest {1, "Tactical", assetRoot});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.references.size(), 1U);
    ASSERT_EQ(plan.filesToModify.size(), 1U);
    EXPECT_EQ(plan.filesToModify.front(), std::filesystem::absolute(radarFile).lexically_normal());

    editor::RenamePageResult result;
    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, &result, &error)) << error;
    EXPECT_TRUE(result.updatedCurrentDocument);
    EXPECT_EQ(loaded.document.pages[1].name, "Tactical");

    const json renamedPage = json::parse(ReadTextFile(radarFile));
    EXPECT_EQ(renamedPage.at("page").at("id").get<std::string>(), "Tactical");

    const json windowDocument = json::parse(ReadTextFile(windowFile));
    EXPECT_EQ(windowDocument.at("defaultPage").get<std::string>(), "Home");
}

TEST(PageRenameServiceTests, RenamesPageReferencedBySeveralWindows)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path currentWindowFile = assetRoot / "windows" / "current.json";
    const std::filesystem::path cockpitWindowFile = assetRoot / "windows" / "cockpit.json";
    const std::filesystem::path radarFile = assetRoot / "pages" / "radar.json";

    WriteTextFile(currentWindowFile,
                  R"json({
  "pages": [
    "../pages/radar.json"
  ],
  "defaultPage": "Radar"
})json");
    WriteTextFile(cockpitWindowFile,
                  R"json({
  "pages": [
    "../pages/radar.json"
  ],
  "defaultPage": "Radar"
})json");
    WriteTextFile(radarFile, R"json({"name":"Radar","staticReticles":[]})json");

    auto [loaded, files] = MakeLoadedWindow(
        currentWindowFile,
        {
            {radarFile, "Radar", true},
        });

    editor::PageRenameService service;
    const editor::RenamePagePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenamePageRequest {0, "Tactical", assetRoot});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.references.size(), 2U);
    ASSERT_EQ(plan.filesToModify.size(), 3U);

    editor::RenamePageResult result;
    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, &result, &error)) << error;
    EXPECT_EQ(result.updatedWindowCount, 2U);

    EXPECT_EQ(json::parse(ReadTextFile(currentWindowFile)).at("defaultPage").get<std::string>(), "Tactical");
    EXPECT_EQ(json::parse(ReadTextFile(cockpitWindowFile)).at("defaultPage").get<std::string>(), "Tactical");
    EXPECT_EQ(json::parse(ReadTextFile(radarFile)).at("name").get<std::string>(), "Tactical");
}

TEST(PageRenameServiceTests, RenamesWindowDefaultPageWhenNeeded)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path windowFile = assetRoot / "windows" / "demo.json";
    const std::filesystem::path homeFile = assetRoot / "pages" / "home.json";
    const std::filesystem::path radarFile = assetRoot / "pages" / "radar.json";

    WriteTextFile(windowFile,
                  R"json({
  "pages": [
    "../pages/home.json",
    "../pages/radar.json"
  ],
  "defaultPage": "Radar"
})json");
    WriteTextFile(homeFile, R"json({"name":"Home","staticReticles":[]})json");
    WriteTextFile(radarFile, R"json({"name":"Radar","staticReticles":[]})json");

    auto [loaded, files] = MakeLoadedWindow(
        windowFile,
        {
            {homeFile, "Home", false},
            {radarFile, "Radar", true},
        });

    editor::PageRenameService service;
    const editor::RenamePagePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenamePageRequest {1, "Tactical", assetRoot});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.references.size(), 1U);
    EXPECT_TRUE(plan.references.front().updatesDefaultPage);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;
    EXPECT_EQ(json::parse(ReadTextFile(windowFile)).at("defaultPage").get<std::string>(), "Tactical");
}

TEST(PageRenameServiceTests, RefusesNameCollisionInsideOneWindow)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path windowFile = assetRoot / "windows" / "demo.json";
    const std::filesystem::path radarFile = assetRoot / "pages" / "radar.json";
    const std::filesystem::path tacticalFile = assetRoot / "pages" / "tactical.json";

    WriteTextFile(windowFile,
                  R"json({
  "pages": [
    "../pages/radar.json",
    "../pages/tactical.json"
  ],
  "defaultPage": "Radar"
})json");
    WriteTextFile(radarFile, R"json({"name":"Radar","staticReticles":[]})json");
    WriteTextFile(tacticalFile, R"json({"name":"Tactical","staticReticles":[]})json");

    auto [loaded, files] = MakeLoadedWindow(
        windowFile,
        {
            {radarFile, "Radar", true},
            {tacticalFile, "Tactical", false},
        });

    editor::PageRenameService service;
    const editor::RenamePagePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenamePageRequest {0, "Tactical", assetRoot});

    EXPECT_FALSE(plan.canExecute);
    ASSERT_FALSE(plan.collisions.empty());
    EXPECT_NE(plan.error.find("already contains page name"), std::string::npos);
}

TEST(PageRenameServiceTests, ReportsClearErrorWhenNoWindowReferenceExistsUnderAssetsRoot)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path outsideWindowFile = tempDir.Path() / "external" / "draft.json";
    const std::filesystem::path radarFile = assetRoot / "pages" / "radar.json";

    WriteTextFile(radarFile, R"json({"name":"Radar","staticReticles":[]})json");

    auto [loaded, files] = MakeLoadedWindow(
        outsideWindowFile,
        {
            {radarFile, "Radar", true},
        });

    editor::PageRenameService service;
    const editor::RenamePagePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenamePageRequest {0, "Tactical", assetRoot});

    EXPECT_FALSE(plan.canExecute);
    EXPECT_NE(plan.error.find("No window under the scanned assets root references page"), std::string::npos);
}

TEST(PageRenameServiceTests, ReportsInvalidWindowJsonClearly)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path windowFile = assetRoot / "windows" / "demo.json";
    const std::filesystem::path brokenWindowFile = assetRoot / "windows" / "broken.json";
    const std::filesystem::path radarFile = assetRoot / "pages" / "radar.json";

    WriteTextFile(windowFile,
                  R"json({
  "pages": [
    "../pages/radar.json"
  ],
  "defaultPage": "Radar"
})json");
    WriteTextFile(brokenWindowFile, "{ invalid");
    WriteTextFile(radarFile, R"json({"name":"Radar","staticReticles":[]})json");

    auto [loaded, files] = MakeLoadedWindow(
        windowFile,
        {
            {radarFile, "Radar", true},
        });

    editor::PageRenameService service;
    const editor::RenamePagePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenamePageRequest {0, "Tactical", assetRoot});

    EXPECT_FALSE(plan.canExecute);
    EXPECT_NE(plan.error.find("valid window JSON files"), std::string::npos);
}

TEST(PageRenameServiceTests, BuildPlanDoesNotModifyFiles)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path windowFile = assetRoot / "windows" / "demo.json";
    const std::filesystem::path radarFile = assetRoot / "pages" / "radar.json";

    WriteTextFile(windowFile,
                  R"json({
  "pages": [
    "../pages/radar.json"
  ],
  "defaultPage": "Radar"
})json");
    WriteTextFile(radarFile, R"json({"name":"Radar","staticReticles":[]})json");

    auto [loaded, files] = MakeLoadedWindow(
        windowFile,
        {
            {radarFile, "Radar", true},
        });

    const std::string windowBefore = ReadTextFile(windowFile);
    const std::string pageBefore = ReadTextFile(radarFile);

    editor::PageRenameService service;
    const editor::RenamePagePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenamePageRequest {0, "Tactical", assetRoot});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_EQ(ReadTextFile(windowFile), windowBefore);
    EXPECT_EQ(ReadTextFile(radarFile), pageBefore);
}

TEST(PageRenameServiceTests, ExecuteModifiesOnlyExpectedFiles)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path currentWindowFile = assetRoot / "windows" / "current.json";
    const std::filesystem::path supportWindowFile = assetRoot / "windows" / "support.json";
    const std::filesystem::path unrelatedWindowFile = assetRoot / "windows" / "unrelated.json";
    const std::filesystem::path radarFile = assetRoot / "pages" / "radar.json";
    const std::filesystem::path homeFile = assetRoot / "pages" / "home.json";

    WriteTextFile(currentWindowFile,
                  R"json({
  "pages": [
    "../pages/radar.json"
  ],
  "defaultPage": "Radar"
})json");
    WriteTextFile(supportWindowFile,
                  R"json({
  "pages": [
    "../pages/home.json",
    "../pages/radar.json"
  ],
  "defaultPage": "Home"
})json");
    WriteTextFile(unrelatedWindowFile,
                  R"json({
  "pages": [
    "../pages/home.json"
  ],
  "defaultPage": "Home"
})json");
    WriteTextFile(radarFile, R"json({"name":"Radar","staticReticles":[]})json");
    WriteTextFile(homeFile, R"json({"name":"Home","staticReticles":[]})json");

    auto [loaded, files] = MakeLoadedWindow(
        currentWindowFile,
        {
            {radarFile, "Radar", true},
        });

    std::unordered_map<std::string, std::string> before;
    for (const std::filesystem::path& file : {currentWindowFile, supportWindowFile, unrelatedWindowFile, radarFile, homeFile})
    {
        before.emplace(std::filesystem::absolute(file).lexically_normal().generic_string(), ReadTextFile(file));
    }

    editor::PageRenameService service;
    const editor::RenamePagePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenamePageRequest {0, "Tactical", assetRoot});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.filesToModify.size(), 2U);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;

    std::vector<std::filesystem::path> changed =
        ChangedFiles(before, {currentWindowFile, supportWindowFile, unrelatedWindowFile, radarFile, homeFile});
    std::sort(changed.begin(), changed.end());

    std::vector<std::filesystem::path> expected = plan.filesToModify;
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(changed, expected);
}

TEST(PageRenameServiceTests, IncludesExecWindowsWhenScanningAssetRoot)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path windowFile = assetRoot / "windows" / "demo.json";
    const std::filesystem::path execWindowFile = assetRoot / "_Exec" / "windows" / "staged.json";
    const std::filesystem::path radarFile = assetRoot / "pages" / "radar.json";

    WriteTextFile(windowFile,
                  R"json({
  "pages": [
    "../pages/radar.json"
  ],
  "defaultPage": "Radar"
})json");
    WriteTextFile(execWindowFile,
                  R"json({
  "pages": [
    "../../pages/radar.json"
  ],
  "defaultPage": "Radar"
})json");
    WriteTextFile(radarFile, R"json({"name":"Radar","staticReticles":[]})json");

    auto [loaded, files] = MakeLoadedWindow(
        windowFile,
        {
            {radarFile, "Radar", true},
        });

    editor::PageRenameService service;
    const editor::RenamePagePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenamePageRequest {0, "Tactical", assetRoot});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.references.size(), 2U);
    ASSERT_EQ(plan.filesToModify.size(), 3U);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;
    EXPECT_EQ(json::parse(ReadTextFile(execWindowFile)).at("defaultPage").get<std::string>(), "Tactical");
}

TEST(PageRenameServiceTests, AllowsRenameInsideExecAssetTree)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "_Exec" / "v142" / "Win32" / "Debug" / "assets";
    const std::filesystem::path windowFile = assetRoot / "windows" / "demo.json";
    const std::filesystem::path radarFile = assetRoot / "pages" / "radar.json";

    WriteTextFile(windowFile,
                  R"json({
  "pages": [
    "../pages/radar.json"
  ],
  "defaultPage": "Radar"
})json");
    WriteTextFile(radarFile, R"json({"name":"Radar","staticReticles":[]})json");

    auto [loaded, files] = MakeLoadedWindow(
        windowFile,
        {
            {radarFile, "Radar", true},
        });

    editor::PageRenameService service;
    const editor::RenamePagePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenamePageRequest {0, "Tactical", assetRoot});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.references.size(), 1U);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;
    EXPECT_EQ(json::parse(ReadTextFile(radarFile)).at("name").get<std::string>(), "Tactical");
    EXPECT_EQ(json::parse(ReadTextFile(windowFile)).at("defaultPage").get<std::string>(), "Tactical");
}
