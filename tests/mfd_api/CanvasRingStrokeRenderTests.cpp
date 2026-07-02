/**
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Hidden-window render tests covering the solid isotropic ring fast path.
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

mfd::ReticleGroup MakeRingReticle(const mfd::LineStyle lineStyle, const bool filled)
{
    mfd::ReticleGroup reticle;
    reticle.id = "ring";

    mfd::Primitive primitive;
    primitive.id = "scope";
    primitive.type = mfd::PrimitiveType::Ring;
    primitive.geometry = mfd::RingGeometry {0.35f, 0.55f, 64};
    primitive.style.color = Green();
    primitive.style.fillColor = Green();
    primitive.style.thickness = 0.04f;
    primitive.style.lineStyle = lineStyle;
    primitive.style.filled = filled;
    reticle.primitives.push_back(std::move(primitive));
    return reticle;
}

mfd::ReticleGroup MakeDegenerateRingReticle()
{
    mfd::ReticleGroup reticle = MakeRingReticle(mfd::LineStyle::Solid, false);
    reticle.primitives.front().geometry = mfd::RingGeometry {0.0f, 0.0f, 64};
    return reticle;
}

mfd::Rgba32Framebuffer RenderReticle(const mfd::ReticleGroup& reticle)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(kRenderSize, kRenderSize, "mfd_canvas_ring_render_tests");
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

bool IsForegroundPixel(const mfd::Rgba8Pixel& pixel) noexcept
{
    return pixel.g > 80U && pixel.g > pixel.r + 30U && pixel.g > pixel.b + 30U;
}

std::size_t CountForegroundPixels(const mfd::Rgba32Framebuffer& framebuffer)
{
    std::size_t count = 0U;
    for (const mfd::Rgba8Pixel& pixel : framebuffer.pixels)
    {
        if (IsForegroundPixel(pixel))
        {
            ++count;
        }
    }

    return count;
}

bool CenterPixelIsForeground(const mfd::Rgba32Framebuffer& framebuffer)
{
    const std::size_t centerIndex =
        static_cast<std::size_t>(framebuffer.height / 2) * static_cast<std::size_t>(framebuffer.width) +
        static_cast<std::size_t>(framebuffer.width / 2);
    return centerIndex < framebuffer.pixels.size() && IsForegroundPixel(framebuffer.pixels[centerIndex]);
}
} // namespace

TEST(CanvasRingStrokeRenderTests, FilledSolidRingPaintsBandButKeepsCenterHole)
{
    const mfd::Rgba32Framebuffer framebuffer = RenderReticle(MakeRingReticle(mfd::LineStyle::Solid, true));

    EXPECT_GT(CountForegroundPixels(framebuffer), 1000U);
    EXPECT_FALSE(CenterPixelIsForeground(framebuffer));
}

TEST(CanvasRingStrokeRenderTests, DashedRingRendersLessStrokeCoverageThanSolidRing)
{
    const mfd::Rgba32Framebuffer solidFramebuffer = RenderReticle(MakeRingReticle(mfd::LineStyle::Solid, false));
    const mfd::Rgba32Framebuffer dashedFramebuffer = RenderReticle(MakeRingReticle(mfd::LineStyle::Dashed, false));

    const std::size_t solidPixels = CountForegroundPixels(solidFramebuffer);
    const std::size_t dashedPixels = CountForegroundPixels(dashedFramebuffer);

    EXPECT_GT(solidPixels, 800U);
    EXPECT_GT(dashedPixels, 200U);
    EXPECT_GT(solidPixels, dashedPixels + dashedPixels / 4U);
}

TEST(CanvasRingStrokeRenderTests, DegenerateSolidRingDrawsNothing)
{
    const mfd::Rgba32Framebuffer framebuffer = RenderReticle(MakeDegenerateRingReticle());

    EXPECT_EQ(CountForegroundPixels(framebuffer), 0U);
}
