/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the page-import planning service.
 */

#include "PageImportService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

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
                std::filesystem::path("mfd_page_import_tests_" + std::to_string(ticks));
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
    std::ofstream stream(path, std::ios::binary);
    ASSERT_TRUE(stream.is_open()) << path.string();
    stream << content;
}

mfd::PageDefinition MakePage(const std::string& name, const bool defaultPage)
{
    mfd::PageDefinition page;
    page.name = name;
    page.normalizedName = mfd::NormalizePageName(name);
    page.title = name;
    page.defaultPage = defaultPage;
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
    return page;
}

mfd::ReticleGroup MakeTextTemplate(const std::string& id, const std::string& text)
{
    mfd::ReticleGroup reticle;
    reticle.id = id;
    reticle.sourceTemplateId = id;

    mfd::Primitive primitive;
    primitive.id = "label";
    primitive.type = mfd::PrimitiveType::Text;
    primitive.geometry = mfd::TextGeometry {text, 0.04f};
    reticle.primitives.push_back(std::move(primitive));
    return reticle;
}

std::pair<mfd::LoadedWindowConfiguration, editor::EditorFileLayout> MakeCurrentWindowFixture(const std::filesystem::path& root)
{
    const std::filesystem::path assetsRoot = root / "current" / "assets";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "current_window.json";
    const std::filesystem::path pageFile = assetsRoot / "pages" / "home.json";
    const std::filesystem::path reticleFolder = assetsRoot / "reticles";

    WriteTextFile(windowFile,
                  R"json({
  "title": "Current",
  "reticleLibraryFolder": "../reticles",
  "pages": [
    "../pages/home.json"
  ],
  "defaultPage": "Home"
})json");
    WriteTextFile(pageFile,
                  R"json({
  "name": "Home",
  "title": "Home",
  "staticReticles": []
})json");
    std::filesystem::create_directories(reticleFolder);

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = windowFile;
    loaded.window.reticleLibraryFolder = reticleFolder;
    loaded.document.pages.push_back(MakePage("Home", true));

    editor::EditorFileLayout files;
    files.pageFiles.push_back(pageFile);
    loaded.window.pageFiles = files.pageFiles;
    return {loaded, files};
}

editor::PageImportRequest MakeImportRequest(const mfd::LoadedWindowConfiguration& loaded,
                                            const std::filesystem::path& sourcePageFile)
{
    return editor::PageImportRequest {
        sourcePageFile,
        loaded.window.sourceFile.parent_path().parent_path() / "pages",
        loaded.window.reticleLibraryFolder};
}
} // namespace

TEST(PageImportServiceTests, ImportsSimplePageWithoutReticleDependency)
{
    ScopedTempDir tempDir;
    auto [loaded, files] = MakeCurrentWindowFixture(tempDir.Path());

    const std::filesystem::path sourcePageFile = tempDir.Path() / "source" / "assets" / "pages" / "support.json";
    WriteTextFile(sourcePageFile,
                  R"json({
  "name": "Support",
  "title": "Support",
  "staticReticles": []
})json");

    editor::PageImportService service;
    const editor::PageImportPlan plan = service.BuildPlan(loaded, files, MakeImportRequest(loaded, sourcePageFile));

    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_EQ(plan.sourcePageName, "Support");
    EXPECT_EQ(plan.targetPageName, "Support");
    EXPECT_TRUE(plan.reticles.empty());

    editor::PageImportResult result;
    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, &result, &error)) << error;
    ASSERT_EQ(loaded.document.pages.size(), 2U);
    EXPECT_EQ(loaded.document.pages.back().name, "Support");
    ASSERT_EQ(files.pageFiles.size(), 2U);
    EXPECT_EQ(result.importedPageIndex, 1);

    ASSERT_TRUE(editor::SaveEditorDocument(loaded, files, &error)) << error;
    EXPECT_TRUE(std::filesystem::exists(plan.targetPageFile));
}

TEST(PageImportServiceTests, ImportsPageWithReticleDependencyIntoCurrentLibrary)
{
    ScopedTempDir tempDir;
    auto [loaded, files] = MakeCurrentWindowFixture(tempDir.Path());

    const std::filesystem::path sourceRoot = tempDir.Path() / "source" / "assets";
    const std::filesystem::path sourcePageFile = sourceRoot / "pages" / "radar.json";
    const std::filesystem::path sourceTemplateFile = sourceRoot / "reticles" / "track_box.json";

    WriteTextFile(sourceTemplateFile,
                  R"json({
  "id": "track_box",
  "elements": [
    {
      "id": "label",
      "type": "text",
      "text": "TRK",
      "size": 0.04
    }
  ]
})json");
    WriteTextFile(sourcePageFile,
                  R"json({
  "name": "Radar",
  "staticReticles": [
    {
      "id": "track_1",
      "template": "track_box"
    }
  ]
})json");

    editor::PageImportService service;
    const editor::PageImportPlan plan = service.BuildPlan(loaded, files, MakeImportRequest(loaded, sourcePageFile));

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.reticles.size(), 1U);
    EXPECT_EQ(plan.reticles.front().sourceTemplateId, "track_box");
    EXPECT_EQ(plan.reticles.front().targetTemplateId, "track_box");
    EXPECT_EQ(plan.reticles.front().disposition, editor::ImportDisposition::CopyNew);

    editor::PageImportResult result;
    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, &result, &error)) << error;
    ASSERT_EQ(loaded.document.pages.size(), 2U);
    ASSERT_EQ(loaded.document.pages.back().staticReticles.size(), 1U);
    EXPECT_EQ(loaded.document.pages.back().staticReticles.front().sourceTemplateId, "track_box");
    ASSERT_TRUE(loaded.document.reticleLibrary.find("track_box") != loaded.document.reticleLibrary.end());
    ASSERT_TRUE(files.templateFiles.find("track_box") != files.templateFiles.end());
}

TEST(PageImportServiceTests, RewritesRelativeImagePathsFromImportedReticleTemplates)
{
    ScopedTempDir tempDir;
    auto [loaded, files] = MakeCurrentWindowFixture(tempDir.Path());

    const std::filesystem::path sourceRoot = tempDir.Path() / "source" / "assets";
    const std::filesystem::path sourcePageFile = sourceRoot / "pages" / "picture.json";
    const std::filesystem::path sourceTemplateFile = sourceRoot / "reticles" / "badge.json";
    const std::filesystem::path sourceImageFile = sourceRoot / "picture" / "badge.png";

    WriteTextFile(sourceImageFile, "png");
    WriteTextFile(sourceTemplateFile,
                  R"json({
  "id": "badge",
  "elements": [
    {
      "id": "image_1",
      "type": "image",
      "file": "../picture/badge.png",
      "width": 0.25,
      "height": 0.12
    }
  ]
})json");
    WriteTextFile(sourcePageFile,
                  R"json({
  "name": "Picture",
  "staticReticles": [
    {
      "id": "badge_1",
      "template": "badge"
    }
  ]
})json");

    editor::PageImportService service;
    const editor::PageImportPlan plan = service.BuildPlan(loaded, files, MakeImportRequest(loaded, sourcePageFile));
    ASSERT_TRUE(plan.canExecute) << plan.error;

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;
    ASSERT_TRUE(editor::SaveEditorDocument(loaded, files, &error)) << error;

    std::ifstream importedTemplateStream(plan.reticles.front().targetFile, std::ios::binary);
    ASSERT_TRUE(importedTemplateStream.is_open());
    const json importedTemplateJson = json::parse(importedTemplateStream);
    const std::string savedRelativeFile =
        importedTemplateJson.at("elements").at(0).at("file").get<std::string>();
    const std::string expectedRelativeFile =
        std::filesystem::relative(sourceImageFile, plan.reticles.front().targetFile.parent_path()).generic_string();
    EXPECT_EQ(savedRelativeFile, expectedRelativeFile);
}

TEST(PageImportServiceTests, RenamesImportedPageWhenNameAlreadyExistsInCurrentWindow)
{
    ScopedTempDir tempDir;
    auto [loaded, files] = MakeCurrentWindowFixture(tempDir.Path());

    const std::filesystem::path sourcePageFile = tempDir.Path() / "source" / "assets" / "pages" / "home.json";
    WriteTextFile(sourcePageFile,
                  R"json({
  "name": "Home",
  "title": "Imported Home",
  "staticReticles": []
})json");

    editor::PageImportService service;
    const editor::PageImportPlan plan = service.BuildPlan(loaded, files, MakeImportRequest(loaded, sourcePageFile));

    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_EQ(plan.targetPageName, "Home_Imported");
    EXPECT_EQ(plan.pageDisposition, editor::ImportDisposition::RenameCopy);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;
    EXPECT_EQ(loaded.document.pages.back().name, "Home_Imported");
}

TEST(PageImportServiceTests, KeepsExistingReticleTemplateWhenTheCurrentLibraryAlreadyMatches)
{
    ScopedTempDir tempDir;
    auto [loaded, files] = MakeCurrentWindowFixture(tempDir.Path());

    const std::filesystem::path sourceRoot = tempDir.Path() / "source" / "assets";
    const std::filesystem::path sourcePageFile = sourceRoot / "pages" / "radar.json";
    const std::filesystem::path sourceTemplateFile = sourceRoot / "reticles" / "track_box.json";
    const std::string templateJson = R"json({
  "id": "track_box",
  "elements": [
    {
      "id": "label",
      "type": "text",
      "text": "TRK",
      "size": 0.04
    }
  ]
})json";
    WriteTextFile(sourceTemplateFile, templateJson);
    WriteTextFile(sourcePageFile,
                  R"json({
  "name": "Radar",
  "staticReticles": [
    {
      "id": "track_1",
      "template": "track_box"
    }
  ]
})json");

    const std::filesystem::path currentTemplateFile = loaded.window.reticleLibraryFolder / "track_box.json";
    WriteTextFile(currentTemplateFile, templateJson);

    mfd::JsonLoader loader;
    loaded = loader.LoadWindowConfiguration(loaded.window.sourceFile);
    files.pageFiles = loaded.window.pageFiles;
    files.templateFiles.clear();
    ASSERT_TRUE(editor::DiscoverReticleTemplateFiles(loaded.window.reticleLibraryFolder, files, nullptr));

    editor::PageImportService service;
    const editor::PageImportPlan plan = service.BuildPlan(loaded, files, MakeImportRequest(loaded, sourcePageFile));

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.reticles.size(), 1U);
    EXPECT_EQ(plan.reticles.front().disposition, editor::ImportDisposition::KeepExisting);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;
    EXPECT_EQ(loaded.document.pages.back().staticReticles.front().sourceTemplateId, "track_box");
    EXPECT_EQ(loaded.document.reticleLibrary.size(), 1U);
}

TEST(PageImportServiceTests, RenamesImportedReticleTemplateWhenTheCurrentLibraryDiffers)
{
    ScopedTempDir tempDir;
    auto [loaded, files] = MakeCurrentWindowFixture(tempDir.Path());

    loaded.document.reticleLibrary.emplace("track_box", MakeTextTemplate("track_box", "LOCAL"));
    files.templateFiles.emplace("track_box", loaded.window.reticleLibraryFolder / "track_box.json");

    const std::filesystem::path sourceRoot = tempDir.Path() / "source" / "assets";
    const std::filesystem::path sourcePageFile = sourceRoot / "pages" / "radar.json";
    const std::filesystem::path sourceTemplateFile = sourceRoot / "reticles" / "track_box.json";
    WriteTextFile(sourceTemplateFile,
                  R"json({
  "id": "track_box",
  "elements": [
    {
      "id": "label",
      "type": "text",
      "text": "REMOTE",
      "size": 0.04
    }
  ]
})json");
    WriteTextFile(sourcePageFile,
                  R"json({
  "name": "Radar",
  "staticReticles": [
    {
      "id": "track_1",
      "template": "track_box"
    }
  ]
})json");

    editor::PageImportService service;
    const editor::PageImportPlan plan = service.BuildPlan(loaded, files, MakeImportRequest(loaded, sourcePageFile));

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.reticles.size(), 1U);
    EXPECT_EQ(plan.reticles.front().disposition, editor::ImportDisposition::RenameCopy);
    EXPECT_EQ(plan.reticles.front().targetTemplateId, "track_box_Imported");

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;
    ASSERT_TRUE(loaded.document.reticleLibrary.find("track_box") != loaded.document.reticleLibrary.end());
    ASSERT_TRUE(loaded.document.reticleLibrary.find("track_box_Imported") != loaded.document.reticleLibrary.end());
    EXPECT_EQ(loaded.document.pages.back().staticReticles.front().sourceTemplateId, "track_box_Imported");
}

TEST(PageImportServiceTests, ReportsMissingReticleDependencyClearly)
{
    ScopedTempDir tempDir;
    auto [loaded, files] = MakeCurrentWindowFixture(tempDir.Path());

    const std::filesystem::path sourcePageFile = tempDir.Path() / "source" / "assets" / "pages" / "radar.json";
    WriteTextFile(sourcePageFile,
                  R"json({
  "name": "Radar",
  "staticReticles": [
    {
      "id": "track_1",
      "template": "missing_box"
    }
  ]
})json");

    editor::PageImportService service;
    const editor::PageImportPlan plan = service.BuildPlan(loaded, files, MakeImportRequest(loaded, sourcePageFile));

    EXPECT_FALSE(plan.canExecute);
    EXPECT_NE(plan.error.find("missing_box"), std::string::npos);
}

TEST(PageImportServiceTests, ReportsInvalidPageJsonClearly)
{
    ScopedTempDir tempDir;
    auto [loaded, files] = MakeCurrentWindowFixture(tempDir.Path());

    const std::filesystem::path sourcePageFile = tempDir.Path() / "source" / "assets" / "pages" / "broken.json";
    WriteTextFile(sourcePageFile, "{ invalid");

    editor::PageImportService service;
    const editor::PageImportPlan plan = service.BuildPlan(loaded, files, MakeImportRequest(loaded, sourcePageFile));

    EXPECT_FALSE(plan.canExecute);
    EXPECT_FALSE(plan.error.empty());
}

TEST(PageImportServiceTests, DroppedPageJsonCanReuseTheSameImportPlanningPath)
{
    ScopedTempDir tempDir;
    auto [loaded, files] = MakeCurrentWindowFixture(tempDir.Path());

    const std::filesystem::path sourcePageFile = tempDir.Path() / "source" / "assets" / "pages" / "dropped.json";
    WriteTextFile(sourcePageFile,
                  R"json({
  "page": {
    "id": "DroppedPage",
    "staticReticles": []
  }
})json");

    editor::PageImportService service;
    const editor::PageImportPlan plan = service.BuildPlan(loaded, files, MakeImportRequest(loaded, sourcePageFile));

    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_TRUE(plan.sourceUsesPageWrapper);
    EXPECT_EQ(plan.sourcePageName, "DroppedPage");
}
