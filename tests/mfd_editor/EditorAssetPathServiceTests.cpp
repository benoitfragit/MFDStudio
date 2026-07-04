/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests validating editor asset path defaults.
 */

#include "EditorAssetPathService.h"

#include <chrono>
#include <filesystem>
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
                std::filesystem::path("mfd_editor_asset_path_tests_" + std::to_string(ticks));
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
} // namespace

TEST(EditorAssetPathServiceTests, ConfiguredAssetDirectoryStripsLeadingAssetRootPrefix)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "authored_assets";

    const editor::EditorAssetPathService service(assetRoot);

    EXPECT_TRUE(service.HasConfiguredAssetDirectory());
    EXPECT_EQ(service.DefaultAssetPath("examples/demo/assets/windows/new_window.json"),
              (assetRoot / "windows" / "new_window.json").lexically_normal());
    EXPECT_EQ(service.DefaultAssetPath("examples/demo/assets/reticles"),
              (assetRoot / "reticles").lexically_normal());
}

TEST(EditorAssetPathServiceTests, DefaultSiblingAssetFileUsesConfiguredAssetDirectoryWithoutAnchor)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const editor::EditorAssetPathService service(assetRoot);

    EXPECT_EQ(service.DefaultSiblingAssetFile({}, "pages", "radar.json"),
              (assetRoot / "pages" / "radar.json").lexically_normal());
}

TEST(EditorAssetPathServiceTests, CurrentPageImportTargetFolderUsesExistingPageFolderWhenConsistent)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "assets";
    const std::filesystem::path pageFolder = assetRoot / "pages";

    editor::EditorFileLayout files;
    files.pageFiles.push_back(pageFolder / "page1.json");
    files.pageFiles.push_back(pageFolder / "page2.json");

    const editor::EditorAssetPathService service(assetRoot);

    EXPECT_EQ(service.CurrentPageImportTargetFolder(assetRoot / "windows" / "window.json", files),
              pageFolder.lexically_normal());
}

TEST(EditorAssetPathServiceTests, CurrentPageImportTargetFolderFallsBackToConfiguredPagesFolder)
{
    ScopedTempDir tempDir;
    const std::filesystem::path assetRoot = tempDir.Path() / "authored";
    const editor::EditorAssetPathService service(assetRoot);

    EXPECT_EQ(service.CurrentPageImportTargetFolder({}, {}),
              (assetRoot / "pages").lexically_normal());
}

TEST(EditorAssetPathServiceTests, JsonFileNameOrFallbackAddsJsonExtension)
{
    EXPECT_EQ(editor::EditorAssetPathService::JsonFileNameOrFallback("radar", "fallback.json"),
              std::filesystem::path("radar.json"));
    EXPECT_EQ(editor::EditorAssetPathService::JsonFileNameOrFallback(std::filesystem::path {}, "fallback.json"),
              std::filesystem::path("fallback.json"));
}

TEST(EditorAssetPathServiceTests, ExecStagingPathDetectionIsCaseInsensitive)
{
    const std::filesystem::path stagedPath =
        std::filesystem::path("build") / "_exec" / "assets" / "windows" / "demo.json";

    EXPECT_TRUE(editor::EditorAssetPathService::IsExecStagingPath(stagedPath));
    EXPECT_FALSE(editor::EditorAssetPathService::IsExecStagingPath("examples/demo/assets/windows/demo.json"));
}
