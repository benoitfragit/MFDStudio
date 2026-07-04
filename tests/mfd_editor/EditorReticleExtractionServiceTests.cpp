/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the page-reticle extraction service.
 */

#include "EditorReticleExtractionService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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
                std::filesystem::path("mfd_reticle_extraction_tests_" + std::to_string(ticks));
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

mfd::Primitive MakeRectanglePrimitive(const std::string& id,
                                     const mfd::Vec2 position,
                                     const float width,
                                     const float height,
                                     const mfd::ColorRgba color = {0, 255, 102, 255})
{
    mfd::Primitive primitive;
    primitive.id = id;
    primitive.type = mfd::PrimitiveType::Rectangle;
    primitive.transform.position = position;
    primitive.style.color = color;
    primitive.geometry = mfd::RectangleGeometry {width, height};
    return primitive;
}

mfd::Primitive MakeCirclePrimitive(const std::string& id, const mfd::Vec2 position, const float radius)
{
    mfd::Primitive primitive;
    primitive.id = id;
    primitive.type = mfd::PrimitiveType::Circle;
    primitive.transform.position = position;
    primitive.geometry = mfd::CircleGeometry {radius};
    return primitive;
}

mfd::Primitive MakeImagePrimitive(const std::string& id,
                                 const std::filesystem::path& file,
                                 const mfd::Vec2 position,
                                 const float width,
                                 const float height)
{
    mfd::Primitive primitive;
    primitive.id = id;
    primitive.type = mfd::PrimitiveType::Image;
    primitive.transform.position = position;
    primitive.geometry = mfd::ImageGeometry {file, width, height};
    return primitive;
}

mfd::ReticleGroup MakePageReticle(const std::string& id,
                                  const std::string& layerId,
                                  const bool drawOnTop,
                                  const mfd::Transform2D& transform,
                                  std::vector<mfd::Primitive> primitives)
{
    mfd::ReticleGroup reticle;
    reticle.id = id;
    reticle.visible = true;
    reticle.drawOnTop = drawOnTop;
    reticle.transform = transform;
    reticle.layerId = layerId;
    reticle.primitives = std::move(primitives);
    return reticle;
}

mfd::Transform2D CombineWorldTransform(const mfd::ReticleGroup& reticle, const mfd::Primitive& primitive)
{
    return mfd::CombineTransforms(reticle.transform, primitive.transform);
}

struct PrimitiveSignature
{
    std::string id {};
    mfd::PrimitiveType type = mfd::PrimitiveType::Line;
    mfd::Transform2D transform {};
    mfd::ColorRgba color {};
};

std::vector<PrimitiveSignature> FlattenWorldPrimitives(const std::vector<mfd::ReticleGroup>& reticles)
{
    std::vector<PrimitiveSignature> signatures;
    for (const mfd::ReticleGroup& reticle : reticles)
    {
        for (const mfd::Primitive& primitive : reticle.primitives)
        {
            const mfd::Transform2D worldTransform = CombineWorldTransform(reticle, primitive);
            signatures.push_back(PrimitiveSignature {primitive.id, primitive.type, worldTransform, primitive.style.color});
        }
    }
    return signatures;
}

void SortPrimitiveSignatures(std::vector<PrimitiveSignature>& signatures)
{
    std::sort(signatures.begin(),
              signatures.end(),
              [](const PrimitiveSignature& lhs, const PrimitiveSignature& rhs)
              {
                  return lhs.id < rhs.id;
              });
}

void WriteTextFile(const std::filesystem::path& path, const std::string_view content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(stream.is_open()) << path.string();
    stream << content;
}
} // namespace

TEST(ReticleExtractionServiceTests, BuildPlanExtractsTwoPageReticlesIntoOneTemplate)
{
    ScopedTempDir tempDir;
    mfd::LoadedWindowConfiguration loaded;
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"layer"});
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
    page.staticReticles.push_back(
        MakePageReticle("alpha",
                        "layer",
                        false,
                        mfd::Transform2D {{-0.4f, 0.0f}, 0.0f, {1.0f, 1.0f}},
                        {MakeRectanglePrimitive("box", {0.1f, 0.0f}, 0.2f, 0.1f)}));
    page.staticReticles.push_back(
        MakePageReticle("beta",
                        "layer",
                        false,
                        mfd::Transform2D {{0.5f, 0.1f}, 0.0f, {1.0f, 1.0f}},
                        {MakeCirclePrimitive("ring", {-0.05f, 0.03f}, 0.08f)}));
    loaded.document.pages.push_back(page);

    editor::EditorFileLayout files;
    editor::ReticleExtractionService service;
    const editor::ReticleExtractionPlan plan = service.BuildPlan(
        loaded,
        files,
        editor::ReticleExtractionRequest {0, {0, 1}, "combo", {}});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_EQ(plan.targetTemplateId, "combo");
    EXPECT_EQ(plan.extractedPrimitiveCount, 2U);
    ASSERT_EQ(plan.extractedTemplate.primitives.size(), 2U);
    EXPECT_EQ(plan.replacementInstance.sourceTemplateId, "combo");
    EXPECT_EQ(plan.sourceReticleIds, std::vector<std::string>({"alpha", "beta"}));
}

TEST(ReticleExtractionServiceTests, BuildPlanConvertsPrimitiveTransformsToNewLocalCoordinates)
{
    ScopedTempDir tempDir;
    mfd::LoadedWindowConfiguration loaded;
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"layer"});
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});

    mfd::ReticleGroup reticle = MakePageReticle(
        "alpha",
        "layer",
        false,
        mfd::Transform2D {{0.25f, -0.10f}, 15.0f, {1.5f, 0.5f}},
        {MakeRectanglePrimitive("box", {0.10f, 0.20f}, 0.2f, 0.1f)});
    reticle.overrides.color = mfd::ColorRgba {255, 128, 64, 200};
    page.staticReticles.push_back(reticle);
    loaded.document.pages.push_back(page);

    editor::EditorFileLayout files;
    editor::ReticleExtractionService service;
    const editor::ReticleExtractionPlan plan = service.BuildPlan(
        loaded,
        files,
        editor::ReticleExtractionRequest {0, {0}, "alpha_extract", {}});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    ASSERT_EQ(plan.extractedTemplate.primitives.size(), 1U);

    const mfd::Transform2D expectedWorld = mfd::CombineTransforms(reticle.transform, reticle.primitives.front().transform);
    const mfd::Primitive& flattened = plan.extractedTemplate.primitives.front();
    EXPECT_NEAR(flattened.transform.position.x, expectedWorld.position.x - plan.anchor.x, 0.0001f);
    EXPECT_NEAR(flattened.transform.position.y, expectedWorld.position.y - plan.anchor.y, 0.0001f);
    EXPECT_NEAR(flattened.transform.rotationDegrees, expectedWorld.rotationDegrees, 0.0001f);
    EXPECT_NEAR(flattened.transform.scale.x, expectedWorld.scale.x, 0.0001f);
    EXPECT_NEAR(flattened.transform.scale.y, expectedWorld.scale.y, 0.0001f);
    EXPECT_EQ(flattened.style.color.r, 255);
    EXPECT_EQ(flattened.style.color.g, 128);
    EXPECT_EQ(flattened.style.color.b, 64);
    EXPECT_EQ(flattened.style.color.a, 200);
}

TEST(ReticleExtractionServiceTests, ExecuteReplacesSelectionWhilePreservingPrimitiveWorldTransforms)
{
    ScopedTempDir tempDir;
    mfd::LoadedWindowConfiguration loaded;
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"layer"});
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
    page.staticReticles.push_back(
        MakePageReticle("alpha",
                        "layer",
                        false,
                        mfd::Transform2D {{-0.2f, 0.0f}, 0.0f, {1.0f, 1.0f}},
                        {MakeRectanglePrimitive("a", {0.0f, 0.0f}, 0.2f, 0.1f)}));
    page.staticReticles.push_back(
        MakePageReticle("beta",
                        "layer",
                        false,
                        mfd::Transform2D {{0.3f, 0.2f}, 20.0f, {1.0f, 1.0f}},
                        {MakeCirclePrimitive("b", {0.05f, -0.02f}, 0.07f)}));
    loaded.document.pages.push_back(page);

    const std::vector<PrimitiveSignature> before = FlattenWorldPrimitives(loaded.document.pages.front().staticReticles);

    editor::EditorFileLayout files;
    editor::ReticleExtractionService service;
    const editor::ReticleExtractionPlan plan = service.BuildPlan(
        loaded,
        files,
        editor::ReticleExtractionRequest {0, {0, 1}, "combo", {}});
    ASSERT_TRUE(plan.canExecute) << plan.error;

    editor::ReticleExtractionResult result;
    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, &result, &error)) << error;

    ASSERT_EQ(loaded.document.pages.front().staticReticles.size(), 1U);
    ASSERT_TRUE(loaded.document.reticleLibrary.find(result.templateId) != loaded.document.reticleLibrary.end());

    std::vector<PrimitiveSignature> after = FlattenWorldPrimitives(loaded.document.pages.front().staticReticles);
    SortPrimitiveSignatures(after);
    std::vector<PrimitiveSignature> sortedBefore = before;
    SortPrimitiveSignatures(sortedBefore);

    ASSERT_EQ(after.size(), sortedBefore.size());
    for (std::size_t index = 0; index < after.size(); ++index)
    {
        EXPECT_EQ(after[index].type, sortedBefore[index].type);
        EXPECT_NEAR(after[index].transform.position.x, sortedBefore[index].transform.position.x, 0.0001f);
        EXPECT_NEAR(after[index].transform.position.y, sortedBefore[index].transform.position.y, 0.0001f);
        EXPECT_NEAR(after[index].transform.rotationDegrees, sortedBefore[index].transform.rotationDegrees, 0.0001f);
        EXPECT_NEAR(after[index].transform.scale.x, sortedBefore[index].transform.scale.x, 0.0001f);
        EXPECT_NEAR(after[index].transform.scale.y, sortedBefore[index].transform.scale.y, 0.0001f);
    }
}

TEST(ReticleExtractionServiceTests, BuildPlanRenamesTemplateIdWhenLibraryCollisionExists)
{
    ScopedTempDir tempDir;
    mfd::LoadedWindowConfiguration loaded;
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";
    loaded.document.reticleLibrary["combo"] = mfd::ReticleGroup {"combo"};

    mfd::PageDefinition page;
    page.name = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"layer"});
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
    page.staticReticles.push_back(
        MakePageReticle("alpha",
                        "layer",
                        false,
                        mfd::Transform2D {},
                        {MakeRectanglePrimitive("box", {}, 0.2f, 0.1f)}));
    loaded.document.pages.push_back(page);

    editor::EditorFileLayout files;
    editor::ReticleExtractionService service;
    const editor::ReticleExtractionPlan plan = service.BuildPlan(
        loaded,
        files,
        editor::ReticleExtractionRequest {0, {0}, "combo", {}});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_NE(plan.targetTemplateId, "combo");
    EXPECT_TRUE(plan.templateIdAdjusted);
}

TEST(ReticleExtractionServiceTests, BuildPlanRenamesTemplateFileWhenPreferredPathIsTaken)
{
    ScopedTempDir tempDir;
    const std::filesystem::path libraryFolder = tempDir.Path() / "reticles";
    WriteTextFile(libraryFolder / "combo.json", "{}\n");

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.reticleLibraryFolder = libraryFolder;

    mfd::PageDefinition page;
    page.name = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"layer"});
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
    page.staticReticles.push_back(
        MakePageReticle("alpha",
                        "layer",
                        false,
                        mfd::Transform2D {},
                        {MakeRectanglePrimitive("box", {}, 0.2f, 0.1f)}));
    loaded.document.pages.push_back(page);

    editor::EditorFileLayout files;
    editor::ReticleExtractionService service;
    const editor::ReticleExtractionPlan plan = service.BuildPlan(
        loaded,
        files,
        editor::ReticleExtractionRequest {0, {0}, "combo", libraryFolder / "combo.json"});

    ASSERT_TRUE(plan.canExecute) << plan.error;
    EXPECT_TRUE(plan.templateFileAdjusted);
    EXPECT_NE(plan.targetTemplateFile.filename().string(), "combo.json");
}

TEST(ReticleExtractionServiceTests, BuildPlanRejectsEmptySelection)
{
    mfd::LoadedWindowConfiguration loaded;
    loaded.window.reticleLibraryFolder = "examples/demo/assets/reticles";
    loaded.document.pages.push_back(mfd::PageDefinition {});

    editor::EditorFileLayout files;
    editor::ReticleExtractionService service;
    const editor::ReticleExtractionPlan plan =
        service.BuildPlan(loaded, files, editor::ReticleExtractionRequest {0, {}, "combo", {}});

    EXPECT_FALSE(plan.canExecute);
    EXPECT_NE(plan.error.find("Select one or more page reticles"), std::string::npos);
}

TEST(ReticleExtractionServiceTests, BuildPlanRejectsUnsupportedClippingSelection)
{
    mfd::LoadedWindowConfiguration loaded;
    loaded.window.reticleLibraryFolder = "examples/demo/assets/reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"layer"});
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
    mfd::ReticleGroup reticle = MakePageReticle(
        "alpha",
        "layer",
        false,
        mfd::Transform2D {},
        {MakeRectanglePrimitive("box", {}, 0.2f, 0.1f)});
    reticle.clipping.mode = mfd::ReticleClipMode::Outer;
    reticle.clipping.primitiveId = "box";
    page.staticReticles.push_back(std::move(reticle));
    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout files;
    editor::ReticleExtractionService service;
    const editor::ReticleExtractionPlan plan =
        service.BuildPlan(loaded, files, editor::ReticleExtractionRequest {0, {0}, "combo", {}});

    EXPECT_FALSE(plan.canExecute);
    EXPECT_NE(plan.error.find("clipped"), std::string::npos);
}

TEST(ReticleExtractionServiceTests, BuildPlanRejectsNonContiguousSelection)
{
    mfd::LoadedWindowConfiguration loaded;
    loaded.window.reticleLibraryFolder = "examples/demo/assets/reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"layer"});
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
    page.staticReticles.push_back(
        MakePageReticle("alpha",
                        "layer",
                        false,
                        mfd::Transform2D {{-0.3f, 0.0f}, 0.0f, {1.0f, 1.0f}},
                        {MakeRectanglePrimitive("a", {}, 0.2f, 0.1f)}));
    page.staticReticles.push_back(
        MakePageReticle("beta",
                        "layer",
                        false,
                        mfd::Transform2D {{0.0f, 0.0f}, 0.0f, {1.0f, 1.0f}},
                        {MakeRectanglePrimitive("b", {}, 0.2f, 0.1f)}));
    page.staticReticles.push_back(
        MakePageReticle("gamma",
                        "layer",
                        false,
                        mfd::Transform2D {{0.3f, 0.0f}, 0.0f, {1.0f, 1.0f}},
                        {MakeRectanglePrimitive("c", {}, 0.2f, 0.1f)}));
    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout files;
    editor::ReticleExtractionService service;
    const editor::ReticleExtractionPlan plan =
        service.BuildPlan(loaded, files, editor::ReticleExtractionRequest {0, {0, 2}, "combo", {}});

    EXPECT_FALSE(plan.canExecute);
    EXPECT_NE(plan.error.find("contiguous"), std::string::npos);
}

TEST(ReticleExtractionServiceTests, BuildPlanRejectsImageDependencyOutsideAssetsRoot)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetsRoot = tempDir.Path() / "assets";
    const std::filesystem::path pageFile = assetsRoot / "pages" / "radar.json";
    const std::filesystem::path libraryFolder = assetsRoot / "reticles";
    const std::filesystem::path externalImage = tempDir.Path() / "external" / "badge.png";
    WriteTextFile(externalImage, "not-an-image-but-good-enough-for-a-path-check");

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = assetsRoot / "windows" / "radar_window.json";
    loaded.window.reticleLibraryFolder = libraryFolder;

    mfd::PageDefinition page;
    page.name = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"layer"});
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
    page.staticReticles.push_back(
        MakePageReticle("alpha",
                        "layer",
                        false,
                        mfd::Transform2D {},
                        {MakeImagePrimitive("badge", externalImage, {}, 0.2f, 0.1f)}));
    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout files;
    files.pageFiles.push_back(pageFile);

    editor::ReticleExtractionService service;
    const editor::ReticleExtractionPlan plan =
        service.BuildPlan(loaded, files, editor::ReticleExtractionRequest {0, {0}, "combo", {}});

    EXPECT_FALSE(plan.canExecute);
    EXPECT_NE(plan.error.find("image dependencies"), std::string::npos);
}

TEST(ReticleExtractionServiceTests, CallerSnapshotCanRestoreStateAfterExecute)
{
    mfd::LoadedWindowConfiguration loaded;
    loaded.window.reticleLibraryFolder = "examples/demo/assets/reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"layer"});
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
    page.staticReticles.push_back(
        MakePageReticle("alpha",
                        "layer",
                        false,
                        mfd::Transform2D {},
                        {MakeRectanglePrimitive("box", {}, 0.2f, 0.1f)}));
    loaded.document.pages.push_back(page);
    const mfd::LoadedWindowConfiguration before = loaded;

    editor::EditorFileLayout files;
    const editor::EditorFileLayout beforeFiles = files;
    editor::ReticleExtractionService service;
    const editor::ReticleExtractionPlan plan =
        service.BuildPlan(loaded, files, editor::ReticleExtractionRequest {0, {0}, "combo", {}});
    ASSERT_TRUE(plan.canExecute) << plan.error;

    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;
    ASSERT_EQ(loaded.document.pages.front().staticReticles.size(), 1U);
    ASSERT_TRUE(loaded.document.reticleLibrary.find(plan.targetTemplateId) != loaded.document.reticleLibrary.end());

    loaded = before;
    files = beforeFiles;
    ASSERT_EQ(loaded.document.pages.front().staticReticles.size(), 1U);
    EXPECT_TRUE(loaded.document.reticleLibrary.empty());
    EXPECT_TRUE(files.templateFiles.empty());
}
