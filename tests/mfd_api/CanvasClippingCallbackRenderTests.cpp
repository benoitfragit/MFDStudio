/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Hidden-window render tests covering the Canvas2D background-restore callback.
 *
 * The editor uses a restore callback so its visual guides (grid, page border) survive reticle
 * clipping, while the runtime keeps erasing clipped regions with the flat page background. These
 * tests pin both behaviours at the Canvas2D level.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <utility>

#include <raylib.h>

#include "Canvas2D.h"
#include "RenderTextureUtils.h"
#include "mfd/model/Reticle.h"
#include "mfd/render/OpenGlFramebufferReader.h"

namespace
{
constexpr int kRenderSize = 96;
constexpr int kCenter = kRenderSize / 2;
constexpr int kGridLineY = 20;

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

mfd::ReticleGroup MakeGreenBackdrop()
{
    mfd::ReticleGroup backdrop;
    backdrop.id = "backdrop";
    backdrop.primitives.push_back(MakeFilledRectangle());
    return backdrop;
}

mfd::ReticleGroup MakeOuterClipMask()
{
    mfd::ReticleGroup reticle;
    reticle.id = "mask";
    reticle.clipping.mode = mfd::ReticleClipMode::Outer;
    reticle.clipping.primitiveId = "mask";
    reticle.primitives.push_back(MakeInvisibleCircleMask());
    return reticle;
}

// Draws a bright red horizontal line standing in for an editor grid line. It is placed outside the
// circle mask but inside the green backdrop, so only a clip-time restore can bring it back.
void DrawGridLine()
{
    DrawRectangle(0, kGridLineY, kRenderSize, 2, RED);
}

mfd::Rgba32Framebuffer RenderClippedScene(const bool restoreGridOnClip)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(kRenderSize, kRenderSize, "mfd_canvas_clipping_callback_render_tests");
    EXPECT_TRUE(IsWindowReady());

    bool stencilReady = false;
    RenderTexture2D target = mfd::LoadRenderTextureWithStencil(kRenderSize, kRenderSize, &stencilReady);

    mfd::Rgba32Framebuffer framebuffer;
    if (stencilReady)
    {
        mfd::Canvas2D::BackgroundRestoreCallback restore;
        if (restoreGridOnClip)
        {
            restore = []()
            {
                DrawRectangle(0, 0, kRenderSize, kRenderSize, BLACK);
                DrawGridLine();
            };
        }

        BeginTextureMode(target);
        ClearBackground(BLACK);
        if (restoreGridOnClip)
        {
            DrawGridLine();
        }

        mfd::Canvas2D canvas(
            kRenderSize, kRenderSize, {}, nullptr, BLACK, true, nullptr, nullptr, nullptr, std::move(restore));
        canvas.DrawReticle(MakeGreenBackdrop());
        canvas.DrawReticle(MakeOuterClipMask());

        framebuffer = mfd::OpenGlFramebufferReader::ReadRgba32();
        EndTextureMode();
    }

    UnloadRenderTexture(target);
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

bool IsGreen(const mfd::Rgba8Pixel& pixel) noexcept
{
    return pixel.r < 80U && pixel.g > 150U && pixel.b < 80U;
}

bool IsGridRed(const mfd::Rgba8Pixel& pixel) noexcept
{
    return pixel.r > 150U && pixel.g < 80U && pixel.b < 80U;
}

std::size_t CountGridRedInColumn(const mfd::Rgba32Framebuffer& framebuffer, const int x)
{
    std::size_t count = 0U;
    for (int y = 0; y < framebuffer.height; ++y)
    {
        if (IsGridRed(PixelAt(framebuffer, x, y)))
        {
            ++count;
        }
    }

    return count;
}
} // namespace

TEST(CanvasClippingCallbackRenderTests, RestoreCallbackRepaintsGridInsideClippedRegion)
{
    const mfd::Rgba32Framebuffer framebuffer = RenderClippedScene(true);
    if (framebuffer.Empty())
    {
        GTEST_SKIP() << "Stencil render target unavailable on this driver.";
    }

    ASSERT_EQ(framebuffer.width, kRenderSize);
    ASSERT_EQ(framebuffer.height, kRenderSize);

    // The mask keeps the inside of the circle, so the green backdrop survives at the center.
    EXPECT_TRUE(IsGreen(PixelAt(framebuffer, kCenter, kCenter)));

    // Outside the circle the callback restored the background grid line, not only the flat fill.
    EXPECT_GT(CountGridRedInColumn(framebuffer, kCenter), 0U);
}

TEST(CanvasClippingCallbackRenderTests, DefaultRestoreErasesClippedRegionToFlatBackground)
{
    const mfd::Rgba32Framebuffer framebuffer = RenderClippedScene(false);
    if (framebuffer.Empty())
    {
        GTEST_SKIP() << "Stencil render target unavailable on this driver.";
    }

    ASSERT_EQ(framebuffer.width, kRenderSize);
    ASSERT_EQ(framebuffer.height, kRenderSize);

    // Runtime behaviour is unchanged: the center stays green and the clipped region holds no grid.
    EXPECT_TRUE(IsGreen(PixelAt(framebuffer, kCenter, kCenter)));
    EXPECT_EQ(CountGridRedInColumn(framebuffer, kCenter), 0U);
}
