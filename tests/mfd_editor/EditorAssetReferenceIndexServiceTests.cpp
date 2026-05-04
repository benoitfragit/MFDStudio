/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the editor asset reference index service.
 */

#include "EditorAssetReferenceIndexService.h"

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
                std::filesystem::path("mfd_asset_reference_index_tests_" + std::to_string(ticks));
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

void WriteTextFile(const std::filesystem::path& path, const std::string& content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    ASSERT_TRUE(output.is_open()) << path.string();
    output << content;
}

void TouchFile(const std::filesystem::path& path)
{
    WriteTextFile(path, "stub");
}

const editor::ReticleReference* FindReference(const editor::AssetReferenceIndex& index,
                                              const editor::ReticleReferenceKind kind,
                                              const std::filesystem::path& ownerFile,
                                              const std::string& reticleId = {},
                                              const std::string& templateId = {},
                                              const std::string& primitiveId = {})
{
    const std::filesystem::path normalizedOwnerFile = std::filesystem::absolute(ownerFile).lexically_normal();
    for (const auto& reference : index.reticles)
    {
        if (reference.kind == kind &&
            reference.ownerFile == normalizedOwnerFile &&
            reference.reticleId == reticleId &&
            reference.templateId == templateId &&
            reference.primitiveId == primitiveId)
        {
            return &reference;
        }
    }

    return nullptr;
}

const editor::MissingAssetReference* FindMissingAsset(const editor::AssetReferenceIndex& index,
                                                      const editor::MissingAssetKind kind,
                                                      const std::filesystem::path& ownerFile,
                                                      const std::filesystem::path& referencedPath = {},
                                                      const std::string& templateId = {})
{
    const std::filesystem::path normalizedOwnerFile = std::filesystem::absolute(ownerFile).lexically_normal();
    const std::filesystem::path normalizedReferencedPath =
        referencedPath.empty() ? std::filesystem::path {} : std::filesystem::absolute(referencedPath).lexically_normal();

    for (const auto& missing : index.missingAssets)
    {
        if (missing.kind == kind &&
            missing.ownerFile == normalizedOwnerFile &&
            missing.referencedPath == normalizedReferencedPath &&
            missing.templateId == templateId)
        {
            return &missing;
        }
    }

    return nullptr;
}
} // namespace

TEST(AssetReferenceIndexServiceTests, BuildIndexTracksWindowPagesTemplatesImagesAndWindowAssets)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path windowFile = assetRoot / "windows" / "demo.json";
    const std::filesystem::path pageFile = assetRoot / "pages" / "radar.json";
    const std::filesystem::path radarTemplateFile = assetRoot / "reticles" / "radar_track.json";
    const std::filesystem::path strobeTemplateFile = assetRoot / "reticles" / "strobe_cursor.json";
    const std::filesystem::path pageImageFile = assetRoot / "picture" / "page_badge.png";
    const std::filesystem::path templateImageFile = assetRoot / "picture" / "template_badge.png";
    const std::filesystem::path fontFile = assetRoot / "fonts" / "main.ttf";
    const std::filesystem::path iconFile = assetRoot / "branding" / "icon.png";

    WriteTextFile(windowFile,
                  R"({
  "title": "Demo",
  "reticleLibraryFolder": "../reticles",
  "fontFile": "../fonts/main.ttf",
  "iconFile": "../branding/icon.png",
  "pages": [
    "../pages/sub/../radar.json"
  ],
  "defaultPage": "Radar"
})");
    WriteTextFile(pageFile,
                  R"({
  "name": "Radar",
  "_editor": {
    "dynamicReticleTemplates": [
      "radar_track"
    ]
  },
  "staticReticles": [
    {
      "id": "track_alpha",
      "template": "radar_track"
    },
    {
      "id": "inline_badge",
      "elements": [
        {
          "id": "page_picture",
          "type": "image",
          "file": "../picture/./page_badge.png"
        }
      ]
    }
  ],
  "strobe": {
    "id": "cursor",
    "template": "strobe_cursor"
  }
})");
    WriteTextFile(radarTemplateFile,
                  R"({
  "id": "radar_track",
  "elements": [
    {
      "id": "template_picture",
      "type": "image",
      "file": "../picture/template_badge.png"
    }
  ]
})");
    WriteTextFile(strobeTemplateFile, R"({"id":"strobe_cursor","elements":[]})");
    TouchFile(pageImageFile);
    TouchFile(templateImageFile);
    TouchFile(fontFile);
    TouchFile(iconFile);

    editor::AssetReferenceIndexService service;
    const editor::AssetReferenceIndex index = service.BuildIndex(editor::AssetReferenceIndexRequest {assetRoot});

    ASSERT_TRUE(index.errors.empty());
    ASSERT_EQ(index.pages.size(), 1U);
    EXPECT_EQ(index.pages[0].windowFile, std::filesystem::absolute(windowFile).lexically_normal());
    EXPECT_EQ(index.pages[0].pageFile, std::filesystem::absolute(pageFile).lexically_normal());
    EXPECT_EQ(index.pages[0].pageName, "Radar");
    EXPECT_TRUE(index.pages[0].defaultPage);

    const editor::ReticleReference* pageTemplateReference =
        FindReference(index, editor::ReticleReferenceKind::PageReticleTemplate, pageFile, "track_alpha", "radar_track");
    ASSERT_NE(pageTemplateReference, nullptr);
    EXPECT_EQ(pageTemplateReference->targetFile, std::filesystem::absolute(radarTemplateFile).lexically_normal());

    const editor::ReticleReference* strobeTemplateReference =
        FindReference(index, editor::ReticleReferenceKind::PageStrobeTemplate, pageFile, "cursor", "strobe_cursor");
    ASSERT_NE(strobeTemplateReference, nullptr);
    EXPECT_EQ(strobeTemplateReference->targetFile, std::filesystem::absolute(strobeTemplateFile).lexically_normal());

    const editor::ReticleReference* pageDynamicTemplateReference =
        FindReference(index, editor::ReticleReferenceKind::PageDynamicTemplate, pageFile, {}, "radar_track");
    ASSERT_NE(pageDynamicTemplateReference, nullptr);
    EXPECT_EQ(pageDynamicTemplateReference->targetFile, std::filesystem::absolute(radarTemplateFile).lexically_normal());

    const editor::ReticleReference* pageImageReference = FindReference(
        index, editor::ReticleReferenceKind::ReticleImageAsset, pageFile, "inline_badge", {}, "page_picture");
    ASSERT_NE(pageImageReference, nullptr);
    EXPECT_EQ(pageImageReference->targetFile, std::filesystem::absolute(pageImageFile).lexically_normal());

    const editor::ReticleReference* templateImageReference = FindReference(
        index, editor::ReticleReferenceKind::ReticleImageAsset, radarTemplateFile, "radar_track", "radar_track", "template_picture");
    ASSERT_NE(templateImageReference, nullptr);
    EXPECT_EQ(templateImageReference->targetFile, std::filesystem::absolute(templateImageFile).lexically_normal());

    const editor::ReticleReference* fontReference =
        FindReference(index, editor::ReticleReferenceKind::WindowFontAsset, windowFile);
    ASSERT_NE(fontReference, nullptr);
    EXPECT_EQ(fontReference->targetFile, std::filesystem::absolute(fontFile).lexically_normal());

    const editor::ReticleReference* iconReference =
        FindReference(index, editor::ReticleReferenceKind::WindowIconAsset, windowFile);
    ASSERT_NE(iconReference, nullptr);
    EXPECT_EQ(iconReference->targetFile, std::filesystem::absolute(iconFile).lexically_normal());

    EXPECT_TRUE(index.missingAssets.empty());
}

TEST(AssetReferenceIndexServiceTests, BuildIndexDetectsMissingPagesAndTemplates)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path windowFile = assetRoot / "windows" / "demo.json";
    const std::filesystem::path pageFile = assetRoot / "pages" / "radar.json";
    const std::filesystem::path missingPageFile = assetRoot / "pages" / "missing.json";

    WriteTextFile(windowFile,
                  R"({
  "reticleLibraryFolder": "../reticles",
  "pages": [
    "../pages/radar.json",
    "../pages/missing.json"
  ]
})");
    WriteTextFile(pageFile,
                  R"({
  "name": "Radar",
  "staticReticles": [
    {
      "id": "track_alpha",
      "template": "missing_template"
    }
  ]
})");

    editor::AssetReferenceIndexService service;
    const editor::AssetReferenceIndex index = service.BuildIndex(editor::AssetReferenceIndexRequest {assetRoot});

    EXPECT_NE(FindMissingAsset(index, editor::MissingAssetKind::PageFile, windowFile, missingPageFile), nullptr);

    const editor::MissingAssetReference* missingTemplate =
        FindMissingAsset(index, editor::MissingAssetKind::ReticleTemplate, pageFile, {}, "missing_template");
    ASSERT_NE(missingTemplate, nullptr);
    EXPECT_EQ(missingTemplate->reticleId, "track_alpha");
    EXPECT_EQ(missingTemplate->pageName, "Radar");
}

TEST(AssetReferenceIndexServiceTests, BuildIndexScansExecLikeAnyOtherFolder)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path liveWindowFile = assetRoot / "windows" / "live.json";
    const std::filesystem::path livePageFile = assetRoot / "pages" / "live.json";
    const std::filesystem::path execWindowFile = assetRoot / "_Exec" / "windows" / "staged.json";
    const std::filesystem::path execPageFile = assetRoot / "_Exec" / "pages" / "staged.json";

    WriteTextFile(liveWindowFile, R"({"pages":["../pages/live.json"]})");
    WriteTextFile(livePageFile, R"({"name":"Live","staticReticles":[]})");
    WriteTextFile(execWindowFile, R"({"pages":["../pages/staged.json"]})");
    WriteTextFile(execPageFile, R"({"name":"Staged","staticReticles":[]})");

    editor::AssetReferenceIndexService service;

    const editor::AssetReferenceIndex defaultIndex = service.BuildIndex(editor::AssetReferenceIndexRequest {assetRoot});
    ASSERT_EQ(defaultIndex.pages.size(), 2U);

    const editor::AssetReferenceIndex includeExecIndex = service.BuildIndex(editor::AssetReferenceIndexRequest {assetRoot});
    ASSERT_EQ(includeExecIndex.pages.size(), 2U);
}

TEST(AssetReferenceIndexServiceTests, BuildIndexReportsInvalidJsonWithoutCrashing)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path windowFile = assetRoot / "windows" / "demo.json";
    const std::filesystem::path validPageFile = assetRoot / "pages" / "valid.json";
    const std::filesystem::path invalidTemplateFile = assetRoot / "reticles" / "broken.json";

    WriteTextFile(windowFile, R"({"pages":["../pages/valid.json"]})");
    WriteTextFile(validPageFile, R"({"name":"Valid","staticReticles":[]})");
    WriteTextFile(invalidTemplateFile, R"({"id":"broken","elements":[})");

    editor::AssetReferenceIndexService service;
    const editor::AssetReferenceIndex index = service.BuildIndex(editor::AssetReferenceIndexRequest {assetRoot});

    ASSERT_EQ(index.pages.size(), 1U);
    ASSERT_FALSE(index.errors.empty());

    bool foundInvalidTemplateError = false;
    for (const auto& error : index.errors)
    {
        if (error.file == std::filesystem::absolute(invalidTemplateFile).lexically_normal())
        {
            foundInvalidTemplateError = true;
            break;
        }
    }

    EXPECT_TRUE(foundInvalidTemplateError);
}
