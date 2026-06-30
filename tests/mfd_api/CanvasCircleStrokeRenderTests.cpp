/**
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Hidden-window render tests covering circle stroke quality and dash continuity.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <utility>

#include <raylib.h>

#include "Canvas2D.h"
#include "mfd/model/Reticle.h"
#include "mfd/render/OpenGlFramebufferReader.h"

namespace
{
constexpr int kRenderSize = 192;

mfd::ColorRgba Green() noexcept
{
    return {0, 255, 0, 255};
}

mfd::ReticleGroup MakeCircleReticle(const mfd::LineStyle lineStyle)
{
    mfd::ReticleGroup reticle;
    reticle.id = lineStyle == mfd::LineStyle::Solid ? "solid_circle" : "dashed_circle";

    mfd::Primitive primitive;
    primitive.id = "scope";
    primitive.type = mfd::PrimitiveType::Circle;
    primitive.geometry = mfd::CircleGeometry {0.55f};
    primitive.style.color = Green();
    primitive.style.thickness = 0.04f;
    primitive.style.lineStyle = lineStyle;
    reticle.primitives.push_back(std::move(primitive));
    return reticle;
}

mfd::ReticleGroup MakeDegenerateCircleReticle()
{
    mfd::ReticleGroup reticle;
    reticle.id = "degenerate_circle";

    mfd::Primitive primitive;
    primitive.id = "scope";
    primitive.type = mfd::PrimitiveType::Circle;
    primitive.geometry = mfd::CircleGeometry {0.0f};
    primitive.style.color = Green();
    primitive.style.thickness = 0.04f;
    primitive.style.lineStyle = mfd::LineStyle::Solid;
    primitive.style.filled = false;
    reticle.primitives.push_back(std::move(primitive));
    return reticle;
}

mfd::Rgba32Framebuffer RenderReticle(const mfd::ReticleGroup& reticle)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(kRenderSize, kRenderSize, "mfd_canvas_circle_render_tests");
    EXPECT_TRUE(IsWindowReady());

    BeginDrawing();
    ClearBackground(BLACK);
    mfd::Canvas2D canvas(kRenderSize, kRenderSize, {}, nullptr, BLACK, false, nullptr, nullptr, nullptr);
    canvas.DrawReticle(reticle);
    const mfd::Rgba32Framebuffer framebuffer = mfd::OpenGlFramebufferReader::ReadRgba32();
    EndDrawing();

    CloseWindow();
    return framebuffer;
}

std::size_t CountForegroundPixels(const mfd::Rgba32Framebuffer& framebuffer)
{
    std::size_t count = 0U;
    for (const mfd::Rgba8Pixel& pixel : framebuffer.pixels)
    {
        if (pixel.g > 80U && pixel.g > pixel.r + 30U && pixel.g > pixel.b + 30U)
        {
            ++count;
        }
    }

    return count;
}
} // namespace

TEST(CanvasCircleStrokeRenderTests, DashedCircleRendersLessStrokeCoverageThanSolidCircle)
{
    const mfd::Rgba32Framebuffer solidFramebuffer = RenderReticle(MakeCircleReticle(mfd::LineStyle::Solid));
    const mfd::Rgba32Framebuffer dashedFramebuffer = RenderReticle(MakeCircleReticle(mfd::LineStyle::Dashed));

    const std::size_t solidPixels = CountForegroundPixels(solidFramebuffer);
    const std::size_t dashedPixels = CountForegroundPixels(dashedFramebuffer);

    EXPECT_GT(solidPixels, 400U);
    EXPECT_GT(dashedPixels, 200U);
    EXPECT_GT(solidPixels, dashedPixels + dashedPixels / 4U);
}

TEST(CanvasCircleStrokeRenderTests, DegenerateSolidCircleDrawsNothing)
{
    const mfd::Rgba32Framebuffer framebuffer = RenderReticle(MakeDegenerateCircleReticle());

    EXPECT_EQ(CountForegroundPixels(framebuffer), 0U);
}
