/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for the private text layout cache used by Canvas2D.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <ctime>
#include <string>
#include <string_view>

#include "TextLayoutCache.h"

namespace
{
int gMeasureCallCount = 0;
int gFormatCallCount = 0;

Vector2 FakeMeasureText(const std::string_view text,
                        const Font&,
                        const float fontSize,
                        const float letterSpacing)
{
    ++gMeasureCallCount;
    return Vector2 {
        static_cast<float>(text.size()) * fontSize + letterSpacing,
        fontSize + letterSpacing};
}

std::string FakeFormatTime(const mfd::TimeGeometry& geometry, const std::time_t second)
{
    ++gFormatCallCount;
    return geometry.format + (geometry.utc ? "_utc_" : "_local_") + std::to_string(static_cast<long long>(second));
}

Font MakeFont(const unsigned int textureId = 7U)
{
    Font font {};
    font.baseSize = 18;
    font.glyphCount = 96;
    font.glyphPadding = 1;
    font.texture.id = textureId;
    return font;
}

void ResetCounters()
{
    gMeasureCallCount = 0;
    gFormatCallCount = 0;
}
} // namespace

TEST(TextLayoutCacheTests, ResolveStaticTextReusesMeasuredLayoutForMatchingInputs)
{
    ResetCounters();
    mfd::TextLayoutCache cache(&FakeMeasureText, &FakeFormatTime);
    const Font font = MakeFont();

    const mfd::CachedTextLayout& first = cache.ResolveStaticText("CALLSIGN", font, 14.0f, 2.0f, mfd::Align::Center);
    const mfd::CachedTextLayout& second = cache.ResolveStaticText("CALLSIGN", font, 14.0f, 2.0f, mfd::Align::Center);

    EXPECT_EQ(&first, &second);
    EXPECT_EQ(gMeasureCallCount, 1);
    EXPECT_EQ(gFormatCallCount, 0);
    EXPECT_FLOAT_EQ(first.size.x, 8.0f * 14.0f + 2.0f);
    EXPECT_FLOAT_EQ(first.origin.x, first.size.x * 0.5f);
    EXPECT_FLOAT_EQ(first.origin.y, first.size.y * 0.5f);

    const mfd::TextLayoutCache::Stats stats = cache.CacheStats();
    EXPECT_EQ(stats.staticMisses, 1U);
    EXPECT_EQ(stats.staticHits, 1U);
    EXPECT_EQ(cache.StaticEntryCount(), 1U);
}

TEST(TextLayoutCacheTests, ResolveStaticTextMissesWhenMeasurementInputsChange)
{
    ResetCounters();
    mfd::TextLayoutCache cache(&FakeMeasureText, &FakeFormatTime);
    const Font font = MakeFont();

    cache.ResolveStaticText("ALT", font, 14.0f, 2.0f, mfd::Align::Center);
    cache.ResolveStaticText("ALT", font, 14.0f, 3.0f, mfd::Align::Center);
    cache.ResolveStaticText("ALT", MakeFont(8U), 14.0f, 3.0f, mfd::Align::Center);

    EXPECT_EQ(gMeasureCallCount, 3);
    const mfd::TextLayoutCache::Stats stats = cache.CacheStats();
    EXPECT_EQ(stats.staticMisses, 3U);
    EXPECT_EQ(stats.staticHits, 0U);
    EXPECT_EQ(cache.StaticEntryCount(), 3U);
}

TEST(TextLayoutCacheTests, ResolveStaticTextTracksAlignmentInCacheKeyAndOrigin)
{
    ResetCounters();
    mfd::TextLayoutCache cache(&FakeMeasureText, &FakeFormatTime);
    const Font font = MakeFont();

    const mfd::CachedTextLayout& left = cache.ResolveStaticText("ALT", font, 14.0f, 2.0f, mfd::Align::Left);
    const mfd::CachedTextLayout& right = cache.ResolveStaticText("ALT", font, 14.0f, 2.0f, mfd::Align::Right);

    EXPECT_EQ(gMeasureCallCount, 2);
    EXPECT_FLOAT_EQ(left.origin.x, 0.0f);
    EXPECT_FLOAT_EQ(right.origin.x, right.size.x);
    EXPECT_NE(&left, &right);
}

TEST(TextLayoutCacheTests, ResolveTimeTextReusesFormattedLayoutWithinSameSecond)
{
    ResetCounters();
    mfd::TextLayoutCache cache(&FakeMeasureText, &FakeFormatTime);
    const Font font = MakeFont();
    mfd::TimeGeometry geometry;
    geometry.format = "clock";
    geometry.utc = true;

    const auto firstSecond = std::chrono::system_clock::from_time_t(1'700'000'000);
    const auto sameSecond = firstSecond + std::chrono::milliseconds(400);
    const auto nextSecond = firstSecond + std::chrono::seconds(1);

    const mfd::CachedTextLayout& first = cache.ResolveTimeText(geometry, font, 18.0f, 1.0f, firstSecond);
    const mfd::CachedTextLayout& second = cache.ResolveTimeText(geometry, font, 18.0f, 1.0f, sameSecond);
    EXPECT_EQ(&first, &second);
    EXPECT_EQ(gFormatCallCount, 1);
    EXPECT_EQ(gMeasureCallCount, 1);

    mfd::TimeGeometry localGeometry = geometry;
    localGeometry.utc = false;
    cache.ResolveTimeText(localGeometry, font, 18.0f, 1.0f, sameSecond);
    localGeometry = geometry;
    localGeometry.align = mfd::Align::Right;
    cache.ResolveTimeText(localGeometry, font, 18.0f, 1.0f, sameSecond);
    cache.ResolveTimeText(geometry, font, 18.0f, 1.0f, nextSecond);

    EXPECT_EQ(gFormatCallCount, 4);
    EXPECT_EQ(gMeasureCallCount, 4);

    const mfd::TextLayoutCache::Stats stats = cache.CacheStats();
    EXPECT_EQ(stats.timeMisses, 4U);
    EXPECT_EQ(stats.timeHits, 1U);
    EXPECT_EQ(cache.TimeEntryCount(), 4U);
}
