/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the design-document export service.
 */

#include "EditorDesignExportService.h"

#include <algorithm>
#include <cctype>
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
                std::filesystem::path("mfd_design_export_tests_" + std::to_string(ticks));
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

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    EXPECT_TRUE(stream.is_open()) << path.string();
    return std::string {(std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>()};
}

bool IsPathWithinRoot(const std::filesystem::path& root, const std::filesystem::path& candidate)
{
    const std::filesystem::path normalizedRoot = std::filesystem::absolute(root).lexically_normal();
    const std::filesystem::path normalizedCandidate = std::filesystem::absolute(candidate).lexically_normal();
    auto rootIterator = normalizedRoot.begin();
    auto candidateIterator = normalizedCandidate.begin();
    for (; rootIterator != normalizedRoot.end() && candidateIterator != normalizedCandidate.end();
         ++rootIterator, ++candidateIterator)
    {
        std::string rootComponent = rootIterator->string();
        std::string candidateComponent = candidateIterator->string();
        std::transform(rootComponent.begin(),
                       rootComponent.end(),
                       rootComponent.begin(),
                       [](const unsigned char character)
                       {
                           return static_cast<char>(std::tolower(character));
                       });
        std::transform(candidateComponent.begin(),
                       candidateComponent.end(),
                       candidateComponent.begin(),
                       [](const unsigned char character)
                       {
                           return static_cast<char>(std::tolower(character));
                       });
        if (rootComponent != candidateComponent)
        {
            return false;
        }
    }

    return rootIterator == normalizedRoot.end();
}

mfd::Primitive MakeTextPrimitive(const std::string& id, const std::string& text, const bool exposed = false)
{
    mfd::Primitive primitive;
    primitive.id = id;
    primitive.type = mfd::PrimitiveType::Text;
    primitive.geometry = mfd::TextGeometry {text, 0.04f, 0.002f};
    primitive.exposed = exposed;
    return primitive;
}

mfd::ReticleGroup MakeReticle(const std::string& id,
                              const std::string& templateId,
                              const float x,
                              const float y,
                              const std::string& layerId)
{
    mfd::ReticleGroup reticle;
    reticle.id = id;
    reticle.sourceTemplateId = templateId;
    reticle.transform.position = {x, y};
    reticle.transform.rotationDegrees = 12.5f;
    reticle.transform.scale = {1.0f, 1.0f};
    reticle.visible = true;
    reticle.layerId = layerId;
    reticle.primitives.push_back(MakeTextPrimitive("label", "TRK", true));
    return reticle;
}

struct DesignExportFixture
{
    mfd::LoadedWindowConfiguration loaded;
    editor::EditorFileLayout files;
};

DesignExportFixture MakeFixture(const std::filesystem::path& root, const std::string& pageName)
{
    const std::filesystem::path assetsRoot = root / "assets";
    const std::filesystem::path windowFile = assetsRoot / "windows" / "demo_window.json";
    const std::filesystem::path pageFile = assetsRoot / "pages" / std::filesystem::path(pageName + ".json");
    const std::filesystem::path templateFile = assetsRoot / "reticles" / "track_box.json";

    WriteTextFile(windowFile, "{}");
    WriteTextFile(pageFile, "{}");
    WriteTextFile(templateFile, "{}");

    DesignExportFixture fixture;
    fixture.loaded.window.sourceFile = windowFile;
    fixture.loaded.window.title = "Demo Cockpit";
    fixture.loaded.window.reticleLibraryFolder = assetsRoot / "reticles";
    fixture.files.pageFiles.push_back(pageFile);
    fixture.files.templateFiles.emplace("track_box", templateFile);

    mfd::PageDefinition page;
    page.name = pageName;
    page.normalizedName = mfd::NormalizePageName(pageName);
    page.title = pageName;
    page.defaultPage = true;
    page.layers.push_back(mfd::PageLayerDefinition {"hud"});
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"hud", true});
    page.blinkTypes.push_back(mfd::PageBlinkDefinition {"warning", mfd::NormalizePageName("warning"), 750U});
    page.defaultBlinkTypeName = "warning";
    page.normalizedDefaultBlinkTypeName = mfd::NormalizePageName("warning");
    page.staticReticles.push_back(MakeReticle("track_box_01", "track_box", 0.250f, -0.400f, "hud"));

    mfd::PageStrobeDefinition strobe;
    strobe.name = "Default";
    strobe.normalizedName = mfd::NormalizePageName(strobe.name);
    strobe.reticle = MakeReticle("strobe_cursor", "strobe_cursor", 0.000f, 0.000f, "hud");
    page.strobes.push_back(strobe);
    page.activeStrobeName = strobe.name;
    page.normalizedActiveStrobeName = strobe.normalizedName;

    fixture.loaded.document.pages.push_back(std::move(page));
    fixture.loaded.document.reticleLibrary.emplace("track_box", MakeReticle("track_box", "track_box", 0.0f, 0.0f, ""));
    fixture.loaded.window.pageFiles = fixture.files.pageFiles;
    return fixture;
}

editor::DesignExportRequest MakeRequest(const std::filesystem::path& outputFolder,
                                        const DesignExportFixture& fixture)
{
    editor::DesignExportRequest request;
    request.outputFolder = outputFolder;
    request.windowFile = fixture.loaded.window.sourceFile;
    request.loaded = &fixture.loaded;
    request.files = &fixture.files;
    return request;
}
} // namespace

TEST(EditorDesignExportServiceTests, BuildPlanRejectsEmptyOutputFolder)
{
    ScopedTempDir tempDir;
    const DesignExportFixture fixture = MakeFixture(tempDir.Path(), "Radar");
    editor::EditorDesignExportService service;

    editor::DesignExportRequest request = MakeRequest({}, fixture);
    const editor::DesignExportPlan plan = service.BuildPlan(request);

    EXPECT_FALSE(plan.canExecute);
    EXPECT_FALSE(plan.error.empty());
}

TEST(EditorDesignExportServiceTests, BuildPlanListsExpectedFilesAndSanitizesPageNames)
{
    ScopedTempDir tempDir;
    const DesignExportFixture fixture = MakeFixture(tempDir.Path(), "Radar/Main:1?");
    editor::EditorDesignExportService service;

    editor::DesignExportRequest request = MakeRequest(tempDir.Path() / "export", fixture);
    const editor::DesignExportPlan plan = service.BuildPlan(request);

    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_EQ(plan.pages.size(), 1U);
    EXPECT_TRUE(std::filesystem::path(plan.pages.front().markdownFile).filename().string().find(':') == std::string::npos);
    EXPECT_TRUE(std::filesystem::path(plan.pages.front().markdownFile).filename().string().find('?') == std::string::npos);
    EXPECT_TRUE(std::find(plan.filesToWrite.begin(), plan.filesToWrite.end(), plan.readmeFile) != plan.filesToWrite.end());
    EXPECT_TRUE(std::find(plan.filesToWrite.begin(), plan.filesToWrite.end(), plan.windowIcdFile) != plan.filesToWrite.end());
    EXPECT_TRUE(std::find(plan.filesToWrite.begin(), plan.filesToWrite.end(), plan.manifestFile) != plan.filesToWrite.end());
}

TEST(EditorDesignExportServiceTests, ExecuteCreatesMarkdownFilesWithReticleCoordinatesStrobeAndBlinkSections)
{
    ScopedTempDir tempDir;
    const DesignExportFixture fixture = MakeFixture(tempDir.Path(), "Radar");
    editor::EditorDesignExportService service;

    editor::DesignExportRequest request = MakeRequest(tempDir.Path() / "export", fixture);
    request.exportExplodedViews = false;
    const editor::DesignExportPlan plan = service.BuildPlan(request);
    ASSERT_TRUE(plan.canExecute) << plan.error;

    const editor::DesignExportResult result = service.Execute(plan);

    EXPECT_TRUE(std::filesystem::exists(plan.readmeFile));
    EXPECT_TRUE(std::filesystem::exists(plan.windowIcdFile));
    EXPECT_TRUE(std::filesystem::exists(plan.pages.front().markdownFile));
    EXPECT_TRUE(std::filesystem::exists(plan.manifestFile));

    const std::string pageMarkdown = ReadTextFile(plan.pages.front().markdownFile);
    EXPECT_NE(pageMarkdown.find("track_box_01"), std::string::npos);
    EXPECT_NE(pageMarkdown.find("0.250"), std::string::npos);
    EXPECT_NE(pageMarkdown.find("-0.400"), std::string::npos);
    EXPECT_NE(pageMarkdown.find("## Strobe ICD"), std::string::npos);
    EXPECT_NE(pageMarkdown.find("## Blink ICD"), std::string::npos);
    EXPECT_NE(pageMarkdown.find("not available"), std::string::npos);

    const std::string readme = ReadTextFile(plan.readmeFile);
    EXPECT_NE(readme.find("[Radar](pages/"), std::string::npos);

    for (const auto& writtenFile : result.writtenFiles)
    {
        EXPECT_TRUE(IsPathWithinRoot(result.outputFolder, writtenFile)) << writtenFile.string();
    }
}

TEST(EditorDesignExportServiceTests, ExecuteUsesRelativeImageLinksWhenExplodedViewsSucceed)
{
    ScopedTempDir tempDir;
    const DesignExportFixture fixture = MakeFixture(tempDir.Path(), "Radar");
    editor::EditorDesignExportService service(
        [](const editor::DesignExportPlan&, const editor::DesignExportPageArtifact& artifact, std::string*) -> bool
        {
            std::filesystem::create_directories(artifact.imageFile.parent_path());
            std::ofstream stream(artifact.imageFile, std::ios::binary);
            stream << "fake_png";
            return stream.good();
        });

    editor::DesignExportRequest request = MakeRequest(tempDir.Path() / "export", fixture);
    request.exportExplodedViews = true;
    const editor::DesignExportPlan plan = service.BuildPlan(request);
    ASSERT_TRUE(plan.canExecute) << plan.error;

    const editor::DesignExportResult result = service.Execute(plan);

    EXPECT_TRUE(std::filesystem::exists(plan.pages.front().imageFile));
    const std::string pageMarkdown = ReadTextFile(plan.pages.front().markdownFile);
    EXPECT_NE(pageMarkdown.find("../images/"), std::string::npos);
    EXPECT_TRUE(std::find(result.writtenFiles.begin(), result.writtenFiles.end(), plan.pages.front().imageFile) !=
                result.writtenFiles.end());
}

TEST(EditorDesignExportServiceTests, ExecuteDocumentsEveryReticleOnThePage)
{
    ScopedTempDir tempDir;
    DesignExportFixture fixture = MakeFixture(tempDir.Path(), "Radar");
    fixture.loaded.document.pages.front().staticReticles.push_back(MakeReticle("track_box_02", "track_box", -0.450f, 0.120f, "hud"));
    fixture.loaded.document.pages.front().staticReticles.push_back(MakeReticle("track_box_03", "track_box", 0.600f, 0.450f, "hud"));

    editor::EditorDesignExportService service;
    editor::DesignExportRequest request = MakeRequest(tempDir.Path() / "export", fixture);
    request.exportExplodedViews = false;
    const editor::DesignExportPlan plan = service.BuildPlan(request);
    ASSERT_TRUE(plan.canExecute) << plan.error;

    const editor::DesignExportResult result = service.Execute(plan);
    EXPECT_FALSE(result.writtenFiles.empty());

    const std::string pageMarkdown = ReadTextFile(plan.pages.front().markdownFile);
    EXPECT_NE(pageMarkdown.find("track_box_01"), std::string::npos);
    EXPECT_NE(pageMarkdown.find("track_box_02"), std::string::npos);
    EXPECT_NE(pageMarkdown.find("track_box_03"), std::string::npos);
}

TEST(EditorDesignExportServiceTests, ExecuteKeepsMarkdownWhenExplodedViewRenderingFails)
{
    ScopedTempDir tempDir;
    const DesignExportFixture fixture = MakeFixture(tempDir.Path(), "Radar");
    editor::EditorDesignExportService service(
        [](const editor::DesignExportPlan&, const editor::DesignExportPageArtifact&, std::string* error) -> bool
        {
            if (error != nullptr)
            {
                *error = "simulated renderer failure";
            }
            return false;
        });

    editor::DesignExportRequest request = MakeRequest(tempDir.Path() / "export", fixture);
    request.exportExplodedViews = true;
    const editor::DesignExportPlan plan = service.BuildPlan(request);
    ASSERT_TRUE(plan.canExecute) << plan.error;

    const editor::DesignExportResult result = service.Execute(plan);

    EXPECT_TRUE(std::filesystem::exists(plan.pages.front().markdownFile));
    EXPECT_FALSE(std::filesystem::exists(plan.pages.front().imageFile));
    EXPECT_FALSE(result.warnings.empty());

    const std::string pageMarkdown = ReadTextFile(plan.pages.front().markdownFile);
    EXPECT_NE(pageMarkdown.find("Exploded design view: not available."), std::string::npos);
}
