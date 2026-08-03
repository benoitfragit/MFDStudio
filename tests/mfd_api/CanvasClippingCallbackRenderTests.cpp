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
#include <stdexcept>
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

mfd::ColorRgba Red() noexcept
{
    return {255, 0, 0, 255};
}

mfd::Primitive MakeFilledRedRectangle()
{
    mfd::Primitive primitive;
    primitive.id = "current_layer";
    primitive.type = mfd::PrimitiveType::Rectangle;
    primitive.geometry = mfd::RectangleGeometry {1.60f, 1.60f};
    primitive.style.color = Red();
    primitive.style.fillColor = Red();
    primitive.style.filled = true;
    return primitive;
}

mfd::ReticleGroup MakeRedCurrentLayerReticle()
{
    mfd::ReticleGroup reticle;
    reticle.id = "current_layer";
    reticle.primitives.push_back(MakeFilledRedRectangle());
    return reticle;
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

// Renders a two-layer scene where the current layer erases its own clipping only: a green lower
// layer, a red current-layer reticle, then an outer clip mask in the same current layer. The restore
// callback repaints the lower layer (without clipping) and the editor grid, but never the current
// layer, mirroring the runtime layer-local restore.
mfd::Rgba32Framebuffer RenderLayerLocalClippedScene()
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(kRenderSize, kRenderSize, "mfd_canvas_clipping_layer_local_render_tests");
    EXPECT_TRUE(IsWindowReady());

    bool stencilReady = false;
    RenderTexture2D target = mfd::LoadRenderTextureWithStencil(kRenderSize, kRenderSize, &stencilReady);

    mfd::Rgba32Framebuffer framebuffer;
    if (stencilReady)
    {
        const mfd::ReticleGroup backdrop = MakeGreenBackdrop();
        mfd::Canvas2D* canvasForRestore = nullptr;
        mfd::Canvas2D::BackgroundRestoreCallback restore =
            [&canvasForRestore, &backdrop]()
        {
            DrawRectangle(0, 0, kRenderSize, kRenderSize, BLACK);
            DrawGridLine();
            if (canvasForRestore != nullptr)
            {
                canvasForRestore->DrawReticleWithoutClipping(backdrop, true);
            }
        };

        BeginTextureMode(target);
        ClearBackground(BLACK);
        DrawGridLine();

        mfd::Canvas2D canvas(
            kRenderSize, kRenderSize, {}, nullptr, BLACK, true, nullptr, nullptr, nullptr, std::move(restore));
        canvasForRestore = &canvas;
        canvas.DrawReticle(backdrop);                  // lower layer
        canvas.DrawReticle(MakeRedCurrentLayerReticle()); // current layer content
        canvas.DrawReticle(MakeOuterClipMask());          // current layer clipping reticle

        framebuffer = mfd::OpenGlFramebufferReader::ReadRgba32();
        EndTextureMode();
    }

    UnloadRenderTexture(target);
    CloseWindow();
    return framebuffer;
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

TEST(CanvasClippingCallbackRenderTests, LayerLocalRestoreKeepsLowerLayerAndErasesCurrentLayer)
{
    const mfd::Rgba32Framebuffer framebuffer = RenderLayerLocalClippedScene();
    if (framebuffer.Empty())
    {
        GTEST_SKIP() << "Stencil render target unavailable on this driver.";
    }

    ASSERT_EQ(framebuffer.width, kRenderSize);
    ASSERT_EQ(framebuffer.height, kRenderSize);

    // Inside the kept clip region, the current-layer red reticle survives.
    EXPECT_TRUE(IsGridRed(PixelAt(framebuffer, kCenter, kCenter)));

    // In the erased region, the lower layer is restored (green) while the current layer is not (no red).
    // The probe row sits inside the backdrop rectangle but outside the clip circle and off the grid line.
    constexpr int kOutsideY = 14;
    EXPECT_TRUE(IsGreen(PixelAt(framebuffer, kCenter, kOutsideY)));
    EXPECT_FALSE(IsGridRed(PixelAt(framebuffer, kCenter, kOutsideY)));

    // The editor grid is still restored inside the clipped region when a callback is provided.
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

TEST(CanvasClippingCallbackRenderTests, ThrowingRestoreCallbackDoesNotLeakStencilOrColorMaskState)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(kRenderSize, kRenderSize, "mfd_canvas_clipping_exception_state");
    ASSERT_TRUE(IsWindowReady());

    bool stencilReady = false;
    RenderTexture2D target = mfd::LoadRenderTextureWithStencil(kRenderSize, kRenderSize, &stencilReady);
    if (!stencilReady)
    {
        UnloadRenderTexture(target);
        CloseWindow();
        GTEST_SKIP() << "Stencil render target unavailable on this driver.";
    }

    BeginTextureMode(target);
    ClearBackground(BLACK);
    mfd::Canvas2D canvas(
        kRenderSize,
        kRenderSize,
        {},
        nullptr,
        BLACK,
        true,
        nullptr,
        nullptr,
        nullptr,
        []()
        {
            throw std::runtime_error("injected background restore failure");
        });
    EXPECT_THROW(canvas.DrawReticle(MakeOuterClipMask()), std::runtime_error);

    DrawRectangle(0, 0, kRenderSize, kRenderSize, GREEN);
    const mfd::Rgba32Framebuffer framebuffer = mfd::OpenGlFramebufferReader::ReadRgba32();
    EndTextureMode();

    const bool frameReady = !framebuffer.Empty();
    const bool centerIsGreen = frameReady && IsGreen(PixelAt(framebuffer, kCenter, kCenter));
    UnloadRenderTexture(target);
    CloseWindow();
    EXPECT_TRUE(frameReady);
    EXPECT_TRUE(centerIsGreen);
}
