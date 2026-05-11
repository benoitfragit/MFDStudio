/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Hidden-window render tests covering stencil clipping behaviour.
 */

#include <gtest/gtest.h>

#include <raylib.h>

#include "mfd/model/PageDefinition.h"
#include "mfd/model/Reticle.h"
#include "mfd/render/MfdRenderer.h"
#include "mfd/render/OpenGlFramebufferReader.h"
#include "mfd/runtime/SceneRegistry.h"

namespace
{
constexpr int kRenderSize = 96;

mfd::ColorRgba Green() noexcept
{
    return {0, 255, 0, 255};
}

mfd::Primitive MakeFilledRectangle()
{
    mfd::Primitive primitive;
    primitive.id = "background";
    primitive.type = mfd::PrimitiveType::Rectangle;
    primitive.geometry = mfd::RectangleGeometry {1.60f, 1.60f};
    primitive.style.color = Green();
    primitive.style.fillColor = Green();
    primitive.style.filled = true;
    return primitive;
}

mfd::Primitive MakeInvisibleCircleMask()
{
    mfd::Primitive primitive;
    primitive.id = "mask";
    primitive.type = mfd::PrimitiveType::Circle;
    primitive.geometry = mfd::CircleGeometry {0.35f};
    primitive.style.visible = false;
    return primitive;
}

mfd::SceneRegistry MakeMaskedScene(const mfd::ReticleClipMode clipMode)
{
    mfd::ReticleGroup backdrop;
    backdrop.id = "backdrop";
    backdrop.primitives.push_back(MakeFilledRectangle());

    mfd::ReticleGroup reticle;
    reticle.id = "mask";
    reticle.clipping.mode = clipMode;
    reticle.clipping.primitiveId = "mask";
    reticle.primitives.push_back(MakeInvisibleCircleMask());

    mfd::PageDefinition page;
    page.name = "Main";
    page.normalizedName = mfd::NormalizePageName(page.name);
    page.title = page.name;
    page.defaultPage = true;
    page.backgroundColor = {0, 0, 0, 255};
    page.staticReticles.push_back(std::move(backdrop));
    page.staticReticles.push_back(std::move(reticle));

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    mfd::SceneRegistry scene;
    scene.LoadDocument(std::move(document));
    scene.SetActivePage("Main");
    return scene;
}

mfd::Rgba32Framebuffer RenderScene(const mfd::SceneRegistry& scene)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(kRenderSize, kRenderSize, "mfd_canvas_clipping_render_tests");
    EXPECT_TRUE(IsWindowReady());

    mfd::MfdRenderer renderer;

    BeginDrawing();
    ClearBackground(BLACK);
    renderer.DrawActivePage(scene, kRenderSize, kRenderSize);
    const mfd::Rgba32Framebuffer framebuffer = mfd::OpenGlFramebufferReader::ReadRgba32();
    EndDrawing();

    CloseWindow();
    return framebuffer;
}

const mfd::Rgba8Pixel& PixelAt(const mfd::Rgba32Framebuffer& framebuffer, const int x, const int y)
{
    const std::size_t index =
        static_cast<std::size_t>(y) * static_cast<std::size_t>(framebuffer.width) +
        static_cast<std::size_t>(x);
    return framebuffer.pixels.at(index);
}

void ExpectGreen(const mfd::Rgba8Pixel& pixel)
{
    EXPECT_LT(pixel.r, 20U);
    EXPECT_GT(pixel.g, 200U);
    EXPECT_LT(pixel.b, 20U);
}

void ExpectBlack(const mfd::Rgba8Pixel& pixel)
{
    EXPECT_LT(pixel.r, 20U);
    EXPECT_LT(pixel.g, 20U);
    EXPECT_LT(pixel.b, 20U);
}
} // namespace

TEST(CanvasClippingRenderTests, OuterClippingErasesPreviouslyDrawnPixelsOutsideMaskOnly)
{
    const mfd::Rgba32Framebuffer framebuffer = RenderScene(MakeMaskedScene(mfd::ReticleClipMode::Outer));
    ASSERT_EQ(framebuffer.width, kRenderSize);
    ASSERT_EQ(framebuffer.height, kRenderSize);

    ExpectGreen(PixelAt(framebuffer, 48, 48));
    ExpectBlack(PixelAt(framebuffer, 12, 12));
}

TEST(CanvasClippingRenderTests, InnerClippingErasesPreviouslyDrawnPixelsInsideMaskOnly)
{
    const mfd::Rgba32Framebuffer framebuffer = RenderScene(MakeMaskedScene(mfd::ReticleClipMode::Inner));
    ASSERT_EQ(framebuffer.width, kRenderSize);
    ASSERT_EQ(framebuffer.height, kRenderSize);

    ExpectBlack(PixelAt(framebuffer, 48, 48));
    ExpectGreen(PixelAt(framebuffer, 12, 12));
}
