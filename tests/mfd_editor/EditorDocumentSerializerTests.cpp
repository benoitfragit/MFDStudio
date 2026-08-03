/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests validating editor document serialization helpers.
 */

#include "EditorDocumentSerializer.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace
{
class ScopedTempDir
{
public:
    ScopedTempDir()
    {
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                std::filesystem::path("mfd_editor_tests_" + std::to_string(ticks));
        std::filesystem::create_directories(path_);
    }

    ~ScopedTempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};
} // namespace

TEST(EditorDocumentSerializerTests, DefaultPathsUseNormalizedPageName)
{
    const auto pagePath = editor::DefaultPageFilePath("/tmp/examples/demo/assets/windows/demo.json", "Page 01");
    const auto templatePath = editor::DefaultTemplateFilePath("/tmp/examples/demo/assets/reticles", "Radar Track");

    const auto expectedPage = std::filesystem::path("/tmp/examples/demo/assets/windows") /
                              std::filesystem::path(mfd::NormalizePageName("Page 01") + ".json");
    const auto expectedTemplate = std::filesystem::path("/tmp/examples/demo/assets/reticles") /
                                  std::filesystem::path(mfd::NormalizePageName("Radar Track") + ".json");

    EXPECT_EQ(pagePath.lexically_normal(), expectedPage.lexically_normal());
    EXPECT_EQ(templatePath.lexically_normal(), expectedTemplate.lexically_normal());
}

TEST(EditorDocumentSerializerTests, DiscoverReticleTemplateFilesLoadsValidJsonTemplates)
{
    ScopedTempDir tempDir;

    const auto validTemplate = tempDir.Path() / "track.json";
    const auto invalidTemplate = tempDir.Path() / "invalid.json";
    const auto nonJson = tempDir.Path() / "note.txt";

    {
        std::ofstream out(validTemplate);
        out << R"({"id":"radar_track","elements":[]})";
    }
    {
        std::ofstream out(invalidTemplate);
        out << R"({"name":"no_id"})";
    }
    {
        std::ofstream out(nonJson);
        out << "ignore";
    }

    editor::EditorFileLayout layout;
    std::string error;
    const bool ok = editor::DiscoverReticleTemplateFiles(tempDir.Path(), layout, &error);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(error.empty());
    ASSERT_EQ(layout.templateFiles.size(), 1U);
    ASSERT_TRUE(layout.templateFiles.find("radar_track") != layout.templateFiles.end());
    EXPECT_EQ(layout.templateFiles.at("radar_track"), validTemplate.lexically_normal());
}

TEST(EditorDocumentSerializerTests, DiscoverReticleTemplateFilesFailsWhenFolderIsMissing)
{
    editor::EditorFileLayout layout;
    std::string error;

    const bool ok = editor::DiscoverReticleTemplateFiles("/path/that/does/not/exist", layout, &error);

    ASSERT_FALSE(ok);
    ASSERT_FALSE(error.empty());
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentFailsWhenPageLayoutDoesNotMatchDocument)
{
    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = "window.json";
    loaded.window.reticleLibraryFolder = "reticles";
    loaded.document.pages.push_back(mfd::PageDefinition {});

    editor::EditorFileLayout layout;
    std::string error;

    const bool ok = editor::SaveEditorDocument(loaded, layout, &error);

    ASSERT_FALSE(ok);
    EXPECT_NE(error.find("Page file layout does not match"), std::string::npos);
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentFailsWhenPageHasNoRuntimeLayers)
{
    ScopedTempDir tempDir;

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = tempDir.Path() / "window.json";
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";
    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(tempDir.Path() / "radar.json");

    std::string error;
    const bool ok = editor::SaveEditorDocument(loaded, layout, &error);

    ASSERT_FALSE(ok);
    EXPECT_NE(error.find("non-empty layers array"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(loaded.window.sourceFile));
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentFailsWhenStaticReticleHasNoLayerId)
{
    ScopedTempDir tempDir;

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = tempDir.Path() / "window.json";
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});

    mfd::ReticleGroup reticle;
    reticle.id = "track_alpha";
    page.staticReticles.push_back(std::move(reticle));

    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(tempDir.Path() / "radar.json");

    std::string error;
    const bool ok = editor::SaveEditorDocument(loaded, layout, &error);

    ASSERT_FALSE(ok);
    EXPECT_NE(error.find("Static reticle 'track_alpha' must define layerId"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(loaded.window.sourceFile));
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentFailsWhenDynamicBindingTemplateIdIsMissing)
{
    ScopedTempDir tempDir;

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = tempDir.Path() / "window.json";
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    page.dynamicReticleBindings.push_back(mfd::DynamicReticleLayerBinding {"", "default", 0});
    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(tempDir.Path() / "radar.json");

    std::string error;
    const bool ok = editor::SaveEditorDocument(loaded, layout, &error);

    ASSERT_FALSE(ok);
    EXPECT_NE(error.find("non-empty templateId"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(loaded.window.sourceFile));
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentFailsWhenDynamicBindingLayerIdIsMissing)
{
    ScopedTempDir tempDir;

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = tempDir.Path() / "window.json";
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    loaded.document.reticleLibrary.emplace("track_template", mfd::ReticleGroup {"track_template"});
    page.dynamicReticleBindings.push_back(mfd::DynamicReticleLayerBinding {"track_template", "", 0});
    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(tempDir.Path() / "radar.json");

    std::string error;
    const bool ok = editor::SaveEditorDocument(loaded, layout, &error);

    ASSERT_FALSE(ok);
    EXPECT_NE(error.find("non-empty layerId"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(loaded.window.sourceFile));
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentFailsWhenDynamicBindingReferencesUnknownLayer)
{
    ScopedTempDir tempDir;

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = tempDir.Path() / "window.json";
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    loaded.document.reticleLibrary.emplace("track_template", mfd::ReticleGroup {"track_template"});
    page.dynamicReticleBindings.push_back(mfd::DynamicReticleLayerBinding {"track_template", "overlay", 0});
    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(tempDir.Path() / "radar.json");

    std::string error;
    const bool ok = editor::SaveEditorDocument(loaded, layout, &error);

    ASSERT_FALSE(ok);
    EXPECT_NE(error.find("unknown runtime layer 'overlay'"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(loaded.window.sourceFile));
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentFailsWhenPagesShareSameNormalizedName)
{
    ScopedTempDir tempDir;

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = tempDir.Path() / "window.json";
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition pageA;
    pageA.name = "Page1";
    pageA.title = "Page 1";
    pageA.layers.push_back(mfd::PageLayerDefinition {"default"});
    loaded.document.pages.push_back(pageA);

    mfd::PageDefinition pageB;
    pageB.name = "page1";
    pageB.title = "Page 1 copy";
    pageB.layers.push_back(mfd::PageLayerDefinition {"default"});
    loaded.document.pages.push_back(pageB);

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(tempDir.Path() / "page1.json");
    layout.pageFiles.push_back(tempDir.Path() / "page1_copy.json");

    std::string error;
    const bool ok = editor::SaveEditorDocument(loaded, layout, &error);

    ASSERT_FALSE(ok);
    EXPECT_NE(error.find("Duplicate page name"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(loaded.window.sourceFile));
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentFailsWhenDynamicBindingTemplateIsNotLoaded)
{
    ScopedTempDir tempDir;

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = tempDir.Path() / "window.json";
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    page.dynamicReticleBindings.push_back(mfd::DynamicReticleLayerBinding {"track_template", "default", 0});
    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(tempDir.Path() / "radar.json");

    std::string error;
    const bool ok = editor::SaveEditorDocument(loaded, layout, &error);

    ASSERT_FALSE(ok);
    EXPECT_NE(error.find("references one unloaded template"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(loaded.window.sourceFile));
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentFailsWhenPageStrobeTemplateIsNotLoaded)
{
    ScopedTempDir tempDir;

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = tempDir.Path() / "window.json";
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});

    mfd::PageStrobeDefinition strobe;
    strobe.name = "Primary";
    strobe.normalizedName = mfd::NormalizePageName(strobe.name);
    strobe.reticle.id = "primary_strobe";
    strobe.reticle.sourceTemplateId = "missing_template";
    page.strobes.push_back(std::move(strobe));

    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(tempDir.Path() / "radar.json");

    std::string error;
    const bool ok = editor::SaveEditorDocument(loaded, layout, &error);

    ASSERT_FALSE(ok);
    EXPECT_NE(error.find("references unknown template"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(loaded.window.sourceFile));
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentFailsWhenPageStrobeClippingIsInvalid)
{
    ScopedTempDir tempDir;

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = tempDir.Path() / "window.json";
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::ReticleGroup strobeTemplate;
    strobeTemplate.id = "strobe_template";
    loaded.document.reticleLibrary.emplace(strobeTemplate.id, strobeTemplate);

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});

    mfd::PageStrobeDefinition strobe;
    strobe.name = "Primary";
    strobe.normalizedName = mfd::NormalizePageName(strobe.name);
    strobe.reticle.id = "primary_strobe";
    strobe.reticle.sourceTemplateId = "strobe_template";
    strobe.reticle.clipping.mode = mfd::ReticleClipMode::Inner;
    strobe.reticle.clipping.primitiveId = "missing_mask";
    page.strobes.push_back(std::move(strobe));

    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(tempDir.Path() / "radar.json");

    std::string error;
    const bool ok = editor::SaveEditorDocument(loaded, layout, &error);

    ASSERT_FALSE(ok);
    EXPECT_NE(error.find("invalid clipping primitive"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(loaded.window.sourceFile));
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentFailsWhenPageReticleBlinkTypeIsUnknown)
{
    ScopedTempDir tempDir;

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = tempDir.Path() / "window.json";
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});

    mfd::ReticleGroup reticle;
    reticle.id = "track_alpha";
    reticle.layerId = "default";
    reticle.blink.enabled = true;
    reticle.blink.typeName = "missing_blink";
    reticle.blink.normalizedTypeName = mfd::NormalizePageName(reticle.blink.typeName);
    page.staticReticles.push_back(std::move(reticle));

    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(tempDir.Path() / "radar.json");

    std::string error;
    const bool ok = editor::SaveEditorDocument(loaded, layout, &error);

    ASSERT_FALSE(ok);
    EXPECT_NE(error.find("unknown blink type"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(loaded.window.sourceFile));
}

mfd::ReticleGroup MakeClippingReticle(const bool eraseLayerOnly)
{
    mfd::ReticleGroup reticle;
    reticle.id = "masker";

    mfd::Primitive shape;
    shape.id = "shape";
    shape.type = mfd::PrimitiveType::Circle;
    shape.geometry = mfd::CircleGeometry {0.1f};
    reticle.primitives.push_back(std::move(shape));

    reticle.clipping.mode = mfd::ReticleClipMode::Inner;
    reticle.clipping.primitiveId = "shape";
    reticle.clipping.eraseLayerOnly = eraseLayerOnly;
    return reticle;
}

TEST(EditorDocumentSerializerTests, SerializeReticleWritesClipEraseLayerOnlyOnlyWhenEnabled)
{
    const nlohmann::json disabledNode =
        nlohmann::json::parse(editor::SerializeReticleTemplateToJsonString(MakeClippingReticle(false)));
    ASSERT_TRUE(disabledNode.contains("clipping"));
    // A disabled policy keeps a compact clipping object without the optional flag.
    EXPECT_FALSE(disabledNode.at("clipping").contains("eraseLayerOnly"));

    const nlohmann::json enabledNode =
        nlohmann::json::parse(editor::SerializeReticleTemplateToJsonString(MakeClippingReticle(true)));
    ASSERT_TRUE(enabledNode.contains("clipping"));
    ASSERT_TRUE(enabledNode.at("clipping").contains("eraseLayerOnly"));
    EXPECT_TRUE(enabledNode.at("clipping").at("eraseLayerOnly").get<bool>());
}

TEST(EditorDocumentSerializerTests, SerializeReticleTemplateIncludesTemplateId)
{
    mfd::ReticleGroup reticle;
    reticle.id = "demo_track";

    const std::string jsonText = editor::SerializeReticleTemplateToJsonString(reticle);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    ASSERT_TRUE(jsonNode.contains("id"));
    EXPECT_EQ(jsonNode.at("id").get<std::string>(), "demo_track");
}

TEST(EditorDocumentSerializerTests, SerializeReticleTemplatePersistsPrimitiveReticleSensitivityFlags)
{
    mfd::ReticleGroup reticle;
    reticle.id = "demo_track";

    mfd::Primitive primitive;
    primitive.id = "caption";
    primitive.type = mfd::PrimitiveType::Text;
    primitive.geometry = mfd::TextGeometry {};
    primitive.reticleRotationSensitive = false;
    primitive.reticleScaleSensitive = false;
    reticle.primitives.push_back(std::move(primitive));

    const std::string jsonText = editor::SerializeReticleTemplateToJsonString(reticle);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    ASSERT_TRUE(jsonNode.contains("elements"));
    ASSERT_EQ(jsonNode.at("elements").size(), 1U);
    const auto& element = jsonNode.at("elements").front();
    ASSERT_TRUE(element.contains("reticleRotationSensitive"));
    ASSERT_TRUE(element.contains("reticleScaleSensitive"));
    EXPECT_FALSE(element.at("reticleRotationSensitive").get<bool>());
    EXPECT_FALSE(element.at("reticleScaleSensitive").get<bool>());
}

TEST(EditorDocumentSerializerTests, SerializeReticleTemplateOmitsDefaultCenteredTextAlignment)
{
    mfd::ReticleGroup reticle;
    reticle.id = "text_defaults";

    mfd::Primitive primitive;
    primitive.id = "caption";
    primitive.type = mfd::PrimitiveType::Text;
    primitive.geometry = mfd::TextGeometry {"UTC", 0.04f, 0.002f, mfd::Align::Center};
    reticle.primitives.push_back(std::move(primitive));

    const std::string jsonText = editor::SerializeReticleTemplateToJsonString(reticle);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    ASSERT_EQ(jsonNode.at("elements").size(), 1U);
    EXPECT_FALSE(jsonNode.at("elements").at(0).contains("align"));
}

TEST(EditorDocumentSerializerTests, SerializeReticleTemplateWritesExplicitTextAlignment)
{
    mfd::ReticleGroup reticle;
    reticle.id = "text_alignment";

    mfd::Primitive text;
    text.id = "left_caption";
    text.type = mfd::PrimitiveType::Text;
    text.geometry = mfd::TextGeometry {"UTC", 0.04f, 0.002f, mfd::Align::Left};
    reticle.primitives.push_back(text);

    mfd::Primitive time;
    time.id = "right_clock";
    time.type = mfd::PrimitiveType::Time;
    time.geometry = mfd::TimeGeometry {"%H:%M:%S", true, 0.04f, 0.002f, mfd::Align::Right};
    reticle.primitives.push_back(time);

    const std::string jsonText = editor::SerializeReticleTemplateToJsonString(reticle);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    ASSERT_EQ(jsonNode.at("elements").size(), 2U);
    EXPECT_EQ(jsonNode.at("elements").at(0).at("align").get<std::string>(), "left");
    EXPECT_EQ(jsonNode.at("elements").at(1).at("align").get<std::string>(), "right");
}

TEST(EditorDocumentSerializerTests, SerializeReticleTemplateWritesRingAndArcGeometry)
{
    mfd::ReticleGroup reticle;
    reticle.id = "geometry_demo";

    mfd::Primitive ring;
    ring.id = "scope_ring";
    ring.type = mfd::PrimitiveType::Ring;
    ring.style.filled = true;
    ring.style.lineStyle = mfd::LineStyle::Dotted;
    ring.style.fillColor = mfd::ColorRgba {10, 20, 30, 40};
    ring.geometry = mfd::RingGeometry {0.12f, 0.20f, 48};
    reticle.primitives.push_back(ring);

    mfd::Primitive arc;
    arc.id = "scan_arc";
    arc.type = mfd::PrimitiveType::Arc;
    arc.style.color = mfd::ColorRgba {1, 2, 3, 255};
    arc.style.lineStyle = mfd::LineStyle::Dashed;
    arc.geometry = mfd::ArcGeometry {0.25f, -60.0f, 120.0f, 36};
    reticle.primitives.push_back(arc);

    const std::string jsonText = editor::SerializeReticleTemplateToJsonString(reticle);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    ASSERT_EQ(jsonNode.at("elements").size(), 2U);

    const auto& ringNode = jsonNode.at("elements").at(0);
    EXPECT_EQ(ringNode.at("type").get<std::string>(), "ring");
    EXPECT_FLOAT_EQ(ringNode.at("innerRadius").get<float>(), 0.12f);
    EXPECT_FLOAT_EQ(ringNode.at("outerRadius").get<float>(), 0.20f);
    EXPECT_EQ(ringNode.at("segments").get<int>(), 48);
    EXPECT_TRUE(ringNode.at("filled").get<bool>());
    EXPECT_EQ(ringNode.at("fill").get<std::string>(), "#0A141E28");
    EXPECT_EQ(ringNode.at("lineStyle").get<std::string>(), "dotted");

    const auto& arcNode = jsonNode.at("elements").at(1);
    EXPECT_EQ(arcNode.at("type").get<std::string>(), "arc");
    EXPECT_FLOAT_EQ(arcNode.at("radius").get<float>(), 0.25f);
    EXPECT_FLOAT_EQ(arcNode.at("startAngleDegrees").get<float>(), -60.0f);
    EXPECT_FLOAT_EQ(arcNode.at("endAngleDegrees").get<float>(), 120.0f);
    EXPECT_EQ(arcNode.at("segments").get<int>(), 36);
    EXPECT_EQ(arcNode.at("stroke").get<std::string>(), "#010203FF");
    EXPECT_EQ(arcNode.at("lineStyle").get<std::string>(), "dashed");
}

TEST(EditorDocumentSerializerTests, SerializeReticleTemplateWritesExplicitFalseFilledFlagForFillCapablePrimitive)
{
    mfd::ReticleGroup reticle;
    reticle.id = "rectangle_demo";

    mfd::Primitive rectangle;
    rectangle.id = "box";
    rectangle.type = mfd::PrimitiveType::Rectangle;
    rectangle.style.fillColor = mfd::ColorRgba {0, 0, 0, 255};
    rectangle.style.filled = false;
    rectangle.geometry = mfd::RectangleGeometry {0.24f, 0.14f};
    reticle.primitives.push_back(std::move(rectangle));

    const std::string jsonText = editor::SerializeReticleTemplateToJsonString(reticle);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    ASSERT_EQ(jsonNode.at("elements").size(), 1U);
    const auto& rectangleNode = jsonNode.at("elements").front();
    ASSERT_TRUE(rectangleNode.contains("filled"));
    EXPECT_FALSE(rectangleNode.at("filled").get<bool>());
    EXPECT_EQ(rectangleNode.at("fill").get<std::string>(), "#000000FF");
}

TEST(EditorDocumentSerializerTests, SerializeReticleTemplateWritesUniformSquareAsSingleSize)
{
    mfd::ReticleGroup reticle;
    reticle.id = "square_demo";

    mfd::Primitive square;
    square.id = "marker";
    square.type = mfd::PrimitiveType::Square;
    square.geometry = mfd::SquareGeometry {0.18f, 0.18f};
    reticle.primitives.push_back(std::move(square));

    const std::string jsonText = editor::SerializeReticleTemplateToJsonString(reticle);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    ASSERT_EQ(jsonNode.at("elements").size(), 1U);
    const auto& squareNode = jsonNode.at("elements").front();
    // A square is uniform, so it always round-trips through a single side length
    // and never emits separate width/height keys that could deform it.
    ASSERT_TRUE(squareNode.contains("size"));
    EXPECT_FLOAT_EQ(squareNode.at("size").get<float>(), 0.18f);
    EXPECT_FALSE(squareNode.contains("width"));
    EXPECT_FALSE(squareNode.contains("height"));
}

TEST(EditorDocumentSerializerTests, SerializeReticleTemplateOmitsFilledFlagForNonFillCapablePrimitive)
{
    mfd::ReticleGroup reticle;
    reticle.id = "line_demo";

    mfd::Primitive line;
    line.id = "guide";
    line.type = mfd::PrimitiveType::Line;
    line.style.fillColor = mfd::ColorRgba {255, 0, 0, 255};
    line.style.filled = true;
    line.geometry = mfd::LineGeometry {};
    reticle.primitives.push_back(std::move(line));

    const std::string jsonText = editor::SerializeReticleTemplateToJsonString(reticle);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    ASSERT_EQ(jsonNode.at("elements").size(), 1U);
    const auto& lineNode = jsonNode.at("elements").front();
    EXPECT_FALSE(lineNode.contains("fill"));
    EXPECT_FALSE(lineNode.contains("filled"));
}

TEST(EditorDocumentSerializerTests, SerializeReticleTemplateWritesReticleFillOverrides)
{
    mfd::ReticleGroup reticle;
    reticle.id = "adi_background";
    reticle.overrides.color = mfd::ColorRgba {40, 50, 60, 255};
    reticle.overrides.fillColor = mfd::ColorRgba {70, 80, 90, 200};
    reticle.overrides.filled = true;

    mfd::Primitive rectangle;
    rectangle.id = "background";
    rectangle.type = mfd::PrimitiveType::Rectangle;
    rectangle.geometry = mfd::RectangleGeometry {0.80f, 0.40f};
    reticle.primitives.push_back(std::move(rectangle));

    const std::string jsonText = editor::SerializeReticleTemplateToJsonString(reticle);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    ASSERT_TRUE(jsonNode.contains("stroke"));
    ASSERT_TRUE(jsonNode.contains("fill"));
    ASSERT_TRUE(jsonNode.contains("filled"));
    EXPECT_EQ(jsonNode.at("stroke").get<std::string>(), "#28323CFF");
    EXPECT_EQ(jsonNode.at("fill").get<std::string>(), "#46505AC8");
    EXPECT_TRUE(jsonNode.at("filled").get<bool>());
}

TEST(EditorDocumentSerializerTests, SerializeReticleTemplateWritesImageGeometryAndDrawOnTop)
{
    mfd::ReticleGroup reticle;
    reticle.id = "image_demo";
    reticle.drawOnTop = true;

    mfd::Primitive primitive;
    primitive.id = "badge";
    primitive.type = mfd::PrimitiveType::Image;
    primitive.geometry = mfd::ImageGeometry {std::filesystem::path("/tmp/examples/demo/assets/picture/badge.png"), 0.28f, 0.16f};
    reticle.primitives.push_back(std::move(primitive));

    const std::string jsonText =
        editor::SerializeReticleTemplateToJsonString(reticle, std::filesystem::path("/tmp/examples/demo/assets/reticles"));
    const auto jsonNode = nlohmann::json::parse(jsonText);

    EXPECT_TRUE(jsonNode.at("drawOnTop").get<bool>());
    ASSERT_EQ(jsonNode.at("elements").size(), 1U);
    const auto& imageNode = jsonNode.at("elements").at(0);
    EXPECT_EQ(imageNode.at("type").get<std::string>(), "image");
    EXPECT_EQ(imageNode.at("file").get<std::string>(), "../picture/badge.png");
    EXPECT_FLOAT_EQ(imageNode.at("width").get<float>(), 0.28f);
    EXPECT_FLOAT_EQ(imageNode.at("height").get<float>(), 0.16f);
}

TEST(EditorDocumentSerializerTests, SerializeReticleTemplateWritesExposedPrimitiveFlag)
{
    mfd::ReticleGroup reticle;
    reticle.id = "progress_demo";

    mfd::Primitive primitive;
    primitive.id = "fill_bar";
    primitive.type = mfd::PrimitiveType::Rectangle;
    primitive.exposed = true;
    primitive.geometry = mfd::RectangleGeometry {0.20f, 0.06f};
    reticle.primitives.push_back(std::move(primitive));

    const std::string jsonText = editor::SerializeReticleTemplateToJsonString(reticle);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    ASSERT_EQ(jsonNode.at("elements").size(), 1U);
    EXPECT_TRUE(jsonNode.at("elements").at(0).at("exposed").get<bool>());
}

TEST(EditorDocumentSerializerTests, SerializePageReticleIncludesTemplateOverridesBlinkAndClipReset)
{
    mfd::ReticleGroup templateReticle;
    templateReticle.id = "radar_track";
    templateReticle.clipping.mode = mfd::ReticleClipMode::Inner;
    templateReticle.clipping.primitiveId = "mask";

    mfd::ReticleLibrary library;
    library.emplace(templateReticle.id, templateReticle);

    mfd::ReticleGroup reticle;
    reticle.id = "track_alpha";
    reticle.sourceTemplateId = "radar_track";
    reticle.info.label = "Alpha";
    reticle.info.category = "friendly";
    reticle.visible = false;
    reticle.transform.position = {0.25f, -0.1f};
    reticle.overrides.color = mfd::ColorRgba {1, 2, 3, 255};
    reticle.overrides.thickness = 0.006f;
    reticle.layerId = "tracks";
    reticle.blink.enabled = false;
    reticle.blink.typeName = "slow";
    reticle.clipping.mode = mfd::ReticleClipMode::None;

    mfd::Primitive text;
    text.id = "track_label";
    text.type = mfd::PrimitiveType::Text;
    text.geometry = mfd::TextGeometry {"T42", 0.04f, 0.01f};
    reticle.primitives.push_back(std::move(text));

    const std::string jsonText = editor::SerializePageReticleToJsonString(reticle, library);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    EXPECT_EQ(jsonNode.at("id").get<std::string>(), "track_alpha");
    EXPECT_EQ(jsonNode.at("template").get<std::string>(), "radar_track");
    EXPECT_EQ(jsonNode.at("label").get<std::string>(), "Alpha");
    EXPECT_EQ(jsonNode.at("category").get<std::string>(), "friendly");
    EXPECT_FALSE(jsonNode.at("visible").get<bool>());
    EXPECT_EQ(jsonNode.at("at"), nlohmann::json::array({0.25f, -0.1f}));
    EXPECT_EQ(jsonNode.at("stroke").get<std::string>(), "#010203FF");
    EXPECT_FLOAT_EQ(jsonNode.at("lineWidth").get<float>(), 0.006f);
    EXPECT_EQ(jsonNode.at("layerId").get<std::string>(), "tracks");
    EXPECT_FALSE(jsonNode.at("blink").at("enabled").get<bool>());
    EXPECT_EQ(jsonNode.at("blink").at("type").get<std::string>(), "slow");
    EXPECT_EQ(jsonNode.at("clipping").at("mode").get<std::string>(), "none");
    EXPECT_EQ(jsonNode.at("texts").at("track_label").get<std::string>(), "T42");
    EXPECT_FLOAT_EQ(jsonNode.at("letterSpacings").at("track_label").get<float>(), 0.01f);
}

TEST(EditorDocumentSerializerTests, TemplateInstanceTransformDoesNotDriftAcrossTenLoadSaveCycles)
{
    ScopedTempDir tempDir;
    const std::filesystem::path windowFile = tempDir.Path() / "window.json";
    const std::filesystem::path pageFile = tempDir.Path() / "page.json";
    const std::filesystem::path templateFolder = tempDir.Path() / "reticles";
    const std::filesystem::path templateFile = templateFolder / "marker.json";
    std::filesystem::create_directories(templateFolder);

    {
        std::ofstream stream(templateFile);
        stream << R"json({
  "id": "marker",
  "at": [0.1, -0.2],
  "angle": 15.0,
  "scale": [2.0, 0.5],
  "elements": []
})json";
    }
    {
        std::ofstream stream(pageFile);
        stream << R"json({
  "name": "Main",
  "title": "Main",
  "layers": [{"id": "default"}],
  "staticReticles": [{
    "id": "instance",
    "template": "marker",
    "layerId": "default",
    "at": [0.2, -0.3],
    "angle": 20.0,
    "scale": [0.5, 3.0]
  }]
})json";
    }
    {
        std::ofstream stream(windowFile);
        stream << R"json({
  "title": "Test",
  "size": [640, 480],
  "reticleLibraryFolder": "reticles",
  "defaultPage": "Main",
  "pages": ["page.json"]
})json";
    }

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(pageFile);
    layout.templateFiles.emplace("marker", templateFile);
    mfd::JsonLoader loader;
    mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(windowFile);
    const mfd::Transform2D expectedWorld = loaded.document.pages.front().staticReticles.front().transform;

    for (int cycle = 0; cycle < 10; ++cycle)
    {
        const std::string pageJson = editor::SerializePageToJsonString(loaded.document.pages.front(),
                                                                       loaded.document.reticleLibrary,
                                                                       layout,
                                                                       0U);
        std::ofstream stream(pageFile);
        ASSERT_TRUE(stream.is_open()) << "cycle " << cycle;
        stream << pageJson;
        stream.close();
        loaded = loader.LoadWindowConfiguration(windowFile);

        const mfd::Transform2D& world = loaded.document.pages.front().staticReticles.front().transform;
        EXPECT_NEAR(world.position.x, expectedWorld.position.x, 0.00001f);
        EXPECT_NEAR(world.position.y, expectedWorld.position.y, 0.00001f);
        EXPECT_NEAR(world.rotationDegrees, expectedWorld.rotationDegrees, 0.00001f);
        EXPECT_NEAR(world.scale.x, expectedWorld.scale.x, 0.00001f);
        EXPECT_NEAR(world.scale.y, expectedWorld.scale.y, 0.00001f);
    }

    std::ifstream pageStream(pageFile);
    const nlohmann::json pageJson = nlohmann::json::parse(pageStream);
    const nlohmann::json& transform = pageJson.at("staticReticles").front();
    EXPECT_NEAR(transform.at("at").at(0).get<float>(), 0.2f, 0.00001f);
    EXPECT_NEAR(transform.at("at").at(1).get<float>(), -0.3f, 0.00001f);
    EXPECT_NEAR(transform.at("angle").get<float>(), 20.0f, 0.00001f);
    EXPECT_NEAR(transform.at("scale").at(0).get<float>(), 0.5f, 0.00001f);
    EXPECT_NEAR(transform.at("scale").at(1).get<float>(), 3.0f, 0.00001f);
}

TEST(EditorDocumentSerializerTests, TemplateInstancePreservesAuthoredTransformWithNonInvertibleTemplateScale)
{
    mfd::ReticleGroup reticleTemplate;
    reticleTemplate.id = "marker";
    reticleTemplate.transform.scale = {0.0f, 2.0f};

    mfd::Transform2D authored;
    authored.position = {0.25f, -0.4f};
    authored.rotationDegrees = 12.0f;
    authored.scale = {3.0f, 0.5f};
    mfd::ReticleGroup instance = mfd::InstantiateReticle(reticleTemplate, "instance", authored);
    instance.layerId = "default";

    mfd::ReticleLibrary library;
    library.emplace(reticleTemplate.id, reticleTemplate);
    const nlohmann::json node =
        nlohmann::json::parse(editor::SerializePageReticleToJsonString(instance, library));

    EXPECT_NEAR(node.at("at").at(0).get<float>(), authored.position.x, 0.00001f);
    EXPECT_NEAR(node.at("at").at(1).get<float>(), authored.position.y, 0.00001f);
    EXPECT_NEAR(node.at("angle").get<float>(), authored.rotationDegrees, 0.00001f);
    EXPECT_NEAR(node.at("scale").at(0).get<float>(), authored.scale.x, 0.00001f);
    EXPECT_NEAR(node.at("scale").at(1).get<float>(), authored.scale.y, 0.00001f);
}

TEST(EditorDocumentSerializerTests, SerializeWindowFallsBackToAbsolutePathsWhenRelativePathsCannotBeComputed)
{
#ifndef _WIN32
    GTEST_SKIP() << "This regression targets Windows path roots";
#else
    mfd::WindowAssetDefinition window;
    window.sourceFile = std::filesystem::path("C:/workspace/window.json");
    window.title = "Demo";
    window.fontFile = std::filesystem::path("D:/assets/fonts/hud.ttf");
    window.reticleLibraryFolder = std::filesystem::path("D:/examples/demo/assets/reticles");

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";
    page.defaultPage = true;

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(std::filesystem::path("D:/examples/demo/assets/pages/radar.json"));

    const auto jsonNode = nlohmann::json::parse(editor::SerializeWindowToJsonString(window, document, layout));

    EXPECT_EQ(jsonNode.at("fontFile").get<std::string>(), "D:/assets/fonts/hud.ttf");
    EXPECT_EQ(jsonNode.at("reticleLibraryFolder").get<std::string>(), "D:/examples/demo/assets/reticles");
    ASSERT_EQ(jsonNode.at("pages").size(), 1U);
    EXPECT_EQ(jsonNode.at("pages").at(0).get<std::string>(), "D:/examples/demo/assets/pages/radar.json");
#endif
}

TEST(EditorDocumentSerializerTests, SerializeWindowWritesFeedbackCadenceFields)
{
    mfd::WindowAssetDefinition window;
    window.sourceFile = std::filesystem::path("C:/workspace/window.json");
    window.title = "Demo";
    window.reticleLibraryFolder = std::filesystem::path("reticles");
    window.feedbackFastIntervalSeconds = 0.025f;
    window.feedbackHeartbeatIntervalSeconds = 0.400f;

    mfd::WindowUdpFeedbackTransport feedbackUdp;
    feedbackUdp.enabled = true;
    feedbackUdp.address = "127.0.0.1";
    feedbackUdp.port = 47221;
    feedbackUdp.maxPacketSize = 4096;
    window.feedbackTransports.udp = feedbackUdp;

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(std::filesystem::path("C:/workspace/pages/radar.json"));

    const auto jsonNode = nlohmann::json::parse(editor::SerializeWindowToJsonString(window, document, layout));
    ASSERT_TRUE(jsonNode.contains("feedback"));
    const auto& feedbackNode = jsonNode.at("feedback");
    EXPECT_EQ(feedbackNode.at("fastIntervalMs").get<int>(), 25);
    EXPECT_EQ(feedbackNode.at("heartbeatIntervalMs").get<int>(), 400);
    EXPECT_FALSE(feedbackNode.contains("fastIntervalSeconds"));
    EXPECT_FALSE(feedbackNode.contains("heartbeatIntervalSeconds"));
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentWritesCurrentFilesAndRemovesObsoleteOnes)
{
    ScopedTempDir tempDir;

    const auto windowFile = tempDir.Path() / "demo_window.json";
    const auto pageFile = tempDir.Path() / "radar_page.json";
    const auto stalePageFile = tempDir.Path() / "obsolete_page.json";
    const auto reticleLibraryFolder = tempDir.Path() / "reticles";
    const auto templateFile = reticleLibraryFolder / "radar_track.json";
    const auto staleTemplateFile = reticleLibraryFolder / "obsolete_track.json";

    std::filesystem::create_directories(reticleLibraryFolder);
    {
        std::ofstream stalePage(stalePageFile);
        stalePage << "{}";
    }
    {
        std::ofstream staleTemplate(staleTemplateFile);
        staleTemplate << "{}";
    }

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = windowFile;
    loaded.window.title = "Demo";
    loaded.window.width = 800;
    loaded.window.height = 600;
    loaded.window.positionX = 10;
    loaded.window.positionY = 20;
    loaded.window.targetFps = 75;
    loaded.window.reticleLibraryFolder = reticleLibraryFolder;

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar Page";
    page.defaultPage = true;
    page.backgroundColor = mfd::ColorRgba {10, 20, 30, 255};
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    page.dynamicReticleBindings.push_back(mfd::DynamicReticleLayerBinding {"radar_track", "default", 0});

    mfd::PageStrobeDefinition strobe;
    strobe.name = "Default";
    strobe.normalizedName = mfd::NormalizePageName(strobe.name);
    strobe.reticle.id = "strobe";
    strobe.reticle.sourceTemplateId = "strobe_cursor";
    strobe.capture.shape = mfd::StrobeCaptureShape::Circle;
    strobe.capture.radius = 0.12f;
    strobe.magnet.enabled = true;
    strobe.magnet.radius = 0.2f;
    strobe.magnet.strength = 0.5f;
    strobe.magnet.visualShapeEnabled = true;
    strobe.magnet.visualShape = mfd::StrobeMagnetVisualShape::Square;
    strobe.magnet.visualShapeSize = 0.18f;
    page.strobes.push_back(strobe);
    page.activeStrobeName = strobe.name;
    page.normalizedActiveStrobeName = strobe.normalizedName;

    mfd::ReticleGroup pageReticle;
    pageReticle.id = "page1_ownship";
    pageReticle.sourceTemplateId = "radar_track";
    pageReticle.transform.position = {0.1f, 0.2f};
    pageReticle.layerId = "default";
    page.staticReticles.push_back(std::move(pageReticle));

    loaded.document.pages.push_back(std::move(page));

    mfd::ReticleGroup templateReticle;
    templateReticle.id = "radar_track";
    mfd::Primitive primitive;
    primitive.id = "track_label";
    primitive.type = mfd::PrimitiveType::Text;
    primitive.geometry = mfd::TextGeometry {"INIT", 0.04f, 0.002f};
    templateReticle.primitives.push_back(std::move(primitive));
    loaded.document.reticleLibrary.emplace("radar_track", std::move(templateReticle));
    loaded.document.reticleLibrary.emplace("strobe_cursor", mfd::ReticleGroup {"strobe_cursor"});

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(pageFile);
    layout.templateFiles.emplace("radar_track", templateFile);
    layout.templateFiles.emplace("strobe_cursor", tempDir.Path() / "reticles" / "strobe_cursor.json");
    layout.removedPageFiles.push_back(stalePageFile);
    layout.removedTemplateFiles.push_back(staleTemplateFile);

    std::string error;
    ASSERT_TRUE(editor::SaveEditorDocument(loaded, layout, &error)) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(std::filesystem::exists(windowFile));
    EXPECT_TRUE(std::filesystem::exists(pageFile));
    EXPECT_TRUE(std::filesystem::exists(templateFile));
    EXPECT_FALSE(std::filesystem::exists(stalePageFile));
    EXPECT_FALSE(std::filesystem::exists(staleTemplateFile));

    std::ifstream windowStream(windowFile);
    ASSERT_TRUE(windowStream.is_open());
    const auto windowJson = nlohmann::json::parse(windowStream);
    EXPECT_EQ(windowJson.at("title").get<std::string>(), "Demo");
    EXPECT_EQ(windowJson.at("pages").at(0).get<std::string>(), "radar_page.json");
    EXPECT_EQ(windowJson.at("defaultPage").get<std::string>(), "Radar");
    EXPECT_EQ(windowJson.at("reticleLibraryFolder").get<std::string>(), "reticles");

    std::ifstream pageStream(pageFile);
    ASSERT_TRUE(pageStream.is_open());
    const auto pageJson = nlohmann::json::parse(pageStream);
    EXPECT_EQ(pageJson.at("name").get<std::string>(), "Radar");
    EXPECT_EQ(pageJson.at("title").get<std::string>(), "Radar Page");
    EXPECT_EQ(pageJson.at("bg").get<std::string>(), "#0A141EFF");
    ASSERT_TRUE(pageJson.contains("strobes"));
    ASSERT_TRUE(pageJson.at("strobes").is_array());
    ASSERT_EQ(pageJson.at("strobes").size(), 1U);
    EXPECT_EQ(pageJson.at("activeStrobe").get<std::string>(), "Default");
    EXPECT_EQ(pageJson.at("strobes").at(0).at("name").get<std::string>(), "Default");
    EXPECT_EQ(pageJson.at("strobes").at(0).at("template").get<std::string>(), "strobe_cursor");
    EXPECT_TRUE(pageJson.at("strobes").at(0).at("magnet").at("enabled").get<bool>());
    ASSERT_TRUE(pageJson.at("strobes").at(0).at("magnet").contains("visual"));
    EXPECT_TRUE(pageJson.at("strobes").at(0).at("magnet").at("visual").at("enabled").get<bool>());
    EXPECT_EQ(pageJson.at("strobes").at(0).at("magnet").at("visual").at("shape").get<std::string>(), "square");
    EXPECT_FLOAT_EQ(pageJson.at("strobes").at(0).at("magnet").at("visual").at("size").get<float>(), 0.18f);
    ASSERT_TRUE(pageJson.contains("layers"));
    ASSERT_EQ(pageJson.at("layers").size(), 1U);
    EXPECT_EQ(pageJson.at("layers").at(0).at("id").get<std::string>(), "default");
    ASSERT_TRUE(pageJson.contains("dynamicReticleBindings"));
    ASSERT_EQ(pageJson.at("dynamicReticleBindings").size(), 1U);
    EXPECT_EQ(pageJson.at("dynamicReticleBindings").at(0).at("templateId").get<std::string>(), "radar_track");
    EXPECT_EQ(pageJson.at("dynamicReticleBindings").at(0).at("layerId").get<std::string>(), "default");
    EXPECT_EQ(pageJson.at("dynamicReticleBindings").at(0).at("orderInLayer").get<int>(), 0);
    ASSERT_EQ(pageJson.at("staticReticles").size(), 1U);
    EXPECT_EQ(pageJson.at("staticReticles").at(0).at("id").get<std::string>(), "page1_ownship");
    EXPECT_EQ(pageJson.at("staticReticles").at(0).at("template").get<std::string>(), "radar_track");
    EXPECT_EQ(pageJson.at("staticReticles").at(0).at("layerId").get<std::string>(), "default");

    std::ifstream templateStream(templateFile);
    ASSERT_TRUE(templateStream.is_open());
    const auto templateJson = nlohmann::json::parse(templateStream);
    EXPECT_EQ(templateJson.at("id").get<std::string>(), "radar_track");
    ASSERT_EQ(templateJson.at("elements").size(), 1U);
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentSerializesPageTitleDisplayChrome)
{
    ScopedTempDir tempDir;

    const auto windowFile = tempDir.Path() / "window.json";
    const auto pageFile = tempDir.Path() / "page.json";

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = windowFile;
    loaded.window.title = "Demo";
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar Page";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    page.titleDisplay.visible = false;
    page.titleDisplay.transform.position = {-1.15f, 0.84f};
    page.titleDisplay.transform.scale = {1.20f, 0.95f};
    page.titleDisplay.color = mfd::ColorRgba {255, 200, 64, 255};
    page.titleDisplay.lineWidth = 0.0080f;
    page.titleDisplay.lineStyle = mfd::LineStyle::Dotted;
    page.titleDisplay.decoration = mfd::PageTitleDecoration::Frame;
    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(pageFile);

    std::string error;
    ASSERT_TRUE(editor::SaveEditorDocument(loaded, layout, &error)) << error;

    std::ifstream pageStream(pageFile);
    ASSERT_TRUE(pageStream.is_open());
    const auto pageJson = nlohmann::json::parse(pageStream);

    ASSERT_TRUE(pageJson.contains("titleDisplay"));
    const auto& titleDisplayJson = pageJson.at("titleDisplay");
    EXPECT_FALSE(titleDisplayJson.at("visible").get<bool>());
    EXPECT_EQ(titleDisplayJson.at("decoration").get<std::string>(), "frame");
    EXPECT_EQ(titleDisplayJson.at("lineStyle").get<std::string>(), "dotted");
    EXPECT_EQ(titleDisplayJson.at("stroke").get<std::string>(), "#FFC840FF");
    EXPECT_FLOAT_EQ(titleDisplayJson.at("lineWidth").get<float>(), 0.0080f);
    ASSERT_TRUE(titleDisplayJson.contains("at"));
    EXPECT_FLOAT_EQ(titleDisplayJson.at("at").at(0).get<float>(), -1.15f);
    EXPECT_FLOAT_EQ(titleDisplayJson.at("at").at(1).get<float>(), 0.84f);
    ASSERT_TRUE(titleDisplayJson.contains("scale"));
    EXPECT_FLOAT_EQ(titleDisplayJson.at("scale").at(0).get<float>(), 1.20f);
    EXPECT_FLOAT_EQ(titleDisplayJson.at("scale").at(1).get<float>(), 0.95f);
}

TEST(EditorDocumentSerializerTests, PageTitleDisplayDefaultsStartInsideVisiblePage)
{
    const mfd::PageTitleDisplayDefinition defaults {};

    EXPECT_FLOAT_EQ(defaults.transform.position.x, -0.95f);
    EXPECT_FLOAT_EQ(defaults.transform.position.y, 0.925f);
    EXPECT_FLOAT_EQ(defaults.transform.rotationDegrees, 0.0f);
    EXPECT_FLOAT_EQ(defaults.transform.scale.x, 1.0f);
    EXPECT_FLOAT_EQ(defaults.transform.scale.y, 1.0f);
}

TEST(EditorDocumentSerializerTests, SaveEditorDocumentOmitsDefaultPageTitleTransformWhenOnlyDecorationChanges)
{
    ScopedTempDir tempDir;

    const auto windowFile = tempDir.Path() / "window.json";
    const auto pageFile = tempDir.Path() / "page.json";

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = windowFile;
    loaded.window.title = "Demo";
    loaded.window.reticleLibraryFolder = tempDir.Path() / "reticles";

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar Page";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    page.titleDisplay.decoration = mfd::PageTitleDecoration::Frame;
    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(pageFile);

    std::string error;
    ASSERT_TRUE(editor::SaveEditorDocument(loaded, layout, &error)) << error;

    std::ifstream pageStream(pageFile);
    ASSERT_TRUE(pageStream.is_open());
    const auto pageJson = nlohmann::json::parse(pageStream);

    ASSERT_TRUE(pageJson.contains("titleDisplay"));
    const auto& titleDisplayJson = pageJson.at("titleDisplay");
    EXPECT_EQ(titleDisplayJson.at("decoration").get<std::string>(), "frame");
    EXPECT_FALSE(titleDisplayJson.contains("at"));
}

TEST(EditorDocumentSerializerTests, RecoveryBundleRoundTripsAllAuthoredFiles)
{
    ScopedTempDir tempDir;

    const auto windowFile = tempDir.Path() / "demo_window.json";
    const auto pageFile = tempDir.Path() / "radar_page.json";
    const auto reticleLibraryFolder = tempDir.Path() / "reticles";
    const auto templateFile = reticleLibraryFolder / "radar_track.json";

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = windowFile;
    loaded.window.title = "Demo";
    loaded.window.width = 800;
    loaded.window.height = 600;
    loaded.window.reticleLibraryFolder = reticleLibraryFolder;

    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar Page";
    page.defaultPage = true;
    page.layers.push_back(mfd::PageLayerDefinition {"default"});

    mfd::ReticleGroup pageReticle;
    pageReticle.id = "page1_ownship";
    pageReticle.sourceTemplateId = "radar_track";
    pageReticle.layerId = "default";
    page.staticReticles.push_back(std::move(pageReticle));
    loaded.document.pages.push_back(std::move(page));

    mfd::ReticleGroup templateReticle;
    templateReticle.id = "radar_track";
    mfd::Primitive primitive;
    primitive.id = "track_label";
    primitive.type = mfd::PrimitiveType::Text;
    primitive.geometry = mfd::TextGeometry {"INIT", 0.04f, 0.002f};
    templateReticle.primitives.push_back(std::move(primitive));
    loaded.document.reticleLibrary.emplace("radar_track", std::move(templateReticle));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(pageFile);
    layout.templateFiles.emplace("radar_track", templateFile);

    const std::string bundle = editor::SerializeRecoveryBundleToJsonString(loaded, layout);
    ASSERT_FALSE(bundle.empty());

    // Nothing is on disk yet: the recovery path must materialize every authored file.
    ASSERT_FALSE(std::filesystem::exists(windowFile));

    std::filesystem::path recoveredWindowFile;
    std::string error;
    ASSERT_TRUE(editor::RestoreRecoveryBundle(bundle, tempDir.Path(), recoveredWindowFile, &error)) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(recoveredWindowFile, windowFile);
    EXPECT_TRUE(std::filesystem::exists(windowFile));
    EXPECT_TRUE(std::filesystem::exists(pageFile));
    EXPECT_TRUE(std::filesystem::exists(templateFile));
    // No staging artifact must be left behind on success.
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(windowFile).concat(".recovery_tmp")));

    // The restored tree must reload cleanly through the runtime loader.
    mfd::JsonLoader loader;
    EXPECT_NO_THROW(loader.LoadWindowConfiguration(recoveredWindowFile));
}

TEST(EditorDocumentSerializerTests, RestoreRecoveryBundleRejectsMalformedJson)
{
    std::filesystem::path windowFile;
    std::string error;
    EXPECT_FALSE(editor::RestoreRecoveryBundle("{ not valid json", std::filesystem::temp_directory_path(),
                                               windowFile, &error));
    EXPECT_FALSE(error.empty());
}

TEST(EditorDocumentSerializerTests, RestoreRecoveryBundleRejectsPathOutsideAssetRoot)
{
    ScopedTempDir assetRoot;
    ScopedTempDir outside;

    const auto windowInside = assetRoot.Path() / "window.json";
    const auto escapingFile = outside.Path() / "stolen.json";

    nlohmann::json bundle = nlohmann::json::object();
    bundle["schemaVersion"] = 1;
    bundle["windowFile"] = windowInside.generic_string();
    nlohmann::json files = nlohmann::json::array();
    nlohmann::json entry = nlohmann::json::object();
    entry["path"] = escapingFile.generic_string();
    entry["document"] = nlohmann::json::object();
    files.push_back(std::move(entry));
    bundle["files"] = std::move(files);

    std::filesystem::path recoveredWindowFile;
    std::string error;
    EXPECT_FALSE(
        editor::RestoreRecoveryBundle(bundle.dump(), assetRoot.Path(), recoveredWindowFile, &error));
    EXPECT_FALSE(error.empty());
    // Nothing outside the asset root may be written, and no staging artifact may leak.
    EXPECT_FALSE(std::filesystem::exists(escapingFile));
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(escapingFile).concat(".recovery_tmp")));
}

TEST(EditorDocumentSerializerTests, RestoreRecoveryBundleLeavesAuthoredFilesIntactWhenAnEntryEscapes)
{
    ScopedTempDir assetRoot;
    ScopedTempDir outside;

    const auto windowInside = assetRoot.Path() / "window.json";
    const auto validPage = assetRoot.Path() / "pages" / "radar.json";
    const auto escapingFile = outside.Path() / "stolen.json";

    // Seed an existing authored file so we can assert it is never overwritten.
    std::filesystem::create_directories(validPage.parent_path());
    {
        std::ofstream seed(validPage, std::ios::binary | std::ios::trunc);
        seed << "{\"original\":true}\n";
    }

    nlohmann::json bundle = nlohmann::json::object();
    bundle["schemaVersion"] = 1;
    bundle["windowFile"] = windowInside.generic_string();
    nlohmann::json files = nlohmann::json::array();

    nlohmann::json validEntry = nlohmann::json::object();
    validEntry["path"] = validPage.generic_string();
    validEntry["document"] = nlohmann::json {{"replaced", true}};
    files.push_back(std::move(validEntry));

    nlohmann::json escapingEntry = nlohmann::json::object();
    escapingEntry["path"] = escapingFile.generic_string();
    escapingEntry["document"] = nlohmann::json::object();
    files.push_back(std::move(escapingEntry));

    bundle["files"] = std::move(files);

    std::filesystem::path recoveredWindowFile;
    std::string error;
    EXPECT_FALSE(
        editor::RestoreRecoveryBundle(bundle.dump(), assetRoot.Path(), recoveredWindowFile, &error));

    // The escaping entry is rejected during validation, before any write, so the pre-existing authored
    // file keeps its original content and no staging artifact remains.
    std::ifstream check(validPage, std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(check)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("\"original\""), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(validPage).concat(".recovery_tmp")));
}

TEST(EditorDocumentSerializerTests, SaveRejectsUnsafePageIdentifiersBeforeWriting)
{
    ScopedTempDir assetRoot;
    ScopedTempDir outside;
    const std::filesystem::path sentinel = outside.Path() / "sentinel.json";
    {
        std::ofstream stream(sentinel, std::ios::binary | std::ios::trunc);
        stream << "unchanged\n";
    }

    const std::vector<std::string> unsafeNames = {"../x", "..\\x", "C:\\x", "CON", "a/b", "a\\b"};
    for (const std::string& unsafeName : unsafeNames)
    {
        mfd::LoadedWindowConfiguration loaded;
        loaded.window.sourceFile = assetRoot.Path() / "windows" / "demo.json";
        loaded.window.reticleLibraryFolder = assetRoot.Path() / "reticles";
        mfd::PageDefinition page;
        page.name = unsafeName;
        page.title = "Unsafe";
        page.layers.push_back(mfd::PageLayerDefinition {"default"});
        loaded.document.pages.push_back(std::move(page));

        editor::EditorFileLayout layout;
        layout.pageFiles.push_back(assetRoot.Path() / "pages" / "page.json");
        std::string error;
        EXPECT_FALSE(editor::SaveEditorDocument(loaded, layout, &error)) << unsafeName;
        EXPECT_FALSE(error.empty()) << unsafeName;
        EXPECT_FALSE(std::filesystem::exists(loaded.window.sourceFile));
    }

    std::ifstream stream(sentinel, std::ios::binary);
    const std::string sentinelContent((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(sentinelContent, "unchanged\n");
}

TEST(EditorDocumentSerializerTests, SaveRejectsTargetOutsideAssetRootBeforeWriting)
{
    ScopedTempDir assetRoot;
    ScopedTempDir outside;

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = assetRoot.Path() / "windows" / "demo.json";
    loaded.window.reticleLibraryFolder = assetRoot.Path() / "reticles";
    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    loaded.document.pages.push_back(std::move(page));

    const std::filesystem::path escapingPage = outside.Path() / "radar.json";
    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(escapingPage);
    std::string error;
    EXPECT_FALSE(editor::SaveEditorDocument(loaded, layout, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(std::filesystem::exists(escapingPage));
    EXPECT_FALSE(std::filesystem::exists(loaded.window.sourceFile));
}

TEST(EditorDocumentSerializerTests, SaveStagingFailurePreservesEveryOriginalFile)
{
    ScopedTempDir assetRoot;
    const std::filesystem::path windowFile = assetRoot.Path() / "windows" / "demo.json";
    const std::filesystem::path blockedPageFolder = assetRoot.Path() / "pages";
    std::filesystem::create_directories(windowFile.parent_path());
    {
        std::ofstream stream(windowFile, std::ios::binary | std::ios::trunc);
        stream << "{\"original\":true}\n";
    }
    {
        std::ofstream stream(blockedPageFolder, std::ios::binary | std::ios::trunc);
        stream << "folder blocker\n";
    }

    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = windowFile;
    loaded.window.reticleLibraryFolder = assetRoot.Path() / "reticles";
    mfd::PageDefinition page;
    page.name = "Radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    loaded.document.pages.push_back(std::move(page));

    editor::EditorFileLayout layout;
    layout.pageFiles.push_back(blockedPageFolder / "radar.json");
    std::string error;
    EXPECT_FALSE(editor::SaveEditorDocument(loaded, layout, &error));
    EXPECT_FALSE(error.empty());

    std::ifstream windowStream(windowFile, std::ios::binary);
    const std::string windowContent((std::istreambuf_iterator<char>(windowStream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(windowContent, "{\"original\":true}\n");
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(windowFile).concat(".mfd_save_tmp_0")));
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(windowFile).concat(".mfd_save_backup_0")));
}
