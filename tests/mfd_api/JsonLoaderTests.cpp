/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for JsonLoaderTests.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "mfd/io/JsonLoader.h"
#include "mfd/model/PageDefinition.h"

namespace
{
using namespace std::chrono_literals;

std::filesystem::path RepositoryRoot()
{
    const std::filesystem::path testFile = std::filesystem::path(__FILE__).lexically_normal();
    return testFile.parent_path().parent_path().parent_path();
}

class TemporaryFolder
{
public:
    TemporaryFolder()
    {
        const auto uniqueId =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        path_ = std::filesystem::temp_directory_path() / ("mfd_json_loader_tests_" + uniqueId);
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
} // namespace

TEST(JsonLoaderTests, LoadWindowConfigurationResolvesRelativeAssetsAndBlinkDefaults)
{
    TemporaryFolder workspace;
    const std::filesystem::path fontFile = workspace.Path() / "fonts" / "demo.ttf";
    const std::filesystem::path iconFile = workspace.Path() / "branding" / "demo_icon.png";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";
    const std::filesystem::path pagesFolder = workspace.Path() / "pages";
    const std::filesystem::path windowFile = workspace.Path() / "window.json";
    const std::filesystem::path documentWindowFile = workspace.Path() / "window_for_document.json";
    const std::filesystem::path pageFile = pagesFolder / "main.json";

    WriteTextFile(reticleFolder / "marker.json",
                  R"json({
  "id": "marker",
  "elements": [
    {
      "id": "marker_text",
      "type": "text",
      "text": "MARK",
      "size": 0.04
    }
  ]
})json");

    WriteTextFile(pageFile,
                  R"json({
  "name": "Main",
  "title": "Main Page",
  "layers": [
    { "id": "default" }
  ],
  "blinkTypes": [
    { "name": "slow", "durationMs": 1200 },
    { "name": "caution", "durationMs": 1200 },
    { "name": "fast", "durationMs": 300 }
  ],
  "defaultBlink": "slow",
  "staticReticles": [
    { "id": "default_marker", "template": "marker", "layerId": "default", "blink": true },
    { "id": "caution_marker", "template": "marker", "layerId": "default", "blink": "caution" }
  ]
})json");

    WriteTextFile(fontFile, "dummy font file");
    WriteTextFile(iconFile, "dummy icon file");

    WriteTextFile(windowFile,
                  R"json({
  "title": "Test Window",
  "size": [640, 480],
  "position": [10, 20],
  "targetFps": 40,
  "fontFile": "fonts/demo.ttf",
  "iconFile": "branding/demo_icon.png",
  "reticleLibraryFolder": "reticles",
  "commands": {
    "udp": {
      "enabled": true,
      "address": "127.0.0.1",
      "port": 48000,
      "maxPacketSize": 2048
    }
  },
  "feedback": {
    "fastIntervalMs": 25,
    "heartbeatIntervalMs": 400,
    "udp": {
      "enabled": true,
      "address": "127.0.0.1",
      "port": 48001,
      "maxPacketSize": 1024
    }
  },
  "defaultPage": "Main",
  "pages": [
    "pages/main.json"
  ]
})json");

    WriteTextFile(documentWindowFile,
                  R"json({
  "title": "Aliased Window",
  "width": 800,
  "height": 600,
  "x": 30,
  "y": 40,
  "fps": 55,
  "reticles": "reticles",
  "defaultPage": "Main",
  "pages": [
    {
      "path": "pages/main.json"
    }
  ]
})json");

    WriteTextFile(documentWindowFile,
                  R"json({
  "title": "Aliased Window",
  "width": 800,
  "height": 600,
  "x": 30,
  "y": 40,
  "fps": 55,
  "reticles": "reticles",
  "pages": [
    {
      "path": "pages/main.json"
    }
  ]
})json");

    WriteTextFile(documentWindowFile,
                  R"json({
  "title": "Aliased Window",
  "width": 800,
  "height": 600,
  "x": 30,
  "y": 40,
  "fps": 55,
  "reticles": "reticles",
  "pages": [
    {
      "path": "pages/main.json"
    }
  ]
})json");

    mfd::JsonLoader loader;
    const mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(windowFile);

    EXPECT_EQ(loaded.window.title, "Test Window");
    EXPECT_EQ(loaded.window.width, 640);
    EXPECT_EQ(loaded.window.height, 480);
    EXPECT_EQ(loaded.window.positionX, 10);
    EXPECT_EQ(loaded.window.positionY, 20);
    EXPECT_EQ(loaded.window.targetFps, 40);
    EXPECT_EQ(loaded.window.fontFile.lexically_normal(), fontFile.lexically_normal());
    EXPECT_EQ(loaded.window.iconFile.lexically_normal(), iconFile.lexically_normal());
    ASSERT_TRUE(loaded.window.commandTransports.udp.has_value());
    ASSERT_TRUE(loaded.window.feedbackTransports.udp.has_value());
    EXPECT_EQ(loaded.window.commandTransports.udp->port, 48000);
    EXPECT_EQ(loaded.window.feedbackTransports.udp->port, 48001);
    EXPECT_FLOAT_EQ(loaded.window.feedbackFastIntervalSeconds, 0.025f);
    EXPECT_FLOAT_EQ(loaded.window.feedbackHeartbeatIntervalSeconds, 0.400f);
    EXPECT_EQ(loaded.window.reticleLibraryFolder.lexically_normal(), reticleFolder.lexically_normal());
    ASSERT_EQ(loaded.window.pageFiles.size(), 1U);
    EXPECT_EQ(loaded.window.pageFiles.front().lexically_normal(), pageFile.lexically_normal());

    ASSERT_EQ(loaded.document.pages.size(), 1U);
    const mfd::PageDefinition& page = loaded.document.pages.front();
    EXPECT_EQ(page.name, "Main");
    EXPECT_TRUE(page.defaultPage);
    EXPECT_EQ(page.defaultBlinkTypeName, "slow");
    ASSERT_EQ(page.blinkTypes.size(), 3U);
    ASSERT_EQ(page.staticReticles.size(), 2U);
    EXPECT_EQ(page.staticReticles[0].blink.enabled, true);
    EXPECT_EQ(page.staticReticles[0].blink.durationMs, 1200U);
    EXPECT_EQ(page.staticReticles[1].blink.typeName, "caution");
    EXPECT_EQ(page.staticReticles[1].blink.durationMs, 1200U);
}

TEST(JsonLoaderTests, LoadDocumentRejectsUnknownDefaultBlinkType)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    WriteTextFile(reticleFolder / "marker.json",
                  R"json({
  "id": "marker",
  "elements": [
    { "id": "shape", "type": "circle", "radius": 0.05 }
  ]
})json");

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "blinkTypes": [
        { "name": "slow", "durationMs": 1000 }
      ],
      "defaultBlink": "missing",
      "staticReticles": [
        { "id": "marker_1", "template": "marker", "layerId": "default" }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentRejectsUnknownReticleBlinkType)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    WriteTextFile(reticleFolder / "marker.json",
                  R"json({
  "id": "marker",
  "elements": [
    { "id": "shape", "type": "circle", "radius": 0.05 }
  ]
})json");

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "blinkTypes": [
        { "name": "slow", "durationMs": 1000 }
      ],
      "staticReticles": [
        { "id": "marker_1", "template": "marker", "layerId": "default", "blink": "fast" }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentReportsInvalidJsonPathAndParseContext)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Broken"
    }
  ]
)json");

    mfd::JsonLoader loader;
    try
    {
        loader.LoadDocument(pagesFile);
        FAIL() << "Expected std::runtime_error for malformed JSON";
    }
    catch (const std::runtime_error& exception)
    {
        const std::string message = exception.what();
        EXPECT_NE(message.find("Unable to parse JSON file"), std::string::npos);
        EXPECT_NE(message.find(pagesFile.string()), std::string::npos);
        EXPECT_NE(message.find("byte"), std::string::npos);
    }
}

TEST(JsonLoaderTests, LoadWindowConfigurationAndLoadDocumentSupportWindowAliasesAndPageWrapper)
{
    TemporaryFolder workspace;
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";
    const std::filesystem::path pagesFolder = workspace.Path() / "pages";
    const std::filesystem::path windowFile = workspace.Path() / "window.json";
    const std::filesystem::path documentWindowFile = workspace.Path() / "window_for_document.json";
    const std::filesystem::path pageFile = pagesFolder / "main.json";

    WriteTextFile(reticleFolder / "filename_marker.json",
                  R"json({
  "elements": [
    {
      "id": "label",
      "type": "text",
      "text": "FN",
      "size": 0.04
    }
  ]
})json");

    WriteTextFile(pageFile,
                  R"json({
  "page": {
    "id": "Main",
    "layers": [
      { "id": "default" }
    ],
    "view": {
      "center": [0.25, -0.5],
      "zoom": 1.0
    },
    "blinks": [
      { "id": "slow", "periodMs": 750 }
    ],
    "defaultBlinkType": "slow",
    "staticReticles": [
      {
        "id": "marker_1",
        "template": "filename_marker",
        "layerId": "default",
        "blink": {
          "enabled": true,
          "blinkType": "slow"
        }
      }
    ]
  }
})json");

    WriteTextFile(windowFile,
                  R"json({
  "title": "Aliased Window",
  "width": 800,
  "height": 600,
  "x": 30,
  "y": 40,
  "fps": 55,
  "reticles": "reticles",
  "commands": {
    "udp": {
      "enabled": true,
      "host": "127.0.0.1",
      "listenPort": 49000,
      "bufferSize": 3000
    }
  },
  "events": {
    "udp": {
      "enabled": true,
      "host": "127.0.0.1",
      "listenPort": 49001,
      "bufferSize": 1500
    }
  },
  "defaultPage": "Main",
  "pageFiles": [
    {
      "path": "pages/main.json"
    }
  ]
})json");

    WriteTextFile(documentWindowFile,
                  R"json({
  "title": "Aliased Window",
  "width": 800,
  "height": 600,
  "x": 30,
  "y": 40,
  "fps": 55,
  "reticles": "reticles",
  "defaultPage": "Main",
  "pages": [
    {
      "path": "pages/main.json"
    }
  ]
})json");

    mfd::JsonLoader loader;
    const mfd::LoadedWindowConfiguration loadedWindow = loader.LoadWindowConfiguration(windowFile);

    EXPECT_EQ(loadedWindow.window.width, 800);
    EXPECT_EQ(loadedWindow.window.height, 600);
    EXPECT_EQ(loadedWindow.window.positionX, 30);
    EXPECT_EQ(loadedWindow.window.positionY, 40);
    EXPECT_EQ(loadedWindow.window.targetFps, 55);
    ASSERT_TRUE(loadedWindow.window.commandTransports.udp.has_value());
    ASSERT_TRUE(loadedWindow.window.feedbackTransports.udp.has_value());
    EXPECT_EQ(loadedWindow.window.commandTransports.udp->port, 49000);
    EXPECT_EQ(loadedWindow.window.commandTransports.udp->maxPacketSize, 3000U);
    EXPECT_EQ(loadedWindow.window.feedbackTransports.udp->port, 49001);
    EXPECT_EQ(loadedWindow.window.feedbackTransports.udp->maxPacketSize, 1500U);

    ASSERT_EQ(loadedWindow.document.pages.size(), 1U);
    const mfd::PageDefinition& page = loadedWindow.document.pages.front();
    EXPECT_EQ(page.name, "Main");
    EXPECT_TRUE(page.defaultPage);
    EXPECT_FLOAT_EQ(page.view.center.x, 0.25f);
    EXPECT_FLOAT_EQ(page.view.center.y, -0.5f);
    EXPECT_FLOAT_EQ(page.view.zoom, 1.0f);
    ASSERT_EQ(page.staticReticles.size(), 1U);
    EXPECT_EQ(page.staticReticles.front().sourceTemplateId, "filename_marker");
    EXPECT_EQ(page.staticReticles.front().blink.durationMs, 750U);

    const mfd::MfdDocument loadedDocument = loader.LoadDocument(documentWindowFile);
    ASSERT_EQ(loadedDocument.pages.size(), 1U);
    EXPECT_TRUE(loadedDocument.pages.front().defaultPage);
    EXPECT_EQ(loadedDocument.pages.front().staticReticles.front().sourceTemplateId, "filename_marker");
}

TEST(JsonLoaderTests, LoadDocumentParsesInlineAndTemplateReticleClipping)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    WriteTextFile(reticleFolder / "mask_template.json",
                  R"json({
  "id": "mask_template",
  "clipping": {
    "mode": "outer",
    "primitive": "body"
  },
  "elements": [
    { "id": "body", "type": "rectangle", "width": 0.12, "height": 0.08 },
    { "id": "mark", "type": "line", "start": [-0.04, 0.0], "end": [0.04, 0.0] }
  ]
})json");

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Radar",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "template_instance",
          "template": "mask_template",
          "layerId": "default",
          "clipping": {
            "mode": "inner",
            "primitive": "body"
          }
        },
        {
          "id": "template_inherited",
          "template": "mask_template",
          "layerId": "default"
        },
        {
          "id": "inline_instance",
          "layerId": "default",
          "clipping": {
            "mode": "outer",
            "primitive": "clip"
          },
          "elements": [
            { "id": "clip", "type": "ellipse", "width": 0.10, "height": 0.06 },
            { "id": "label", "type": "text", "text": "T", "size": 0.03 }
          ]
        }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    const mfd::MfdDocument loaded = loader.LoadDocument(pagesFile);

    ASSERT_EQ(loaded.reticleLibrary.size(), 1U);
    const auto templateIt = loaded.reticleLibrary.find("mask_template");
    ASSERT_NE(templateIt, loaded.reticleLibrary.end());
    EXPECT_EQ(templateIt->second.clipping.mode, mfd::ReticleClipMode::Outer);
    EXPECT_EQ(templateIt->second.clipping.primitiveId, "body");

    ASSERT_EQ(loaded.pages.size(), 1U);
    ASSERT_EQ(loaded.pages.front().staticReticles.size(), 3U);
    EXPECT_EQ(loaded.pages.front().staticReticles[0].clipping.mode, mfd::ReticleClipMode::Inner);
    EXPECT_EQ(loaded.pages.front().staticReticles[0].clipping.primitiveId, "body");
    EXPECT_EQ(loaded.pages.front().staticReticles[1].clipping.mode, mfd::ReticleClipMode::Outer);
    EXPECT_EQ(loaded.pages.front().staticReticles[1].clipping.primitiveId, "body");
    EXPECT_EQ(loaded.pages.front().staticReticles[2].clipping.mode, mfd::ReticleClipMode::Outer);
    EXPECT_EQ(loaded.pages.front().staticReticles[2].clipping.primitiveId, "clip");
}

TEST(JsonLoaderTests, LoadDocumentReportsSemanticClippingDiagnostics)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    std::filesystem::create_directories(workspace.Path() / "reticles");

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "adi_group",
          "layerId": "default",
          "clipping": { "mode": "inner" },
          "elements": [
            { "id": "mask", "type": "circle", "radius": 0.1 }
          ]
        }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(
        {
            try
            {
                static_cast<void>(loader.LoadDocument(pagesFile));
            }
            catch (const std::runtime_error& exception)
            {
                EXPECT_NE(std::string(exception.what()).find("MFD016"), std::string::npos);
                throw;
            }
        },
        std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentParsesExposedPrimitiveFlags)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    WriteTextFile(reticleFolder / "progress_bar.json",
                  R"json({
  "id": "progress_bar",
  "elements": [
    { "id": "fill_bar", "type": "rectangle", "width": 0.22, "height": 0.06, "exposed": true },
    { "id": "frame", "type": "rectangle", "width": 0.28, "height": 0.10, "client": { "expose": true } },
    { "id": "mask", "type": "rectangle", "width": 0.30, "height": 0.12 }
  ]
})json");

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Page2",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        { "id": "progress_bar_01", "template": "progress_bar", "layerId": "default" }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    const mfd::MfdDocument loaded = loader.LoadDocument(pagesFile);

    const auto templateIt = loaded.reticleLibrary.find("progress_bar");
    ASSERT_NE(templateIt, loaded.reticleLibrary.end());
    ASSERT_EQ(templateIt->second.primitives.size(), 3U);
    EXPECT_TRUE(templateIt->second.primitives[0].exposed);
    EXPECT_TRUE(templateIt->second.primitives[1].exposed);
    EXPECT_FALSE(templateIt->second.primitives[2].exposed);
}

TEST(JsonLoaderTests, LoadDocumentParsesPageDynamicReticleBindings)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    WriteTextFile(reticleFolder / "radar_track.json",
                  R"json({
  "id": "radar_track",
  "elements": [
    { "id": "shape", "type": "circle", "radius": 0.05 }
  ]
})json");
    WriteTextFile(reticleFolder / "cursor.json",
                  R"json({
  "id": "cursor",
  "elements": [
    { "id": "shape", "type": "line", "start": [-0.05, 0.0], "end": [0.05, 0.0] }
  ]
})json");

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Radar",
      "layers": [
        { "id": "default" }
      ],
      "dynamicReticleBindings": [
        { "templateId": "radar_track", "layerId": "default", "orderInLayer": 0 },
        { "templateId": "cursor", "layerId": "default", "orderInLayer": 1 }
      ],
      "staticReticles": [
        { "id": "track_1", "template": "radar_track", "layerId": "default" }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    const mfd::MfdDocument loaded = loader.LoadDocument(pagesFile);

    ASSERT_EQ(loaded.pages.size(), 1U);
    ASSERT_EQ(loaded.pages.front().layers.size(), 1U);
    EXPECT_EQ(loaded.pages.front().layers.front().id, "default");
    ASSERT_EQ(loaded.pages.front().dynamicReticleBindings.size(), 2U);
    EXPECT_EQ(loaded.pages.front().dynamicReticleBindings.at(0).templateId, "radar_track");
    EXPECT_EQ(loaded.pages.front().dynamicReticleBindings.at(0).layerId, "default");
    EXPECT_EQ(loaded.pages.front().dynamicReticleBindings.at(0).orderInLayer, 0);
    EXPECT_EQ(loaded.pages.front().dynamicReticleBindings.at(1).templateId, "cursor");
    EXPECT_EQ(loaded.pages.front().dynamicReticleBindings.at(1).layerId, "default");
    EXPECT_EQ(loaded.pages.front().dynamicReticleBindings.at(1).orderInLayer, 1);
}

TEST(JsonLoaderTests, LoadWindowConfigurationRejectsUnknownDefaultPageName)
{
    TemporaryFolder workspace;
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";
    const std::filesystem::path pagesFolder = workspace.Path() / "pages";
    const std::filesystem::path windowFile = workspace.Path() / "window.json";

    WriteTextFile(reticleFolder / "marker.json",
                  R"json({
  "id": "marker",
  "elements": [
    { "id": "shape", "type": "circle", "radius": 0.05 }
  ]
})json");

    WriteTextFile(pagesFolder / "main.json",
                  R"json({
  "name": "Main",
  "layers": [
    { "id": "default" }
  ],
  "staticReticles": [
    { "id": "marker_1", "template": "marker", "layerId": "default" }
  ]
})json");

    WriteTextFile(pagesFolder / "radar.json",
                  R"json({
  "name": "Radar",
  "layers": [
    { "id": "default" }
  ],
  "staticReticles": [
    { "id": "marker_2", "template": "marker", "layerId": "default" }
  ]
})json");

    WriteTextFile(windowFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "defaultPage": "Missing",
  "pages": [
    "pages/main.json",
    "pages/radar.json"
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadWindowConfiguration(windowFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadWindowConfigurationRejectsLegacyPageEntryDefaultFlag)
{
    TemporaryFolder workspace;
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";
    const std::filesystem::path pagesFolder = workspace.Path() / "pages";
    const std::filesystem::path windowFile = workspace.Path() / "window.json";

    WriteTextFile(reticleFolder / "marker.json",
                  R"json({
  "id": "marker",
  "elements": [
    { "id": "shape", "type": "circle", "radius": 0.05 }
  ]
})json");

    WriteTextFile(pagesFolder / "main.json",
                  R"json({
  "name": "Main",
  "layers": [
    { "id": "default" }
  ],
  "staticReticles": [
    { "id": "marker_1", "template": "marker", "layerId": "default" }
  ]
})json");

    WriteTextFile(windowFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    { "file": "pages/main.json", "default": true }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadWindowConfiguration(windowFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentRejectsDuplicateBlinkTypeNamesAfterNormalization)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    WriteTextFile(reticleFolder / "marker.json",
                  R"json({
  "id": "marker",
  "elements": [
    { "id": "shape", "type": "circle", "radius": 0.05 }
  ]
})json");

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "blinkTypes": [
        { "name": "Slow", "durationMs": 1000 },
        { "name": " slow ", "durationMs": 800 }
      ],
      "staticReticles": [
        { "id": "marker_1", "template": "marker", "layerId": "default" }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentRejectsBlinkEnabledWithoutDeclaredPageBlinkTypes)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    WriteTextFile(reticleFolder / "marker.json",
                  R"json({
  "id": "marker",
  "elements": [
    { "id": "shape", "type": "circle", "radius": 0.05 }
  ]
})json");

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        { "id": "marker_1", "template": "marker", "layerId": "default", "blink": true }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentRejectsDuplicateReticleIdsIncludingStrobe)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    WriteTextFile(reticleFolder / "marker.json",
                  R"json({
  "id": "marker",
  "elements": [
    { "id": "shape", "type": "circle", "radius": 0.05 }
  ]
})json");

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        { "id": "dup", "template": "marker", "layerId": "default" }
      ],
      "strobe": {
        "id": "dup",
        "template": "marker"
      }
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentParsesOptionalStrobeMagnetVisualShape)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    WriteTextFile(reticleFolder / "marker.json",
                  R"json({
  "id": "marker",
  "elements": [
    { "id": "shape", "type": "circle", "radius": 0.05 }
  ]
})json");

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "DefaultVisual",
      "layers": [
        { "id": "default" }
      ],
      "strobe": {
        "id": "default_strobe",
        "template": "marker",
        "magnet": {
          "enabled": true,
          "radius": 0.2,
          "strength": 0.5
        }
      }
    },
    {
      "name": "SquareVisual",
      "layers": [
        { "id": "default" }
      ],
      "strobe": {
        "id": "visual_strobe",
        "template": "marker",
        "magnet": {
          "enabled": true,
          "radius": 0.2,
          "visual": {
            "enabled": true,
            "shape": "square",
            "size": 0.11
          }
        }
      }
    }
  ]
})json");

    mfd::JsonLoader loader;
    const mfd::MfdDocument document = loader.LoadDocument(pagesFile);

    ASSERT_EQ(document.pages.size(), 2U);
    ASSERT_TRUE(document.pages[0].strobe.has_value());
    EXPECT_TRUE(document.pages[0].strobe->magnet.enabled);
    EXPECT_FALSE(document.pages[0].strobe->magnet.visualShapeEnabled);
    EXPECT_EQ(document.pages[0].strobe->magnet.visualShape, mfd::StrobeMagnetVisualShape::Circle);

    ASSERT_TRUE(document.pages[1].strobe.has_value());
    EXPECT_TRUE(document.pages[1].strobe->magnet.enabled);
    EXPECT_TRUE(document.pages[1].strobe->magnet.visualShapeEnabled);
    EXPECT_EQ(document.pages[1].strobe->magnet.visualShape, mfd::StrobeMagnetVisualShape::Square);
    EXPECT_FLOAT_EQ(document.pages[1].strobe->magnet.visualShapeSize, 0.11f);
}

TEST(JsonLoaderTests, LoadWindowConfigurationRejectsInvalidPageEntryObject)
{
    TemporaryFolder workspace;
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";
    const std::filesystem::path windowFile = workspace.Path() / "window.json";

    WriteTextFile(reticleFolder / "marker.json",
                  R"json({
  "id": "marker",
  "elements": [
    { "id": "shape", "type": "circle", "radius": 0.05 }
  ]
})json");

    WriteTextFile(windowFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {}
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadWindowConfiguration(windowFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentRejectsTemplateChainingInReticleLibrary)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    WriteTextFile(reticleFolder / "invalid_template.json",
                  R"json({
  "template": "other_template",
  "elements": [
    { "id": "shape", "type": "circle", "radius": 0.05 }
  ]
})json");

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "inline_marker",
          "layerId": "default",
          "elements": [
            { "id": "shape", "type": "circle", "radius": 0.05 }
          ]
        }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadRepositoryCockpitWindowConfigurationSmokeTest)
{
    mfd::JsonLoader loader;
    const std::filesystem::path windowFile = RepositoryRoot() / "assets/windows/demo_pages_cockpit.json";

    const mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(windowFile);

    ASSERT_EQ(loaded.document.pages.size(), 1U);
    const mfd::PageDefinition& page = loaded.document.pages.front();
    EXPECT_EQ(page.name, "Cockpit");
    EXPECT_FALSE(page.staticReticles.empty());
    EXPECT_FALSE(loaded.document.reticleLibrary.empty());
}

TEST(JsonLoaderTests, LoadRepositoryDemoWindowConfigurationSmokeTest)
{
    mfd::JsonLoader loader;
    const std::filesystem::path windowFile = RepositoryRoot() / "assets/windows/demo_pages.json";

    const mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(windowFile);

    ASSERT_EQ(loaded.document.pages.size(), 6U);
    EXPECT_EQ(loaded.document.pages[0].name, "Pfd");
    EXPECT_EQ(loaded.document.pages[1].name, "Navigation");
    EXPECT_EQ(loaded.document.pages[2].name, "AircraftCentric");
    EXPECT_EQ(loaded.document.pages[3].name, "Radar");
    EXPECT_EQ(loaded.document.pages[4].name, "Tactical");
    EXPECT_EQ(loaded.document.pages[5].name, "PictureDemo");
    EXPECT_FALSE(loaded.document.reticleLibrary.empty());

    const auto picturePageIt = std::find_if(
        loaded.document.pages.begin(),
        loaded.document.pages.end(),
        [](const mfd::PageDefinition& page)
        {
            return page.name == "PictureDemo";
        });
    ASSERT_NE(picturePageIt, loaded.document.pages.end());

    const auto pictureReticleIt = std::find_if(
        picturePageIt->staticReticles.begin(),
        picturePageIt->staticReticles.end(),
        [](const mfd::ReticleGroup& reticle)
        {
            return reticle.id == "picture_demo";
        });
    ASSERT_NE(pictureReticleIt, picturePageIt->staticReticles.end());
    EXPECT_TRUE(pictureReticleIt->drawOnTop);

    const auto picturePrimitiveIt = std::find_if(
        pictureReticleIt->primitives.begin(),
        pictureReticleIt->primitives.end(),
        [](const mfd::Primitive& primitive)
        {
            return primitive.id == "demo_picture";
        });
    ASSERT_NE(picturePrimitiveIt, pictureReticleIt->primitives.end());
    EXPECT_EQ(picturePrimitiveIt->type, mfd::PrimitiveType::Image);

    const auto* image = std::get_if<mfd::ImageGeometry>(&picturePrimitiveIt->geometry);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(
        image->file.lexically_normal(),
        (RepositoryRoot() / "assets" / "picture" / "mfdstudio_badge.png").lexically_normal());
}

TEST(JsonLoaderTests, LoadRepositoryMinimalWindowConfigurationMarksRadarAsDefaultPage)
{
    mfd::JsonLoader loader;
    const std::filesystem::path windowFile = RepositoryRoot() / "assets/windows/demo_pages_minimal.json";

    const mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(windowFile);

    std::size_t defaultPageCount = 0U;
    const mfd::PageDefinition* defaultPage = nullptr;
    for (const auto& page : loaded.document.pages)
    {
        if (!page.defaultPage)
        {
            continue;
        }

        ++defaultPageCount;
        defaultPage = &page;
    }

    ASSERT_EQ(defaultPageCount, 1U);
    ASSERT_NE(defaultPage, nullptr);
    EXPECT_EQ(defaultPage->name, "Radar");
}

TEST(JsonLoaderTests, LoadRepositoryInspiredWindowConfigurationSmokeTest)
{
    mfd::JsonLoader loader;
    const std::filesystem::path windowFile = RepositoryRoot() / "assets/windows/demo_pages_inspired.json";

    const mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(windowFile);

    ASSERT_EQ(loaded.document.pages.size(), 6U);
    EXPECT_EQ(loaded.document.pages[0].name, "HSD_INSPIRED");
    EXPECT_EQ(loaded.document.pages[1].name, "FCR_INSPIRED");
    EXPECT_EQ(loaded.document.pages[2].name, "HAD_INSPIRED");
    EXPECT_EQ(loaded.document.pages[3].name, "TGP_INSPIRED");
    EXPECT_EQ(loaded.document.pages[4].name, "SMS_INSPIRED");
    EXPECT_EQ(loaded.document.pages[5].name, "FLCS_INSPIRED");

    std::size_t defaultPageCount = 0U;
    const mfd::PageDefinition* defaultPage = nullptr;
    for (const auto& page : loaded.document.pages)
    {
        if (!page.defaultPage)
        {
            continue;
        }

        ++defaultPageCount;
        defaultPage = &page;
    }

    ASSERT_EQ(defaultPageCount, 1U);
    ASSERT_NE(defaultPage, nullptr);
    EXPECT_EQ(defaultPage->name, "HSD_INSPIRED");
    EXPECT_FALSE(loaded.document.reticleLibrary.empty());
}

TEST(JsonLoaderTests, LoadWindowConfigurationRejectsNonPositiveUdpPacketSizes)
{
    TemporaryFolder workspace;
    const std::filesystem::path windowFile = workspace.Path() / "window.json";
    const std::filesystem::path pageFile = workspace.Path() / "page.json";

    WriteTextFile(pageFile,
                  R"json({
  "name": "Main",
  "layers": [
    { "id": "default" }
  ],
  "staticReticles": []
})json");
    WriteTextFile(windowFile,
                  R"json({
  "commands": {
    "udp": {
      "enabled": true,
      "maxPacketSize": 0
    }
  },
  "pages": ["page.json"]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadWindowConfiguration(windowFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadWindowConfigurationRejectsUdpPacketSizesOutsideSupportedRange)
{
    TemporaryFolder workspace;
    const std::filesystem::path highWindowFile = workspace.Path() / "window_high.json";
    const std::filesystem::path lowWindowFile = workspace.Path() / "window_low.json";
    const std::filesystem::path pageFile = workspace.Path() / "page.json";

    WriteTextFile(pageFile,
                  R"json({
  "name": "Main",
  "layers": [
    { "id": "default" }
  ],
  "staticReticles": []
})json");
    WriteTextFile(highWindowFile,
                  R"json({
  "commands": {
    "udp": {
      "enabled": true,
      "maxPacketSize": 70000
    }
  },
  "pages": ["page.json"]
})json");
    WriteTextFile(lowWindowFile,
                  R"json({
  "feedback": {
    "udp": {
      "enabled": true,
      "maxPacketSize": 32
    }
  },
  "pages": ["page.json"]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadWindowConfiguration(highWindowFile), std::runtime_error);
    EXPECT_THROW(loader.LoadWindowConfiguration(lowWindowFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadWindowConfigurationRejectsWindowMetricsOutsideSignedIntRange)
{
    TemporaryFolder workspace;
    const std::filesystem::path windowFile = workspace.Path() / "window.json";
    const std::filesystem::path pageFile = workspace.Path() / "page.json";

    WriteTextFile(pageFile,
                  R"json({
  "name": "Main",
  "layers": [
    { "id": "default" }
  ],
  "staticReticles": []
})json");
    WriteTextFile(windowFile,
                  R"json({
  "width": 3000000000,
  "height": 600,
  "pages": ["page.json"]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadWindowConfiguration(windowFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadWindowConfigurationRejectsNonPositiveOrOversizedWindowDimensions)
{
    TemporaryFolder workspace;
    const std::filesystem::path invalidSizeFile = workspace.Path() / "window_invalid_size.json";
    const std::filesystem::path oversizedFile = workspace.Path() / "window_oversized.json";
    const std::filesystem::path pageFile = workspace.Path() / "page.json";

    WriteTextFile(pageFile,
                  R"json({
  "name": "Main",
  "layers": [
    { "id": "default" }
  ],
  "staticReticles": []
})json");

    WriteTextFile(invalidSizeFile,
                  R"json({
  "width": 0,
  "height": -1,
  "pages": ["page.json"]
})json");

    WriteTextFile(oversizedFile,
                  R"json({
  "width": 16385,
  "height": 600,
  "pages": ["page.json"]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadWindowConfiguration(invalidSizeFile), std::runtime_error);
    EXPECT_THROW(loader.LoadWindowConfiguration(oversizedFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentRejectsTriangleWithMoreThanThreePoints)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";

    WriteTextFile(pagesFile,
                  R"json({
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "shape",
          "layerId": "default",
          "elements": [
            {
              "id": "tri",
              "type": "triangle",
              "points": [[0,0], [1,0], [0,1], [1,1]]
            }
          ]
        }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentRejectsBezierWithAmbiguousPointFields)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";

    WriteTextFile(pagesFile,
                  R"json({
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "shape",
          "layerId": "default",
          "elements": [
            {
              "id": "curve",
              "type": "bezier",
              "controlPoints": [[0,0], [1,0], [1,1]],
              "points": [[0,0], [0.5,1], [1,1]]
            }
          ]
        }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentParsesArcPrimitiveWithAnglesAndSegments)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    std::filesystem::create_directories(reticleFolder);

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "shape",
          "layerId": "default",
          "elements": [
            {
              "id": "scan_arc",
              "type": "arc",
              "radius": 0.24,
              "startAngle": -45.0,
              "endAngleDegrees": 135.0,
              "segments": 40
            }
          ]
        }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    const mfd::MfdDocument document = loader.LoadDocument(pagesFile);

    ASSERT_EQ(document.pages.size(), 1U);
    ASSERT_EQ(document.pages.front().staticReticles.size(), 1U);
    ASSERT_EQ(document.pages.front().staticReticles.front().primitives.size(), 1U);
    const mfd::Primitive& primitive = document.pages.front().staticReticles.front().primitives.front();
    EXPECT_EQ(primitive.type, mfd::PrimitiveType::Arc);
    const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry);
    ASSERT_NE(arc, nullptr);
    EXPECT_FLOAT_EQ(arc->radius, 0.24f);
    EXPECT_FLOAT_EQ(arc->startAngleDegrees, -45.0f);
    EXPECT_FLOAT_EQ(arc->endAngleDegrees, 135.0f);
    EXPECT_EQ(arc->segments, 40);
}

TEST(JsonLoaderTests, LoadDocumentParsesPrimitiveLineStylesFromDirectAndNestedFields)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    std::filesystem::create_directories(reticleFolder);

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "shape",
          "layerId": "default",
          "elements": [
            {
              "id": "solid_line",
              "type": "line",
              "start": [-0.20, 0.0],
              "end": [0.20, 0.0]
            },
            {
              "id": "dotted_ring",
              "type": "ring",
              "radius": 0.18,
              "lineStyle": "dotted"
            },
            {
              "id": "dashed_arc",
              "type": "arc",
              "radius": 0.24,
              "startAngle": 0.0,
              "endAngle": 180.0,
              "style": {
                "lineStyle": "dashed"
              }
            }
          ]
        }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    const mfd::MfdDocument document = loader.LoadDocument(pagesFile);

    ASSERT_EQ(document.pages.size(), 1U);
    ASSERT_EQ(document.pages.front().staticReticles.size(), 1U);
    const auto& primitives = document.pages.front().staticReticles.front().primitives;
    ASSERT_EQ(primitives.size(), 3U);
    EXPECT_EQ(primitives[0].style.lineStyle, mfd::LineStyle::Solid);
    EXPECT_EQ(primitives[1].style.lineStyle, mfd::LineStyle::Dotted);
    EXPECT_EQ(primitives[2].style.lineStyle, mfd::LineStyle::Dashed);
}

TEST(JsonLoaderTests, LoadDocumentResolvesImagePrimitivePathsAndDrawOnTop)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path imageFolder = workspace.Path() / "picture";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    WriteTextFile(imageFolder / "badge.png", "not_a_real_png_but_a_valid_path_for_loader_tests");
    std::filesystem::create_directories(reticleFolder);
    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Images",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "badge_layer",
          "layerId": "default",
          "drawOnTop": true,
          "elements": [
            {
              "id": "badge",
              "type": "image",
              "file": "picture/badge.png",
              "size": [0.30, 0.12]
            }
          ]
        }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    const mfd::MfdDocument document = loader.LoadDocument(pagesFile);

    ASSERT_EQ(document.pages.size(), 1U);
    ASSERT_EQ(document.pages.front().staticReticles.size(), 1U);
    const mfd::ReticleGroup& reticle = document.pages.front().staticReticles.front();
    EXPECT_TRUE(reticle.drawOnTop);
    ASSERT_EQ(reticle.primitives.size(), 1U);
    const mfd::Primitive& primitive = reticle.primitives.front();
    EXPECT_EQ(primitive.type, mfd::PrimitiveType::Image);
    const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(image->file.lexically_normal(), (imageFolder / "badge.png").lexically_normal());
    EXPECT_FLOAT_EQ(image->width, 0.30f);
    EXPECT_FLOAT_EQ(image->height, 0.12f);
}

TEST(JsonLoaderTests, LoadDocumentKeepsNonZeroRingBandWhenOnlyOuterRadiusIsProvided)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";

    WriteTextFile(reticleFolder / "small_ring.json",
                  R"json({
  "id": "small_ring",
  "elements": [
    {
      "id": "ring_small",
      "type": "ring",
      "radius": 0.01
    }
  ]
})json");

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "shape",
          "template": "small_ring",
          "layerId": "default"
        }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    const mfd::MfdDocument document = loader.LoadDocument(pagesFile);

    ASSERT_EQ(document.pages.size(), 1U);
    ASSERT_EQ(document.pages.front().staticReticles.size(), 1U);
    ASSERT_EQ(document.pages.front().staticReticles.front().primitives.size(), 1U);
    const mfd::Primitive& primitive = document.pages.front().staticReticles.front().primitives.front();
    const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry);
    ASSERT_NE(ring, nullptr);
    EXPECT_LT(ring->innerRadius, ring->outerRadius);
}

TEST(JsonLoaderTests, LoadDocumentRejectsConflictingTransformAliases)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";

    WriteTextFile(pagesFile,
                  R"json({
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "shape",
          "layerId": "default",
          "position": [0.1, 0.2],
          "at": [-0.3, 0.4],
          "elements": [
            { "id": "circle", "type": "circle", "radius": 0.05 }
          ]
        }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadWindowConfigurationRejectsFloatingPointUdpPort)
{
    TemporaryFolder workspace;
    const std::filesystem::path windowFile = workspace.Path() / "window.json";
    const std::filesystem::path pageFile = workspace.Path() / "page.json";

    WriteTextFile(pageFile,
                  R"json({
  "name": "Main",
  "layers": [
    { "id": "default" }
  ],
  "staticReticles": []
})json");

    WriteTextFile(windowFile,
                  R"json({
  "commands": {
    "udp": {
      "enabled": true,
      "port": 48000.0
    }
  },
  "pages": ["page.json"]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadWindowConfiguration(windowFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentRejectsVec2ArraysWithExtraCoordinates)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";

    WriteTextFile(pagesFile,
                  R"json({
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "shape",
          "layerId": "default",
          "elements": [
            {
              "id": "line",
              "type": "line",
              "start": [0.0, 0.0, 1.0],
              "end": [1.0, 1.0]
            }
          ]
        }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentRejectsPolylinePointListsExceedingRuntimeBudget)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    std::ostringstream points;
    for (std::size_t index = 0; index < 2049U; ++index)
    {
        if (index > 0U)
        {
            points << ',';
        }

        points << '[' << static_cast<double>(index) * 0.001 << ",0.0]";
    }

    const std::string document =
        std::string(R"json({
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "shape",
          "layerId": "default",
          "elements": [
            {
              "id": "poly",
              "type": "polyline",
              "points": [)json") +
        points.str() +
        R"json(]
            }
          ]
        }
      ]
    }
  ]
})json";

    WriteTextFile(pagesFile, document);

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentRejectsPrimitiveSegmentsOutsideRuntimeBudget)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";

    WriteTextFile(pagesFile,
                  R"json({
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": [
        {
          "id": "shape",
          "layerId": "default",
          "elements": [
            {
              "id": "scan_arc",
              "type": "arc",
              "radius": 0.24,
              "segments": 5000
            }
          ]
        }
      ]
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentRejectsNonPositivePageZoom)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";

    WriteTextFile(pagesFile,
                  R"json({
  "pages": [
    {
      "name": "Main",
      "layers": [
        { "id": "default" }
      ],
      "view": {
        "center": [0.0, 0.0],
        "zoom": 0.0
      },
      "staticReticles": []
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}

TEST(JsonLoaderTests, LoadDocumentParsesPageTitleDisplayChrome)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";
    std::filesystem::create_directories(reticleFolder);

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Main",
      "title": "Main page",
      "titleDisplay": {
        "visible": false,
        "at": [-1.1, 0.8],
        "scale": [1.5, 0.9],
        "stroke": "#FFAA33FF",
        "lineWidth": 0.0075,
        "lineStyle": "dashed",
        "decoration": "frame"
      },
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": []
    }
  ]
})json");

    mfd::JsonLoader loader;
    const mfd::MfdDocument document = loader.LoadDocument(pagesFile);

    ASSERT_EQ(document.pages.size(), 1U);
    const mfd::PageTitleDisplayDefinition& titleDisplay = document.pages.front().titleDisplay;
    EXPECT_FALSE(titleDisplay.visible);
    EXPECT_FLOAT_EQ(titleDisplay.transform.position.x, -1.1f);
    EXPECT_FLOAT_EQ(titleDisplay.transform.position.y, 0.8f);
    EXPECT_FLOAT_EQ(titleDisplay.transform.scale.x, 1.5f);
    EXPECT_FLOAT_EQ(titleDisplay.transform.scale.y, 0.9f);
    EXPECT_EQ(titleDisplay.color.r, 255);
    EXPECT_EQ(titleDisplay.color.g, 170);
    EXPECT_EQ(titleDisplay.color.b, 51);
    EXPECT_EQ(titleDisplay.color.a, 255);
    EXPECT_FLOAT_EQ(titleDisplay.lineWidth, 0.0075f);
    EXPECT_EQ(titleDisplay.lineStyle, mfd::LineStyle::Dashed);
    EXPECT_EQ(titleDisplay.decoration, mfd::PageTitleDecoration::Frame);
}

TEST(JsonLoaderTests, LoadDocumentRejectsNonBooleanPageTitleDisplayVisibility)
{
    TemporaryFolder workspace;
    const std::filesystem::path pagesFile = workspace.Path() / "pages.json";
    const std::filesystem::path reticleFolder = workspace.Path() / "reticles";
    std::filesystem::create_directories(reticleFolder);

    WriteTextFile(pagesFile,
                  R"json({
  "reticleLibraryFolder": "reticles",
  "pages": [
    {
      "name": "Main",
      "title": "Main page",
      "titleDisplay": {
        "visible": "false"
      },
      "layers": [
        { "id": "default" }
      ],
      "staticReticles": []
    }
  ]
})json");

    mfd::JsonLoader loader;
    EXPECT_THROW(loader.LoadDocument(pagesFile), std::runtime_error);
}
