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

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
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

mfd::ReticleGroup MakeSmallTextReticle(const mfd::Vec2 position)
{
    mfd::ReticleGroup reticle;
    reticle.id = "tiny_text";

    mfd::Primitive text;
    text.id = "label";
    text.type = mfd::PrimitiveType::Text;
    text.transform.position = position;
    text.geometry = mfd::TextGeometry {"PIX", 0.06f, 0.0f};
    text.style.color = Green();
    reticle.primitives.push_back(std::move(text));

    return reticle;
}

mfd::ReticleGroup MakeAlignedTextReticle(const std::string& textValue,
                                         const mfd::Align align,
                                         const mfd::Vec2 position)
{
    mfd::ReticleGroup reticle;
    reticle.id = "aligned_text";

    mfd::Primitive text;
    text.id = "label";
    text.type = mfd::PrimitiveType::Text;
    text.transform.position = position;
    text.geometry = mfd::TextGeometry {textValue, 0.18f, 0.0f, align};
    text.style.color = Green();
    reticle.primitives.push_back(std::move(text));

    return reticle;
}

mfd::Rgba32Framebuffer RenderReticle(const mfd::ReticleGroup& reticle, mfd::TextLayoutCache& cache)
{
    BeginDrawing();
    ClearBackground(BLACK);
    mfd::Canvas2D canvas(kRenderSize, kRenderSize, {}, nullptr, BLACK, false, nullptr, nullptr, &cache);
    canvas.DrawReticle(reticle);
    const mfd::Rgba32Framebuffer framebuffer = mfd::OpenGlFramebufferReader::ReadRgba32();
    EndDrawing();
    return framebuffer;
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

std::size_t CountDifferentPixels(const mfd::Rgba32Framebuffer& lhs, const mfd::Rgba32Framebuffer& rhs)
{
    if (lhs.width != rhs.width || lhs.height != rhs.height || lhs.pixels.size() != rhs.pixels.size())
    {
        return static_cast<std::size_t>(-1);
    }

    std::size_t differenceCount = 0U;
    for (std::size_t index = 0; index < lhs.pixels.size(); ++index)
    {
        if (std::memcmp(&lhs.pixels[index], &rhs.pixels[index], sizeof(mfd::Rgba8Pixel)) != 0)
        {
            ++differenceCount;
        }
    }

    return differenceCount;
}

struct ForegroundSpan
{
    int minX = kRenderSize;
    int maxX = -1;

    [[nodiscard]] bool valid() const noexcept
    {
        return maxX >= minX;
    }
};

ForegroundSpan ComputeForegroundSpan(const mfd::Rgba32Framebuffer& framebuffer,
                                     const int minY,
                                     const int maxY)
{
    ForegroundSpan span;
    for (int y = minY; y < maxY; ++y)
    {
        for (int x = 0; x < framebuffer.width; ++x)
        {
            const std::size_t index =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(framebuffer.width) +
                static_cast<std::size_t>(x);
            const mfd::Rgba8Pixel& pixel = framebuffer.pixels.at(index);
            if (pixel.g > 80U && pixel.g > pixel.r + 30U && pixel.g > pixel.b + 30U)
            {
                span.minX = std::min(span.minX, x);
                span.maxX = std::max(span.maxX, x);
            }
        }
    }

    return span;
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

TEST(CanvasTextRenderTests, SmallTextSubPixelTranslationKeepsRasterStable)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(kRenderSize, kRenderSize, "mfd_canvas_text_subpixel_tests");
    ASSERT_TRUE(IsWindowReady());

    mfd::TextLayoutCache cache;
    const mfd::ReticleGroup aligned = MakeSmallTextReticle({0.0f, 0.0f});
    const mfd::ReticleGroup shifted = MakeSmallTextReticle({0.004f, 0.0f});

    const mfd::Rgba32Framebuffer alignedFramebuffer = RenderReticle(aligned, cache);
    const mfd::Rgba32Framebuffer shiftedFramebuffer = RenderReticle(shifted, cache);

    CloseWindow();

    EXPECT_GT(CountForegroundPixels(alignedFramebuffer, 40, 48, 88, 80), 8U);
    EXPECT_GT(CountForegroundPixels(shiftedFramebuffer, 40, 48, 88, 80), 8U);
    EXPECT_EQ(CountDifferentPixels(alignedFramebuffer, shiftedFramebuffer), 0U);
}

TEST(CanvasTextRenderTests, LeftAndRightAlignedTextKeepTheirAnchoredSideWhenWidthChanges)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(kRenderSize, kRenderSize, "mfd_canvas_text_alignment_tests");
    ASSERT_TRUE(IsWindowReady());

    mfd::TextLayoutCache cache;
    const mfd::Rgba32Framebuffer leftShort = RenderReticle(MakeAlignedTextReticle("UTC", mfd::Align::Left, {-0.35f, 0.0f}), cache);
    const mfd::Rgba32Framebuffer leftLong = RenderReticle(MakeAlignedTextReticle("UTCX", mfd::Align::Left, {-0.35f, 0.0f}), cache);
    const mfd::Rgba32Framebuffer rightShort = RenderReticle(MakeAlignedTextReticle("UTC", mfd::Align::Right, {0.35f, 0.0f}), cache);
    const mfd::Rgba32Framebuffer rightLong = RenderReticle(MakeAlignedTextReticle("UTCX", mfd::Align::Right, {0.35f, 0.0f}), cache);

    CloseWindow();

    const ForegroundSpan leftShortSpan = ComputeForegroundSpan(leftShort, 44, 84);
    const ForegroundSpan leftLongSpan = ComputeForegroundSpan(leftLong, 44, 84);
    const ForegroundSpan rightShortSpan = ComputeForegroundSpan(rightShort, 44, 84);
    const ForegroundSpan rightLongSpan = ComputeForegroundSpan(rightLong, 44, 84);

    ASSERT_TRUE(leftShortSpan.valid());
    ASSERT_TRUE(leftLongSpan.valid());
    ASSERT_TRUE(rightShortSpan.valid());
    ASSERT_TRUE(rightLongSpan.valid());

    EXPECT_LE(std::abs(leftShortSpan.minX - leftLongSpan.minX), 1);
    EXPECT_GT(leftLongSpan.maxX, leftShortSpan.maxX + 1);
    EXPECT_LE(std::abs(rightShortSpan.maxX - rightLongSpan.maxX), 1);
    EXPECT_LT(rightLongSpan.minX, rightShortSpan.minX - 1);
}
