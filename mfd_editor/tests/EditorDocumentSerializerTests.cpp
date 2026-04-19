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
#include <string>

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
    const auto pagePath = editor::DefaultPageFilePath("/tmp/assets/windows/demo.json", "Page 01");
    const auto templatePath = editor::DefaultTemplateFilePath("/tmp/assets/reticles", "Radar Track");

    const auto expectedPage = std::filesystem::path("/tmp/assets/windows") /
                              std::filesystem::path(mfd::NormalizePageName("Page 01") + ".json");
    const auto expectedTemplate = std::filesystem::path("/tmp/assets/reticles") /
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

TEST(EditorDocumentSerializerTests, SerializeReticleTemplateIncludesTemplateId)
{
    mfd::ReticleGroup reticle;
    reticle.id = "demo_track";

    const std::string jsonText = editor::SerializeReticleTemplateToJsonString(reticle);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    ASSERT_TRUE(jsonNode.contains("id"));
    EXPECT_EQ(jsonNode.at("id").get<std::string>(), "demo_track");
}

TEST(EditorDocumentSerializerTests, SerializeReticleTemplatePreservesRingPrimitiveTypeAndGeometry)
{
    mfd::ReticleGroup reticle;
    reticle.id = "ring_template";

    mfd::Primitive ring;
    ring.id = "scan_ring";
    ring.type = mfd::PrimitiveType::Ring;
    ring.geometry = mfd::RingGeometry {.innerRadius = 0.012f, .outerRadius = 0.02f, .segments = 72};
    reticle.primitives.push_back(std::move(ring));

    const std::string jsonText = editor::SerializeReticleTemplateToJsonString(reticle);
    const auto jsonNode = nlohmann::json::parse(jsonText);

    ASSERT_TRUE(jsonNode.contains("elements"));
    ASSERT_TRUE(jsonNode.at("elements").is_array());
    ASSERT_EQ(jsonNode.at("elements").size(), 1U);

    const auto& element = jsonNode.at("elements").at(0);
    ASSERT_TRUE(element.contains("type"));
    EXPECT_EQ(element.at("type").get<std::string>(), "ring");
    EXPECT_FLOAT_EQ(element.at("innerRadius").get<float>(), 0.012f);
    EXPECT_FLOAT_EQ(element.at("outerRadius").get<float>(), 0.02f);
    EXPECT_EQ(element.at("segments").get<int>(), 72);
}
