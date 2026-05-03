/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the editor reticle-template usage highlight service.
 */

#include "EditorReticleUsageHighlightService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
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
                std::filesystem::path("mfd_reticle_usage_highlight_tests_" + std::to_string(ticks));
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

void WritePageJson(const std::filesystem::path& pageFile,
                   const std::string& pageName,
                   const std::vector<std::pair<std::string, std::string>>& staticReticles,
                   const std::optional<std::pair<std::string, std::string>>& strobe = std::nullopt)
{
    json page = json::object();
    page["name"] = pageName;
    page["title"] = pageName;
    page["staticReticles"] = json::array();

    for (const auto& [reticleId, templateId] : staticReticles)
    {
        page["staticReticles"].push_back(json {
            {"id", reticleId},
            {"template", templateId}});
    }

    if (strobe.has_value())
    {
        page["strobe"] = json {
            {"id", strobe->first},
            {"template", strobe->second},
            {"capture", {{"shape", "circle"}, {"radius", 0.1f}}}};
    }

    WriteTextFile(pageFile, page.dump(2) + '\n');
}

void AddPage(mfd::LoadedWindowConfiguration& loaded,
             editor::EditorFileLayout& files,
             const std::filesystem::path& pageFile,
             const std::string& name,
             const std::vector<std::pair<std::string, std::string>>& staticReticles,
             const std::optional<std::pair<std::string, std::string>>& strobe = std::nullopt)
{
    mfd::PageDefinition page;
    page.name = name;
    page.normalizedName = mfd::NormalizePageName(name);
    page.title = name;
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});

    for (const auto& [reticleId, templateId] : staticReticles)
    {
        mfd::ReticleGroup reticle;
        reticle.id = reticleId;
        reticle.sourceTemplateId = templateId;
        reticle.editor.layerId = "layer";
        page.staticReticles.push_back(std::move(reticle));
    }

    if (strobe.has_value())
    {
        mfd::PageStrobeDefinition strobeDefinition;
        strobeDefinition.reticle.id = strobe->first;
        strobeDefinition.reticle.sourceTemplateId = strobe->second;
        page.strobe = std::move(strobeDefinition);
    }

    loaded.document.pages.push_back(std::move(page));
    files.pageFiles.push_back(pageFile);
}
} // namespace

TEST(ReticleUsageHighlightServiceTests, BuildHighlightFindsCurrentPageUsage)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";

    WritePageJson(radarFile, "Radar", {{"track_a", "track_box"}, {"track_b", "track_box"}});

    mfd::LoadedWindowConfiguration loaded;
    editor::EditorFileLayout files;
    AddPage(loaded, files, radarFile, "Radar", {{"track_a", "track_box"}, {"track_b", "track_box"}});

    editor::ReticleUsageHighlightService service;
    const editor::ReticleUsageHighlightResult result = service.BuildHighlight(
        loaded,
        files,
        editor::ReticleUsageHighlightRequest {"track_box", assetsRoot, false});

    ASSERT_TRUE(result.hasUsage);
    ASSERT_EQ(result.pages.size(), 1U);
    EXPECT_EQ(result.pages.front().pageName, "Radar");
    EXPECT_EQ(result.pages.front().currentPageIndex, 0);
    EXPECT_EQ(result.pages.front().matchingReticleIndices, std::vector<int>({0, 1}));
    EXPECT_FALSE(result.pages.front().matchingStrobe);
    EXPECT_EQ(result.totalReferenceCount, 2U);
}

TEST(ReticleUsageHighlightServiceTests, BuildHighlightMergesCurrentAndScannedPageUsages)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path cockpitFile = assetsRoot / "pages" / "cockpit.json";
    const std::filesystem::path supportFile = assetsRoot / "pages" / "support.json";

    WritePageJson(radarFile, "Radar", {{"track", "track_box"}});
    WritePageJson(cockpitFile, "Cockpit", {{"track", "track_box"}}, std::make_optional(std::make_pair(std::string("cursor"), std::string("track_box"))));
    WritePageJson(supportFile, "Support", {{"other", "other_template"}});

    mfd::LoadedWindowConfiguration loaded;
    editor::EditorFileLayout files;
    AddPage(loaded, files, radarFile, "Radar", {{"track", "track_box"}});

    editor::ReticleUsageHighlightService service;
    const editor::ReticleUsageHighlightResult result = service.BuildHighlight(
        loaded,
        files,
        editor::ReticleUsageHighlightRequest {"track_box", assetsRoot, false});

    ASSERT_TRUE(result.hasUsage);
    ASSERT_EQ(result.pages.size(), 2U);
    EXPECT_EQ(result.pages.front().pageName, "Radar");
    EXPECT_EQ(result.pages.front().currentPageIndex, 0);
    EXPECT_EQ(result.pages.back().pageName, "Cockpit");
    EXPECT_EQ(result.pages.back().currentPageIndex, -1);
    EXPECT_TRUE(result.pages.back().matchingStrobe);
    EXPECT_EQ(result.totalReferenceCount, 3U);
}

TEST(ReticleUsageHighlightServiceTests, BuildHighlightReturnsNoPagesWhenTemplateIsUnused)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";

    WritePageJson(radarFile, "Radar", {{"other", "other_template"}});

    mfd::LoadedWindowConfiguration loaded;
    editor::EditorFileLayout files;
    AddPage(loaded, files, radarFile, "Radar", {{"other", "other_template"}});

    editor::ReticleUsageHighlightService service;
    const editor::ReticleUsageHighlightResult result = service.BuildHighlight(
        loaded,
        files,
        editor::ReticleUsageHighlightRequest {"track_box", assetsRoot, false});

    EXPECT_FALSE(result.hasUsage);
    EXPECT_TRUE(result.pages.empty());
    EXPECT_EQ(result.totalReferenceCount, 0U);
}

TEST(ReticleUsageHighlightServiceTests, BuildHighlightIgnoresExecByDefault)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path execPageFile = assetsRoot / "_Exec" / "pages" / "staged.json";

    WritePageJson(radarFile, "Radar", {{"track", "track_box"}});
    WritePageJson(execPageFile, "Staged", {{"staged_track", "track_box"}});

    mfd::LoadedWindowConfiguration loaded;
    editor::EditorFileLayout files;
    AddPage(loaded, files, radarFile, "Radar", {{"track", "track_box"}});

    editor::ReticleUsageHighlightService service;
    const editor::ReticleUsageHighlightResult result = service.BuildHighlight(
        loaded,
        files,
        editor::ReticleUsageHighlightRequest {"track_box", assetsRoot, false});

    ASSERT_TRUE(result.hasUsage);
    ASSERT_EQ(result.pages.size(), 1U);
    EXPECT_EQ(result.pages.front().pageName, "Radar");
    EXPECT_EQ(result.totalReferenceCount, 1U);
}

TEST(ReticleUsageHighlightServiceTests, BuildHighlightIncludesExecAssetsWhenRequested)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path execAssetsRoot = assetsRoot / "_Exec";
    const std::filesystem::path execPageFile = execAssetsRoot / "pages" / "staged.json";

    WritePageJson(execPageFile, "Staged", {{"staged_track", "track_box"}});

    mfd::LoadedWindowConfiguration loaded;
    editor::EditorFileLayout files;

    editor::ReticleUsageHighlightService service;
    const editor::ReticleUsageHighlightResult result = service.BuildHighlight(
        loaded,
        files,
        editor::ReticleUsageHighlightRequest {"track_box", execAssetsRoot, true});

    ASSERT_TRUE(result.hasUsage);
    ASSERT_EQ(result.pages.size(), 1U);
    EXPECT_EQ(result.pages.front().pageName, "Staged");
    EXPECT_EQ(result.pages.front().currentPageIndex, -1);
    EXPECT_EQ(result.totalReferenceCount, 1U);
}
