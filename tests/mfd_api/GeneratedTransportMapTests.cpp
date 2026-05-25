/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for generated transport map loading and integration.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>

#include "mfd/io/JsonLoader.h"
#include "mfd/runtime/GeneratedTransportMap.h"
#include "mfd/runtime/SceneRegistry.h"

namespace
{
class TemporaryFolder
{
public:
    TemporaryFolder()
    {
        const auto uniqueId =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        path_ = std::filesystem::temp_directory_path() / ("mfd_generated_map_tests_" + uniqueId);
        std::filesystem::create_directories(path_);
    }

    TemporaryFolder(const TemporaryFolder&) = delete;
    TemporaryFolder& operator=(const TemporaryFolder&) = delete;

    ~TemporaryFolder()
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
    ASSERT_TRUE(stream.is_open()) << "Unable to create test file: " << path.string();
    stream << content;
    stream.close();
}

void WriteBasicWindowAssets(const TemporaryFolder& workspace)
{
    WriteTextFile(workspace.Path() / "reticles" / "status_template.json",
                  R"json({
  "id": "status_template",
  "elements": [
    { "id": "status_value", "type": "text", "text": "INIT" }
  ]
})json");

    WriteTextFile(workspace.Path() / "pages" / "main.json",
                  R"json({
  "name": "Main",
  "layers": [
    { "id": "default" }
  ],
  "blinkTypes": [
    { "name": "slow", "durationMs": 750 }
  ],
  "staticReticles": [
    { "id": "status", "template": "status_template", "layerId": "default" }
  ]
})json");

    WriteTextFile(workspace.Path() / "window.json",
                  R"json({
  "title": "Main Window",
  "reticleLibraryFolder": "reticles",
  "defaultPage": "Main",
  "pages": [
    "pages/main.json"
  ]
})json");
}
} // namespace

TEST(GeneratedTransportMapTests, CompanionPathUsesWindowStemNextToWindowFile)
{
    const std::filesystem::path windowFile = "C:/repo/assets/windows/demo_pages.json";
    const std::filesystem::path expected = "C:/repo/assets/windows/demo_pages.generated.map";

    EXPECT_EQ(mfd::CompanionGeneratedTransportMapPath(windowFile).lexically_normal(),
              expected.lexically_normal());
}

TEST(GeneratedTransportMapTests, MissingCompanionMapReturnsNulloptWithoutError)
{
    TemporaryFolder workspace;
    std::string error = "preset";

    const auto map = mfd::TryLoadGeneratedTransportMap(workspace.Path() / "window.json", &error);

    EXPECT_FALSE(map.has_value());
    EXPECT_TRUE(error.empty());
}

TEST(GeneratedTransportMapTests, InvalidCompanionMapReportsValidationError)
{
    TemporaryFolder workspace;
    WriteTextFile(workspace.Path() / "window.generated.map",
                  R"json({
  "schemaVersion": 2,
  "mappingHash": "hash",
  "window": { "name": "window", "title": "Main Window", "source": "window.json" },
  "pages": [],
  "reticles": [],
  "primitives": [],
  "templates": [],
  "blinkTypes": []
})json");

    std::string error;
    const auto map = mfd::TryLoadGeneratedTransportMap(workspace.Path() / "window.json", &error);

    EXPECT_FALSE(map.has_value());
    EXPECT_NE(error.find("schemaVersion"), std::string::npos);
}

TEST(GeneratedTransportMapTests, JsonLoaderLoadsValidatedCompanionMapAndSceneKeepsIt)
{
    TemporaryFolder workspace;
    WriteBasicWindowAssets(workspace);
    WriteTextFile(workspace.Path() / "window.generated.map",
                  R"json({
  "schemaVersion": 1,
  "mappingHash": "abc123",
  "window": {
    "name": "window",
    "title": "Main Window",
    "source": "window.json"
  },
  "pages": [
    {
      "id": 1,
      "name": "Main",
      "normalizedName": "main",
      "hasStrobe": false,
      "defaultPage": true
    }
  ],
  "reticles": [
    {
      "id": 2,
      "pageId": 1,
      "reticleId": "status",
      "normalizedReticleId": "status",
      "source": "static"
    }
  ],
  "primitives": [
    {
      "id": 4,
      "ownerKind": "template",
      "ownerId": 3,
      "primitiveId": "status_value",
      "normalizedPrimitiveId": "status_value",
      "primitiveType": "text",
      "exposed": true
    }
  ],
  "templates": [
    {
      "id": 3,
      "templateId": "status_template",
      "normalizedTemplateId": "status_template"
    }
  ],
  "blinkTypes": [
    {
      "id": 5,
      "pageId": 1,
      "blinkType": "slow",
      "normalizedBlinkType": "slow",
      "durationMs": 750
    }
  ],
  "strobes": []
})json");

    mfd::JsonLoader loader;
    mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(workspace.Path() / "window.json");

    ASSERT_TRUE(loaded.generatedTransportMap.has_value());
    EXPECT_EQ(loaded.generatedTransportMap->mappingHash, "abc123");
    EXPECT_EQ(loaded.generatedTransportMap->templates.size(), 1U);
    EXPECT_EQ(loaded.generatedTransportMap->primitives.size(), 1U);

    mfd::SceneRegistry scene(std::move(loaded.document), loaded.generatedTransportMap);
    EXPECT_TRUE(scene.HasTransportMap());
    ASSERT_TRUE(scene.TransportMap().has_value());
    EXPECT_EQ(scene.TransportMap()->window.source, "window.json");
}

TEST(GeneratedTransportMapTests, JsonLoaderRejectsCompanionMapThatDriftsFromWindowDocument)
{
    TemporaryFolder workspace;
    WriteBasicWindowAssets(workspace);
    WriteTextFile(workspace.Path() / "window.generated.map",
                  R"json({
  "schemaVersion": 1,
  "mappingHash": "abc123",
  "window": {
    "name": "window",
    "title": "Main Window",
    "source": "window.json"
  },
  "pages": [
    {
      "id": 1,
      "name": "Main",
      "normalizedName": "main",
      "hasStrobe": false,
      "defaultPage": true
    }
  ],
  "reticles": [
    {
      "id": 2,
      "pageId": 1,
      "reticleId": "status",
      "normalizedReticleId": "status",
      "source": "static"
    }
  ],
  "primitives": [
    {
      "id": 4,
      "ownerKind": "template",
      "ownerId": 3,
      "primitiveId": "status_value",
      "normalizedPrimitiveId": "status_value",
      "primitiveType": "line",
      "exposed": true
    }
  ],
  "templates": [
    {
      "id": 3,
      "templateId": "status_template",
      "normalizedTemplateId": "status_template"
    }
  ],
  "blinkTypes": [
    {
      "id": 5,
      "pageId": 1,
      "blinkType": "slow",
      "normalizedBlinkType": "slow",
      "durationMs": 750
    }
  ],
  "strobes": []
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadWindowConfiguration(workspace.Path() / "window.json"), std::runtime_error);
}

TEST(GeneratedTransportMapTests, JsonLoaderRejectsCompanionMapWithUnknownReticleSource)
{
    TemporaryFolder workspace;
    WriteBasicWindowAssets(workspace);
    WriteTextFile(workspace.Path() / "window.generated.map",
                  R"json({
  "schemaVersion": 1,
  "mappingHash": "abc123",
  "window": {
    "name": "window",
    "title": "Main Window",
    "source": "window.json"
  },
  "pages": [
    {
      "id": 1,
      "name": "Main",
      "normalizedName": "main",
      "hasStrobe": false,
      "defaultPage": true
    }
  ],
  "reticles": [
    {
      "id": 2,
      "pageId": 1,
      "reticleId": "status",
      "normalizedReticleId": "status",
      "source": "invalid"
    }
  ],
  "primitives": [],
  "templates": [],
  "blinkTypes": [],
  "strobes": []
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadWindowConfiguration(workspace.Path() / "window.json"), std::runtime_error);
}
