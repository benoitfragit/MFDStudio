/**
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Hidden-window render tests and fast-path predicates covering solid ring rendering.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <utility>

#include <raylib.h>

#include "Canvas2D.h"
#include "CanvasFastPath.h"
#include "mfd/model/Reticle.h"
#include "mfd/render/OpenGlFramebufferReader.h"

namespace
{
constexpr int kRenderSize = 192;

mfd::ColorRgba Green() noexcept
{
    return {0, 255, 0, 255};
}

mfd::ReticleGroup MakeRingReticle(const bool filled,
                                  const mfd::LineStyle lineStyle,
                                  const mfd::Vec2 scale = {1.0f, 1.0f},
                                  const float innerRadius = 0.25f,
                                  const float outerRadius = 0.55f)
{
    mfd::ReticleGroup reticle;
    reticle.id = filled ? "filled_ring" : "outline_ring";
    reticle.transform.scale = scale;

    mfd::Primitive primitive;
    primitive.id = "scope";
    primitive.type = mfd::PrimitiveType::Ring;
    primitive.geometry = mfd::RingGeometry {innerRadius, outerRadius, 0};
    primitive.style.color = Green();
    primitive.style.fillColor = Green();
    primitive.style.thickness = 0.04f;
    primitive.style.lineStyle = lineStyle;
    primitive.style.filled = filled;
    reticle.primitives.push_back(std::move(primitive));
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

const mfd::Rgba8Pixel& CenterPixel(const mfd::Rgba32Framebuffer& framebuffer)
{
    const std::size_t centerIndex =
        static_cast<std::size_t>(kRenderSize / 2) * framebuffer.width + static_cast<std::size_t>(kRenderSize / 2);
    return framebuffer.pixels.at(centerIndex);
}
} // namespace

TEST(CanvasRingFastPathTests, AcceptsSolidIsotropicFiniteRings)
{
    EXPECT_TRUE(mfd::detail::CanUseFastSolidRingPath(mfd::LineStyle::Solid, {1.0f, 1.0f}, 24.0f, 56.0f));
    EXPECT_TRUE(mfd::detail::CanUseFastSolidRingPath(mfd::LineStyle::Solid, {-2.0f, 2.0f}, 12.0f, 28.0f));
}

TEST(CanvasRingFastPathTests, RejectsDashedNonIsotropicAndInvalidRings)
{
    EXPECT_FALSE(mfd::detail::CanUseFastSolidRingPath(mfd::LineStyle::Dashed, {1.0f, 1.0f}, 24.0f, 56.0f));
    EXPECT_FALSE(mfd::detail::CanUseFastSolidRingPath(mfd::LineStyle::Solid, {1.0f, 1.3f}, 24.0f, 56.0f));
    EXPECT_FALSE(mfd::detail::CanUseFastSolidRingPath(mfd::LineStyle::Solid, {1.0f, 1.0f}, -1.0f, 56.0f));
    EXPECT_FALSE(mfd::detail::CanUseFastSolidRingPath(mfd::LineStyle::Solid, {1.0f, 1.0f}, 60.0f, 56.0f));
    EXPECT_FALSE(mfd::detail::CanUseFastSolidRingPath(mfd::LineStyle::Solid, {1.0f, 1.0f}, 24.0f, 0.0f));
}

TEST(CanvasRingRenderTests, FilledSolidRingRendersAnnulusAndKeepsCenterDark)
{
    const mfd::Rgba32Framebuffer framebuffer = RenderReticle(MakeRingReticle(true, mfd::LineStyle::Solid));

    EXPECT_GT(CountForegroundPixels(framebuffer), 900U);
    const mfd::Rgba8Pixel& center = CenterPixel(framebuffer);
    EXPECT_LT(center.g, 30U);
}

TEST(CanvasRingRenderTests, OutlineOnlySolidRingRendersLessCoverageThanFilledRing)
{
    const mfd::Rgba32Framebuffer filled = RenderReticle(MakeRingReticle(true, mfd::LineStyle::Solid));
    const mfd::Rgba32Framebuffer outline = RenderReticle(MakeRingReticle(false, mfd::LineStyle::Solid));

    const std::size_t filledPixels = CountForegroundPixels(filled);
    const std::size_t outlinePixels = CountForegroundPixels(outline);
    EXPECT_GT(filledPixels, outlinePixels + outlinePixels / 3U);
}

TEST(CanvasRingRenderTests, DegenerateSolidRingWithZeroOuterRadiusDrawsNothing)
{
    const mfd::Rgba32Framebuffer framebuffer =
        RenderReticle(MakeRingReticle(true, mfd::LineStyle::Solid, {1.0f, 1.0f}, 0.0f, 0.0f));

    EXPECT_EQ(CountForegroundPixels(framebuffer), 0U);
}
