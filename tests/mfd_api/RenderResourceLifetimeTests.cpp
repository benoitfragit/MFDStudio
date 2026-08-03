/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Regression coverage for render-resource cleanup around window shutdown.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <raylib.h>

#include "ImageTextureCache.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/render/MfdRenderer.h"
#include "mfd/runtime/SceneRegistry.h"

namespace
{
class TemporaryPngFile
{
public:
    TemporaryPngFile()
    {
        const std::filesystem::path folder =
            std::filesystem::temp_directory_path() / "mfd_render_resource_lifetime_tests";
        std::filesystem::create_directories(folder);
        path_ = folder / "renderer_shutdown.png";

        Image image = GenImageColor(2, 2, WHITE);
        const bool exported = ExportImage(image, path_.string().c_str());
        UnloadImage(image);

        if (!exported)
        {
            throw std::runtime_error("Unable to export the temporary PNG used by render lifetime tests.");
        }
    }

    ~TemporaryPngFile()
    {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_ {};
};

class TemporaryPngCollection
{
public:
    explicit TemporaryPngCollection(const std::size_t fileCount)
    {
        const auto uniqueSuffix = std::chrono::steady_clock::now().time_since_epoch().count();
        folder_ = std::filesystem::temp_directory_path() /
                  ("mfd_image_texture_cache_" + std::to_string(uniqueSuffix));
        std::filesystem::create_directories(folder_);

        Image image = GenImageColor(2, 2, WHITE);
        const bool exported = ExportImage(image, Path(0).string().c_str());
        UnloadImage(image);
        if (!exported)
        {
            throw std::runtime_error("Unable to export the first texture-cache test image.");
        }

        for (std::size_t fileIndex = 1; fileIndex < fileCount; ++fileIndex)
        {
            std::filesystem::copy_file(Path(0), Path(fileIndex));
        }
    }

    ~TemporaryPngCollection()
    {
        std::error_code error;
        std::filesystem::remove_all(folder_, error);
    }

    [[nodiscard]] std::filesystem::path Path(const std::size_t fileIndex) const
    {
        return folder_ / ("image_" + std::to_string(fileIndex) + ".png");
    }

private:
    std::filesystem::path folder_ {};
};

mfd::SceneRegistry MakeSceneWithImagePrimitive(const std::filesystem::path& imageFile)
{
    mfd::Primitive primitive;
    primitive.id = "badge";
    primitive.type = mfd::PrimitiveType::Image;
    primitive.geometry = mfd::ImageGeometry {imageFile, 0.35f, 0.35f};

    mfd::ReticleGroup reticle;
    reticle.id = "status";
    reticle.primitives.push_back(std::move(primitive));

    mfd::PageDefinition page;
    page.name = "Main";
    page.normalizedName = mfd::NormalizePageName(page.name);
    page.title = page.name;
    page.defaultPage = true;
    page.staticReticles.push_back(std::move(reticle));

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    mfd::SceneRegistry scene;
    scene.LoadDocument(std::move(document));
    scene.SetActivePage("Main");
    scene.SetWindowBrightness(0.75f);
    return scene;
}
} // namespace

/**
 * @brief Verifies the texture cache can be cleared safely after the raylib window closes.
 */
TEST(RenderResourceLifetimeTests, ImageTextureCacheClearAfterWindowCloseDoesNotRequireLiveContext)
{
    const TemporaryPngFile imageFile;

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(32, 32, "mfd_render_resource_lifetime_cache");
    ASSERT_TRUE(IsWindowReady());

    mfd::ImageTextureCache cache;
    const Texture2D* texture = cache.Resolve(imageFile.Path());
    ASSERT_NE(texture, nullptr);
    ASSERT_NE(texture->id, 0U);

    CloseWindow();

    cache.Clear();
    SUCCEED();
}

/**
 * @brief Verifies one renderer can release cached GPU resources even if its destructor runs after `CloseWindow()`.
 */
TEST(RenderResourceLifetimeTests, RendererDestructionAfterWindowCloseDoesNotRequireLiveContext)
{
    const TemporaryPngFile imageFile;

    {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(64, 64, "mfd_render_resource_lifetime_renderer");
        ASSERT_TRUE(IsWindowReady());

        mfd::MfdRenderer renderer;
        mfd::SceneRegistry scene = MakeSceneWithImagePrimitive(imageFile.Path());

        BeginDrawing();
        ClearBackground(BLACK);
        renderer.DrawActivePage(scene, 32, 32);
        EndDrawing();

        CloseWindow();
    }

    SUCCEED();
}

/**
 * @brief Verifies least-recently-used images are evicted once the entry budget is exhausted.
 */
TEST(RenderResourceLifetimeTests, ImageTextureCacheEvictsLeastRecentlyUsedEntry)
{
    constexpr std::size_t kImageCountBeyondCacheBudget = 257;
    const TemporaryPngCollection imageFiles(kImageCountBeyondCacheBudget);

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(32, 32, "mfd_render_resource_lifetime_eviction");
    ASSERT_TRUE(IsWindowReady());

    mfd::ImageTextureCache cache;
    for (std::size_t fileIndex = 0; fileIndex < (kImageCountBeyondCacheBudget - 1); ++fileIndex)
    {
        ASSERT_NE(cache.Resolve(imageFiles.Path(fileIndex)), nullptr);
    }

    ASSERT_NE(cache.Resolve(imageFiles.Path(0)), nullptr);
    ASSERT_NE(cache.Resolve(imageFiles.Path(kImageCountBeyondCacheBudget - 1)), nullptr);

    ASSERT_TRUE(std::filesystem::remove(imageFiles.Path(0)));
    ASSERT_TRUE(std::filesystem::remove(imageFiles.Path(1)));
    EXPECT_NE(cache.Resolve(imageFiles.Path(0)), nullptr);
    EXPECT_EQ(cache.Resolve(imageFiles.Path(1)), nullptr);

    cache.Clear();
    CloseWindow();
}
