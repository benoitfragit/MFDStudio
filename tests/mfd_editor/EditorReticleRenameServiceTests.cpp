/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the global reticle-template rename planning service.
 */

#include "EditorReticleRenameService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
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
                std::filesystem::path("mfd_reticle_rename_tests_" + std::to_string(ticks));
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

void WriteWindowJson(const std::filesystem::path& windowFile,
                     const std::filesystem::path& reticleFolder,
                     const std::vector<std::filesystem::path>& pageFiles,
                     const std::string& defaultPage)
{
    json window = json::object();
    window["title"] = "Current";
    window["reticleLibraryFolder"] =
        std::filesystem::relative(reticleFolder, windowFile.parent_path()).generic_string();
    window["pages"] = json::array();
    for (const std::filesystem::path& pageFile : pageFiles)
    {
        window["pages"].push_back(std::filesystem::relative(pageFile, windowFile.parent_path()).generic_string());
    }
    if (!defaultPage.empty())
    {
        window["defaultPage"] = defaultPage;
    }

    WriteTextFile(windowFile, window.dump(2) + '\n');
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
        json reticle = json::object();
        reticle["id"] = reticleId;
        reticle["template"] = templateId;
        page["staticReticles"].push_back(std::move(reticle));
    }

    if (strobe.has_value())
    {
        json strobeNode = json::object();
        strobeNode["id"] = strobe->first;
        strobeNode["template"] = strobe->second;
        strobeNode["capture"] = {{"shape", "circle"}, {"radius", 0.1f}};
        page["strobe"] = std::move(strobeNode);
    }

    WriteTextFile(pageFile, page.dump(2) + '\n');
}

void WriteTemplateJson(const std::filesystem::path& templateFile,
                       const std::string& templateId,
                       const std::string_view imageFile = {})
{
    json primitive = json::object();
    primitive["id"] = "body";
    if (imageFile.empty())
    {
        primitive["type"] = "text";
        primitive["text"] = "T";
        primitive["size"] = 0.04f;
    }
    else
    {
        primitive["type"] = "image";
        primitive["file"] = std::string(imageFile);
        primitive["size"] = json::array({0.2f, 0.2f});
    }

    json document = json::object();
    document["id"] = templateId;
    document["elements"] = json::array({primitive});
    WriteTextFile(templateFile, document.dump(2) + '\n');
}

mfd::PageDefinition MakePage(const std::string& name, const bool defaultPage = false)
{
    mfd::PageDefinition page;
    page.name = name;
    page.normalizedName = mfd::NormalizePageName(name);
    page.title = name;
    page.defaultPage = defaultPage;
    return page;
}

void AddStaticTemplateReference(mfd::PageDefinition& page,
                                const std::string& reticleId,
                                const std::string& templateId)
{
    mfd::ReticleGroup reticle;
    reticle.id = reticleId;
    reticle.sourceTemplateId = templateId;
    page.staticReticles.push_back(std::move(reticle));
}

void SetStrobeTemplateReference(mfd::PageDefinition& page,
                                const std::string& reticleId,
                                const std::string& templateId)
{
    mfd::PageStrobeDefinition strobe;
    strobe.reticle.id = reticleId;
    strobe.reticle.sourceTemplateId = templateId;
    strobe.capture.shape = mfd::StrobeCaptureShape::Circle;
    strobe.capture.radius = 0.1f;
    page.strobe = std::move(strobe);
}

mfd::ReticleGroup MakeTemplate(const std::string& templateId)
{
    mfd::ReticleGroup reticle;
    reticle.id = templateId;
    reticle.sourceTemplateId = templateId;

    mfd::Primitive primitive;
    primitive.id = "body";
    primitive.type = mfd::PrimitiveType::Text;
    primitive.geometry = mfd::TextGeometry {"T", 0.04f};
    reticle.primitives.push_back(std::move(primitive));
    return reticle;
}

mfd::ReticleGroup MakeImageTemplate(const std::string& templateId, const std::filesystem::path& imageFile)
{
    mfd::ReticleGroup reticle;
    reticle.id = templateId;
    reticle.sourceTemplateId = templateId;

    mfd::Primitive primitive;
    primitive.id = "image";
    primitive.type = mfd::PrimitiveType::Image;
    primitive.geometry = mfd::ImageGeometry {imageFile, 0.2f, 0.2f};
    reticle.primitives.push_back(std::move(primitive));
    return reticle;
}

void AddPage(mfd::LoadedWindowConfiguration& loaded,
             editor::EditorFileLayout& files,
             const std::filesystem::path& pageFile,
             mfd::PageDefinition page)
{
    loaded.document.pages.push_back(std::move(page));
    files.pageFiles.push_back(pageFile);
}

void AddTemplate(mfd::LoadedWindowConfiguration& loaded,
                 editor::EditorFileLayout& files,
                 const std::filesystem::path& templateFile,
                 mfd::ReticleGroup reticle)
{
    files.templateFiles[reticle.id] = templateFile;
    loaded.document.reticleLibrary.emplace(reticle.id, std::move(reticle));
}

std::pair<mfd::LoadedWindowConfiguration, editor::EditorFileLayout> MakeLoadedWindow(
    const std::filesystem::path& windowFile,
    const std::filesystem::path& reticleFolder,
    const std::vector<std::pair<std::filesystem::path, mfd::PageDefinition>>& pages)
{
    mfd::LoadedWindowConfiguration loaded;
    editor::EditorFileLayout files;

    loaded.window.sourceFile = windowFile;
    loaded.window.reticleLibraryFolder = reticleFolder;
    for (const auto& [pageFile, page] : pages)
    {
        AddPage(loaded, files, pageFile, page);
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

TEST(ReticleRenameServiceTests, RenamesReticleUsedByOnePage)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path reticleFolder = assetsRoot / "reticles";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "current.json";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path templateFile = reticleFolder / "track_box.json";

    WritePageJson(radarFile, "Radar", {{"track", "track_box"}});
    WriteTemplateJson(templateFile, "track_box");
    WriteWindowJson(windowFile, reticleFolder, {radarFile}, "Radar");

    mfd::PageDefinition radarPage = MakePage("Radar", true);
    AddStaticTemplateReference(radarPage, "track", "track_box");

    auto [loaded, files] = MakeLoadedWindow(windowFile, reticleFolder, {{radarFile, radarPage}});
    AddTemplate(loaded, files, templateFile, MakeTemplate("track_box"));

    editor::ReticleRenameService service;
    const editor::RenameReticlePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenameReticleRequest {"track_box", "threat_box", assetsRoot, false});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.references.size(), 1U);
    EXPECT_EQ(plan.filesToModify.size(), 2U);
    EXPECT_TRUE(plan.filesToDelete.empty());

    editor::RenameReticleResult result;
    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, &result, &error)) << error;

    EXPECT_TRUE(loaded.document.reticleLibrary.find("track_box") == loaded.document.reticleLibrary.end());
    EXPECT_TRUE(loaded.document.reticleLibrary.find("threat_box") != loaded.document.reticleLibrary.end());
    ASSERT_EQ(loaded.document.pages.front().staticReticles.size(), 1U);
    EXPECT_EQ(loaded.document.pages.front().staticReticles.front().sourceTemplateId, "threat_box");

    const json pageDocument = json::parse(ReadTextFile(radarFile));
    ASSERT_TRUE(pageDocument.at("staticReticles").is_array());
    EXPECT_EQ(pageDocument.at("staticReticles").front().at("template").get<std::string>(), "threat_box");

    const json templateDocument = json::parse(ReadTextFile(templateFile));
    EXPECT_EQ(templateDocument.at("id").get<std::string>(), "threat_box");
    EXPECT_EQ(result.updatedPageCount, 1U);
}

TEST(ReticleRenameServiceTests, RenamesReticleUsedBySeveralPagesIncludingStrobe)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path reticleFolder = assetsRoot / "reticles";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "current.json";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path supportFile = assetsRoot / "pages" / "support.json";
    const std::filesystem::path tacticalFile = assetsRoot / "pages" / "tactical.json";
    const std::filesystem::path templateFile = reticleFolder / "track_box.json";

    WritePageJson(radarFile, "Radar", {{"track", "track_box"}});
    WritePageJson(supportFile, "Support", {}, std::make_optional(std::make_pair(std::string("cursor"), std::string("track_box"))));
    WritePageJson(tacticalFile, "Tactical", {{"secondary", "track_box"}});
    WriteTemplateJson(templateFile, "track_box");
    WriteWindowJson(windowFile, reticleFolder, {radarFile, supportFile}, "Radar");

    mfd::PageDefinition radarPage = MakePage("Radar", true);
    AddStaticTemplateReference(radarPage, "track", "track_box");

    mfd::PageDefinition supportPage = MakePage("Support");
    SetStrobeTemplateReference(supportPage, "cursor", "track_box");

    auto [loaded, files] = MakeLoadedWindow(windowFile, reticleFolder, {{radarFile, radarPage}, {supportFile, supportPage}});
    AddTemplate(loaded, files, templateFile, MakeTemplate("track_box"));

    editor::ReticleRenameService service;
    const editor::RenameReticlePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenameReticleRequest {"track_box", "threat_box", assetsRoot, false});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.references.size(), 3U);
    ASSERT_EQ(plan.filesToModify.size(), 4U);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;

    EXPECT_EQ(loaded.document.pages[0].staticReticles.front().sourceTemplateId, "threat_box");
    ASSERT_TRUE(loaded.document.pages[1].strobe.has_value());
    EXPECT_EQ(loaded.document.pages[1].strobe->reticle.sourceTemplateId, "threat_box");
    EXPECT_EQ(json::parse(ReadTextFile(tacticalFile)).at("staticReticles").front().at("template").get<std::string>(), "threat_box");
}

TEST(ReticleRenameServiceTests, RefusesCollisionInsideOnePage)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path reticleFolder = assetsRoot / "reticles";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "current.json";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path templateFile = reticleFolder / "track_box.json";

    WritePageJson(radarFile, "Radar", {{"track", "track_box"}, {"existing", "threat_box"}});
    WriteTemplateJson(templateFile, "track_box");
    WriteWindowJson(windowFile, reticleFolder, {radarFile}, "Radar");

    mfd::PageDefinition radarPage = MakePage("Radar", true);
    AddStaticTemplateReference(radarPage, "track", "track_box");
    AddStaticTemplateReference(radarPage, "existing", "threat_box");

    auto [loaded, files] = MakeLoadedWindow(windowFile, reticleFolder, {{radarFile, radarPage}});
    AddTemplate(loaded, files, templateFile, MakeTemplate("track_box"));

    editor::ReticleRenameService service;
    const editor::RenameReticlePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenameReticleRequest {"track_box", "threat_box", assetsRoot, false});

    EXPECT_FALSE(plan.canExecute);
    ASSERT_FALSE(plan.collisions.empty());
    EXPECT_NE(plan.error.find("already references target template id"), std::string::npos);
}

TEST(ReticleRenameServiceTests, ReportsClearErrorWhenReticleIsMissing)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path reticleFolder = assetsRoot / "reticles";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "current.json";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";

    WritePageJson(radarFile, "Radar", {{"track", "track_box"}});
    WriteWindowJson(windowFile, reticleFolder, {radarFile}, "Radar");

    mfd::PageDefinition radarPage = MakePage("Radar", true);
    AddStaticTemplateReference(radarPage, "track", "track_box");

    auto [loaded, files] = MakeLoadedWindow(windowFile, reticleFolder, {{radarFile, radarPage}});

    editor::ReticleRenameService service;
    const editor::RenameReticlePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenameReticleRequest {"unknown_template", "threat_box", assetsRoot, false});

    EXPECT_FALSE(plan.canExecute);
    EXPECT_NE(plan.error.find("does not exist in the current editor document"), std::string::npos);
}

TEST(ReticleRenameServiceTests, LogicalRenameOnlyKeepsTemplateFilePath)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path reticleFolder = assetsRoot / "reticles";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "current.json";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path templateFile = reticleFolder / "track_box.json";

    WritePageJson(radarFile, "Radar", {{"track", "track_box"}});
    WriteTemplateJson(templateFile, "track_box");
    WriteWindowJson(windowFile, reticleFolder, {radarFile}, "Radar");

    mfd::PageDefinition radarPage = MakePage("Radar", true);
    AddStaticTemplateReference(radarPage, "track", "track_box");

    auto [loaded, files] = MakeLoadedWindow(windowFile, reticleFolder, {{radarFile, radarPage}});
    AddTemplate(loaded, files, templateFile, MakeTemplate("track_box"));

    editor::ReticleRenameService service;
    const editor::RenameReticlePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenameReticleRequest {"track_box", "threat_box", assetsRoot, false});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_EQ(plan.currentTemplateFile, plan.targetTemplateFile);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;
    EXPECT_TRUE(std::filesystem::exists(templateFile));
    EXPECT_FALSE(std::filesystem::exists(reticleFolder / "threat_box.json"));
}

TEST(ReticleRenameServiceTests, LogicalRenamePlusFileMoveDeletesOldTemplateJson)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path reticleFolder = assetsRoot / "reticles";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "current.json";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path templateFile = reticleFolder / "track_box.json";
    const std::filesystem::path renamedTemplateFile = reticleFolder / "threat_box.json";

    WritePageJson(radarFile, "Radar", {{"track", "track_box"}});
    WriteTemplateJson(templateFile, "track_box");
    WriteWindowJson(windowFile, reticleFolder, {radarFile}, "Radar");

    mfd::PageDefinition radarPage = MakePage("Radar", true);
    AddStaticTemplateReference(radarPage, "track", "track_box");

    auto [loaded, files] = MakeLoadedWindow(windowFile, reticleFolder, {{radarFile, radarPage}});
    AddTemplate(loaded, files, templateFile, MakeTemplate("track_box"));

    editor::ReticleRenameService service;
    const editor::RenameReticlePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenameReticleRequest {"track_box", "threat_box", assetsRoot, true});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_EQ(plan.targetTemplateFile, std::filesystem::absolute(renamedTemplateFile).lexically_normal());
    ASSERT_EQ(plan.filesToDelete.size(), 1U);

    editor::RenameReticleResult result;
    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, &result, &error)) << error;

    EXPECT_FALSE(std::filesystem::exists(templateFile));
    EXPECT_TRUE(std::filesystem::exists(renamedTemplateFile));
    EXPECT_TRUE(result.renamedTemplateFile);
    EXPECT_EQ(json::parse(ReadTextFile(renamedTemplateFile)).at("id").get<std::string>(), "threat_box");
}

TEST(ReticleRenameServiceTests, RenamesTemplateFileAndRewritesRelativeImagePaths)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path reticleFolder = assetsRoot / "reticles";
    const std::filesystem::path nestedTemplateFolder = reticleFolder / "custom";
    const std::filesystem::path imageFile = assetsRoot / "images" / "cursor.png";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "current.json";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path templateFile = nestedTemplateFolder / "track_box.json";
    const std::filesystem::path renamedTemplateFile = reticleFolder / "threat_box.json";

    WriteTextFile(imageFile, "not-a-real-image");
    WritePageJson(radarFile, "Radar", {{"track", "track_box"}});
    WriteTemplateJson(templateFile, "track_box", "../../images/cursor.png");
    WriteWindowJson(windowFile, reticleFolder, {radarFile}, "Radar");

    mfd::PageDefinition radarPage = MakePage("Radar", true);
    AddStaticTemplateReference(radarPage, "track", "track_box");

    auto [loaded, files] = MakeLoadedWindow(windowFile, reticleFolder, {{radarFile, radarPage}});
    AddTemplate(loaded, files, templateFile, MakeImageTemplate("track_box", imageFile));

    editor::ReticleRenameService service;
    const editor::RenameReticlePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenameReticleRequest {"track_box", "threat_box", assetsRoot, true});

    ASSERT_TRUE(plan.canExecute) << plan.error;

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;

    const json renamedTemplate = json::parse(ReadTextFile(renamedTemplateFile));
    ASSERT_TRUE(renamedTemplate.at("elements").is_array());
    EXPECT_EQ(renamedTemplate.at("elements").front().at("file").get<std::string>(), "../images/cursor.png");
}

TEST(ReticleRenameServiceTests, BuildPlanDoesNotModifyFiles)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path reticleFolder = assetsRoot / "reticles";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "current.json";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path templateFile = reticleFolder / "track_box.json";

    WritePageJson(radarFile, "Radar", {{"track", "track_box"}});
    WriteTemplateJson(templateFile, "track_box");
    WriteWindowJson(windowFile, reticleFolder, {radarFile}, "Radar");

    mfd::PageDefinition radarPage = MakePage("Radar", true);
    AddStaticTemplateReference(radarPage, "track", "track_box");

    auto [loaded, files] = MakeLoadedWindow(windowFile, reticleFolder, {{radarFile, radarPage}});
    AddTemplate(loaded, files, templateFile, MakeTemplate("track_box"));

    const std::string pageBefore = ReadTextFile(radarFile);
    const std::string templateBefore = ReadTextFile(templateFile);

    editor::ReticleRenameService service;
    const editor::RenameReticlePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenameReticleRequest {"track_box", "threat_box", assetsRoot, false});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_EQ(ReadTextFile(radarFile), pageBefore);
    EXPECT_EQ(ReadTextFile(templateFile), templateBefore);
}

TEST(ReticleRenameServiceTests, ExecuteModifiesOnlyExpectedFiles)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path reticleFolder = assetsRoot / "reticles";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "current.json";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path supportFile = assetsRoot / "pages" / "support.json";
    const std::filesystem::path unrelatedFile = assetsRoot / "pages" / "unrelated.json";
    const std::filesystem::path templateFile = reticleFolder / "track_box.json";

    WritePageJson(radarFile, "Radar", {{"track", "track_box"}});
    WritePageJson(supportFile, "Support", {{"backup", "track_box"}});
    WritePageJson(unrelatedFile, "Unrelated", {{"other", "other_template"}});
    WriteTemplateJson(templateFile, "track_box");
    WriteWindowJson(windowFile, reticleFolder, {radarFile}, "Radar");

    mfd::PageDefinition radarPage = MakePage("Radar", true);
    AddStaticTemplateReference(radarPage, "track", "track_box");

    auto [loaded, files] = MakeLoadedWindow(windowFile, reticleFolder, {{radarFile, radarPage}});
    AddTemplate(loaded, files, templateFile, MakeTemplate("track_box"));

    std::unordered_map<std::string, std::string> before;
    for (const std::filesystem::path& file : {radarFile, supportFile, unrelatedFile, templateFile})
    {
        before.emplace(std::filesystem::absolute(file).lexically_normal().generic_string(), ReadTextFile(file));
    }

    editor::ReticleRenameService service;
    const editor::RenameReticlePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenameReticleRequest {"track_box", "threat_box", assetsRoot, false});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.filesToModify.size(), 3U);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;

    const std::vector<std::filesystem::path> changed =
        ChangedFiles(before, {radarFile, supportFile, unrelatedFile, templateFile});
    std::vector<std::filesystem::path> expected = plan.filesToModify;
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(changed, expected);
}

TEST(ReticleRenameServiceTests, IncludesExecPagesWhenScanningAssetRoot)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path reticleFolder = assetsRoot / "reticles";
    const std::filesystem::path execPageFile = assetsRoot / "_Exec" / "pages" / "staged.json";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "current.json";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path templateFile = reticleFolder / "track_box.json";

    WritePageJson(radarFile, "Radar", {{"track", "track_box"}});
    WritePageJson(execPageFile, "Staged", {{"staged_track", "track_box"}});
    WriteTemplateJson(templateFile, "track_box");
    WriteWindowJson(windowFile, reticleFolder, {radarFile}, "Radar");

    mfd::PageDefinition radarPage = MakePage("Radar", true);
    AddStaticTemplateReference(radarPage, "track", "track_box");

    auto [loaded, files] = MakeLoadedWindow(windowFile, reticleFolder, {{radarFile, radarPage}});
    AddTemplate(loaded, files, templateFile, MakeTemplate("track_box"));

    editor::ReticleRenameService service;
    const editor::RenameReticlePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenameReticleRequest {"track_box", "threat_box", assetsRoot, false});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.references.size(), 2U);
    ASSERT_EQ(plan.filesToModify.size(), 3U);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;
    EXPECT_EQ(json::parse(ReadTextFile(execPageFile)).at("staticReticles").front().at("template").get<std::string>(), "threat_box");
}

TEST(ReticleRenameServiceTests, AllowsRenameInsideExecAssetTree)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "_Exec" / "v142" / "Win32" / "Debug" / "assets";
    const std::filesystem::path reticleFolder = assetsRoot / "reticles";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "current.json";
    const std::filesystem::path radarFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path templateFile = reticleFolder / "track_box.json";

    WritePageJson(radarFile, "Radar", {{"track", "track_box"}});
    WriteTemplateJson(templateFile, "track_box");
    WriteWindowJson(windowFile, reticleFolder, {radarFile}, "Radar");

    mfd::PageDefinition radarPage = MakePage("Radar", true);
    AddStaticTemplateReference(radarPage, "track", "track_box");

    auto [loaded, files] = MakeLoadedWindow(windowFile, reticleFolder, {{radarFile, radarPage}});
    AddTemplate(loaded, files, templateFile, MakeTemplate("track_box"));

    editor::ReticleRenameService service;
    const editor::RenameReticlePlan plan = service.BuildPlan(
        loaded,
        files,
        editor::RenameReticleRequest {"track_box", "threat_box", assetsRoot, false});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.references.size(), 1U);

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;
    EXPECT_EQ(json::parse(ReadTextFile(radarFile)).at("staticReticles").front().at("template").get<std::string>(), "threat_box");
    EXPECT_EQ(json::parse(ReadTextFile(templateFile)).at("id").get<std::string>(), "threat_box");
}
