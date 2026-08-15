/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Deterministic unit and hidden-window coverage for Canvas2D render-work accounting.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include <raylib.h>

#include "Canvas2D.h"
#include "CanvasDrawWork.h"
#include "ImageTextureCache.h"
#include "TextLayoutCache.h"
#include "mfd/model/Reticle.h"
#include "mfd/render/OpenGlFramebufferReader.h"

namespace
{
constexpr int kRenderSize = 128;
constexpr std::size_t kFastPathSegmentCount = 64U;
constexpr std::size_t kExplicitRingSegmentCount = 128U;

mfd::ColorRgba Green() noexcept { return {0, 255, 0, 255}; }
mfd::ColorRgba Red() noexcept { return {255, 0, 0, 255}; }

void AppendBudgetWitness(mfd::ReticleGroup& reticle)
{
    mfd::Primitive witness;
    witness.id = "budget_witness";
    witness.type = mfd::PrimitiveType::Text;
    witness.transform.position = {0.45f, 0.0f};
    witness.geometry = mfd::TextGeometry {"W", 0.20f, 0.0f};
    witness.style.color = Red();
    reticle.primitives.push_back(std::move(witness));
}

mfd::ReticleGroup MakeTextBudgetReticle(const mfd::PrimitiveType type)
{
    mfd::ReticleGroup reticle;
    reticle.id = type == mfd::PrimitiveType::Text ? "text_budget" : "time_budget";

    mfd::Primitive primitive;
    primitive.id = "text_like";
    primitive.type = type;
    primitive.transform.position = {-0.45f, 0.0f};
    primitive.style.color = Green();
    if (type == mfd::PrimitiveType::Text)
    {
        primitive.geometry = mfd::TextGeometry {"A", 0.20f, 0.0f};
    }
    else
    {
        primitive.geometry = mfd::TimeGeometry {"T", true, 0.20f, 0.0f};
    }
    reticle.primitives.push_back(std::move(primitive));
    AppendBudgetWitness(reticle);
    return reticle;
}

mfd::ReticleGroup MakeCircleBudgetReticle()
{
    mfd::ReticleGroup reticle;
    reticle.id = "circle_budget";
    mfd::Primitive circle;
    circle.id = "circle";
    circle.type = mfd::PrimitiveType::Circle;
    circle.transform.position = {-0.45f, 0.0f};
    circle.geometry = mfd::CircleGeometry {0.15f};
    circle.style.color = Green();
    circle.style.fillColor = Green();
    circle.style.filled = true;
    circle.style.lineStyle = mfd::LineStyle::Solid;
    reticle.primitives.push_back(std::move(circle));
    AppendBudgetWitness(reticle);
    return reticle;
}

mfd::ReticleGroup MakeRingBudgetReticle()
{
    mfd::ReticleGroup reticle;
    reticle.id = "ring_budget";
    mfd::Primitive ring;
    ring.id = "ring";
    ring.type = mfd::PrimitiveType::Ring;
    ring.transform.position = {-0.45f, 0.0f};
    ring.geometry = mfd::RingGeometry {
        0.08f,
        0.15f,
        static_cast<int>(kExplicitRingSegmentCount)};
    ring.style.color = Green();
    ring.style.fillColor = Green();
    ring.style.filled = true;
    ring.style.lineStyle = mfd::LineStyle::Solid;
    reticle.primitives.push_back(std::move(ring));
    AppendBudgetWitness(reticle);
    return reticle;
}

mfd::ReticleGroup MakeImageBudgetReticle(const std::filesystem::path& imageFile)
{
    mfd::ReticleGroup reticle;
    reticle.id = "image_budget";
    mfd::Primitive image;
    image.id = "image";
    image.type = mfd::PrimitiveType::Image;
    image.transform.position = {-0.45f, 0.0f};
    image.geometry = mfd::ImageGeometry {imageFile, 0.25f, 0.25f};
    reticle.primitives.push_back(std::move(image));
    AppendBudgetWitness(reticle);
    return reticle;
}

mfd::ReticleGroup MakeCircleClipBudgetReticle()
{
    mfd::ReticleGroup reticle;
    reticle.id = "circle_clip_budget";
    reticle.clipping.mode = mfd::ReticleClipMode::Outer;
    reticle.clipping.primitiveId = "mask";

    mfd::Primitive mask;
    mask.id = "mask";
    mask.type = mfd::PrimitiveType::Circle;
    mask.geometry = mfd::CircleGeometry {0.35f};
    mask.style.visible = false;
    reticle.primitives.push_back(std::move(mask));
    AppendBudgetWitness(reticle);
    return reticle;
}

class RestoreCounter
{
public:
    void operator()() noexcept { ++count_; }
    [[nodiscard]] int Count() const noexcept { return count_; }

private:
    int count_ = 0;
};

class TemporaryGreenPng
{
public:
    TemporaryGreenPng()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("mfd_canvas_draw_work_" + std::to_string(suffix) + ".png");
        Image image = GenImageColor(2, 2, GREEN);
        const bool exported = ExportImage(image, path_.string().c_str());
        UnloadImage(image);
        if (!exported)
        {
            throw std::runtime_error("Unable to export the temporary Canvas draw-work PNG.");
        }
    }

    ~TemporaryGreenPng()
    {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }

private:
    std::filesystem::path path_ {};
};

mfd::Rgba32Framebuffer RenderWithBudget(const mfd::ReticleGroup& reticle,
                                        const std::size_t maximumDrawWorkUnits,
                                        mfd::ImageTextureCache* imageCache = nullptr)
{
    mfd::TextLayoutCache textLayoutCache;
    BeginDrawing();
    ClearBackground(BLACK);
    mfd::Canvas2D canvas(kRenderSize,
                         kRenderSize,
                         {},
                         nullptr,
                         BLACK,
                         false,
                         nullptr,
                         imageCache,
                         &textLayoutCache,
                         {},
                         maximumDrawWorkUnits);
    canvas.DrawReticle(reticle);
    const mfd::Rgba32Framebuffer framebuffer = mfd::OpenGlFramebufferReader::ReadRgba32();
    EndDrawing();
    return framebuffer;
}

std::size_t CountDominantPixels(const mfd::Rgba32Framebuffer& framebuffer,
                                const bool leftHalf,
                                const bool green)
{
    std::size_t count = 0U;
    const int minimumX = leftHalf ? 0 : kRenderSize / 2;
    const int maximumX = leftHalf ? kRenderSize / 2 : kRenderSize;
    for (int y = 0; y < framebuffer.height; ++y)
    {
        for (int x = minimumX; x < maximumX; ++x)
        {
            const std::size_t index = static_cast<std::size_t>(y) *
                                          static_cast<std::size_t>(framebuffer.width) +
                                      static_cast<std::size_t>(x);
            const mfd::Rgba8Pixel& pixel = framebuffer.pixels.at(index);
            const bool matches = green
                                     ? pixel.g > 80U && pixel.g > pixel.r + 30U && pixel.g > pixel.b + 30U
                                     : pixel.r > 80U && pixel.r > pixel.g + 30U && pixel.r > pixel.b + 30U;
            if (matches)
            {
                ++count;
            }
        }
    }
    return count;
}

void ExpectOnlyBudgetedPrimitiveRendered(const mfd::Rgba32Framebuffer& framebuffer)
{
    EXPECT_GT(CountDominantPixels(framebuffer, true, true), 4U);
    EXPECT_EQ(CountDominantPixels(framebuffer, false, false), 0U);
}

class CanvasDrawWorkRenderTests : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(kRenderSize, kRenderSize, "mfd_canvas_draw_work_tests");
        ASSERT_TRUE(IsWindowReady());
    }

    static void TearDownTestSuite() { CloseWindow(); }
};
} // namespace

TEST(CanvasDrawWorkTests, EstimatesTextAndTessellatedGeometryCosts)
{
    EXPECT_EQ(mfd::detail::EstimateTextDrawWorkUnits(""), 1U);
    EXPECT_EQ(mfd::detail::EstimateTextDrawWorkUnits("HUD"), 3U);
    EXPECT_EQ(mfd::detail::EstimateTextDrawWorkUnits("\xC3\xA9"), 2U);
    EXPECT_EQ(mfd::detail::EstimateTextDrawWorkUnits(std::string(4096U, 'X')), 4096U);
    EXPECT_EQ(mfd::detail::EstimateConvexFillWorkUnits(2U), 0U);
    EXPECT_EQ(mfd::detail::EstimateConvexFillWorkUnits(kFastPathSegmentCount), 62U);
    EXPECT_EQ(mfd::detail::EstimateClosedStrokeWorkUnits(kFastPathSegmentCount), 64U);
    EXPECT_EQ(mfd::detail::EstimateRingFillWorkUnits(kFastPathSegmentCount), 128U);
    EXPECT_EQ(
        mfd::detail::EstimateRingFillWorkUnits(std::numeric_limits<std::size_t>::max()),
        std::numeric_limits<std::size_t>::max());
    EXPECT_EQ(mfd::detail::EstimateDirectDrawWorkUnits(), 1U);
}

TEST_F(CanvasDrawWorkRenderTests, TextAndTimeConsumeTheirTextBudget)
{
    ExpectOnlyBudgetedPrimitiveRendered(RenderWithBudget(
        MakeTextBudgetReticle(mfd::PrimitiveType::Text), mfd::detail::EstimateTextDrawWorkUnits("A")));
    ExpectOnlyBudgetedPrimitiveRendered(RenderWithBudget(
        MakeTextBudgetReticle(mfd::PrimitiveType::Time), mfd::detail::EstimateTextDrawWorkUnits("T")));
}

TEST_F(CanvasDrawWorkRenderTests, FastCircleAndRingConsumeEquivalentTessellationBudget)
{
    const std::size_t circleWorkUnits =
        mfd::detail::EstimateConvexFillWorkUnits(kFastPathSegmentCount) +
        mfd::detail::EstimateClosedStrokeWorkUnits(kFastPathSegmentCount);
    ExpectOnlyBudgetedPrimitiveRendered(RenderWithBudget(MakeCircleBudgetReticle(), circleWorkUnits));

    const std::size_t ringWorkUnits =
        mfd::detail::EstimateRingFillWorkUnits(kExplicitRingSegmentCount) +
        2U * mfd::detail::EstimateClosedStrokeWorkUnits(kExplicitRingSegmentCount);
    ExpectOnlyBudgetedPrimitiveRendered(RenderWithBudget(MakeRingBudgetReticle(), ringWorkUnits));
}

TEST_F(CanvasDrawWorkRenderTests, ImageResolveAndDrawConsumesOneDirectOperation)
{
    const TemporaryGreenPng imageFile;
    mfd::ImageTextureCache imageCache;
    ExpectOnlyBudgetedPrimitiveRendered(RenderWithBudget(
        MakeImageBudgetReticle(imageFile.Path()), mfd::detail::EstimateDirectDrawWorkUnits(), &imageCache));
    imageCache.Clear();
}

TEST_F(CanvasDrawWorkRenderTests, CircleClipRequiresItsCompleteTessellationAndRestoreBudget)
{
    const std::size_t clipWorkUnits =
        mfd::detail::EstimateConvexFillWorkUnits(kFastPathSegmentCount) +
        mfd::detail::EstimateDirectDrawWorkUnits();
    const mfd::ReticleGroup reticle = MakeCircleClipBudgetReticle();

    RestoreCounter acceptedRestore;
    BeginDrawing();
    ClearBackground(BLACK);
    mfd::Canvas2D acceptedCanvas(
        kRenderSize,
        kRenderSize,
        {},
        nullptr,
        BLACK,
        true,
        nullptr,
        nullptr,
        nullptr,
        std::ref(acceptedRestore),
        clipWorkUnits);
    acceptedCanvas.DrawReticle(reticle);
    EndDrawing();
    EXPECT_EQ(acceptedRestore.Count(), 1);

    RestoreCounter rejectedRestore;
    BeginDrawing();
    ClearBackground(BLACK);
    mfd::Canvas2D rejectedCanvas(
        kRenderSize,
        kRenderSize,
        {},
        nullptr,
        BLACK,
        true,
        nullptr,
        nullptr,
        nullptr,
        std::ref(rejectedRestore),
        clipWorkUnits - 1U);
    rejectedCanvas.DrawReticle(reticle);
    const mfd::Rgba32Framebuffer rejectedFrame = mfd::OpenGlFramebufferReader::ReadRgba32();
    EndDrawing();

    EXPECT_EQ(rejectedRestore.Count(), 0);
    EXPECT_EQ(CountDominantPixels(rejectedFrame, false, false), 0U);
}
