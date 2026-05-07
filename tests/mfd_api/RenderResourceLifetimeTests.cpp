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
