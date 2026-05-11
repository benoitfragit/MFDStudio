/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Hidden-window render tests covering text and time primitive drawing.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <utility>

#include <raylib.h>

#include "Canvas2D.h"
#include "TextLayoutCache.h"
#include "mfd/model/Reticle.h"
#include "mfd/render/OpenGlFramebufferReader.h"

namespace
{
constexpr int kRenderSize = 128;

mfd::ColorRgba Green() noexcept
{
    return {0, 255, 0, 255};
}

mfd::ReticleGroup MakeTextAndTimeReticle()
{
    mfd::ReticleGroup reticle;
    reticle.id = "hud_text";

    mfd::Primitive text;
    text.id = "label";
    text.type = mfd::PrimitiveType::Text;
    text.transform.position = {0.0f, 0.35f};
    text.geometry = mfd::TextGeometry {"TXT", 0.20f, 0.01f};
    text.style.color = Green();
    reticle.primitives.push_back(std::move(text));

    mfd::Primitive time;
    time.id = "clock";
    time.type = mfd::PrimitiveType::Time;
    time.transform.position = {0.0f, -0.35f};
    time.geometry = mfd::TimeGeometry {"UTC", true, 0.20f, 0.01f};
    time.style.color = Green();
    reticle.primitives.push_back(std::move(time));

    return reticle;
}

std::size_t CountForegroundPixels(const mfd::Rgba32Framebuffer& framebuffer,
                                  const int minX,
                                  const int minY,
                                  const int maxX,
                                  const int maxY)
{
    std::size_t count = 0U;
    for (int y = minY; y < maxY; ++y)
    {
        for (int x = minX; x < maxX; ++x)
        {
            const std::size_t index =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(framebuffer.width) +
                static_cast<std::size_t>(x);
            const mfd::Rgba8Pixel& pixel = framebuffer.pixels.at(index);
            if (pixel.g > 80U && pixel.g > pixel.r + 30U && pixel.g > pixel.b + 30U)
            {
                ++count;
            }
        }
    }

    return count;
}
} // namespace

TEST(CanvasTextRenderTests, TextAndTimePrimitivesStillRenderVisiblePixels)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(kRenderSize, kRenderSize, "mfd_canvas_text_render_tests");
    ASSERT_TRUE(IsWindowReady());

    mfd::TextLayoutCache cache;
    const mfd::ReticleGroup reticle = MakeTextAndTimeReticle();

    BeginDrawing();
    ClearBackground(BLACK);
    mfd::Canvas2D firstCanvas(kRenderSize, kRenderSize, {}, nullptr, BLACK, false, nullptr, nullptr, &cache);
    firstCanvas.DrawReticle(reticle);
    EndDrawing();

    BeginDrawing();
    ClearBackground(BLACK);
    mfd::Canvas2D secondCanvas(kRenderSize, kRenderSize, {}, nullptr, BLACK, false, nullptr, nullptr, &cache);
    secondCanvas.DrawReticle(reticle);
    const mfd::Rgba32Framebuffer framebuffer = mfd::OpenGlFramebufferReader::ReadRgba32();
    EndDrawing();

    CloseWindow();

    const std::size_t topPixels = CountForegroundPixels(framebuffer, 20, 18, 108, 58);
    const std::size_t bottomPixels = CountForegroundPixels(framebuffer, 20, 70, 108, 110);
    EXPECT_GT(topPixels, 12U);
    EXPECT_GT(bottomPixels, 12U);

    const mfd::TextLayoutCache::Stats stats = cache.CacheStats();
    EXPECT_EQ(stats.staticMisses, 1U);
    EXPECT_GE(stats.staticHits, 1U);
}
