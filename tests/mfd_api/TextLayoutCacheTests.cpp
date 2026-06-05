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
#include <cmath>
#include <ctime>
#include <string>
#include <string_view>

#include "TextLayoutCache.h"
#include "TimeFormatValidation.h"

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
    std::string suffix = geometry.runtimeUtc.value_or(geometry.utc) ? "_utc_" : "_local_";
    if (geometry.runtimeValueOverride.has_value())
    {
        suffix += std::to_string(geometry.runtimeValueOverride->hour);
    }
    if (geometry.runtimeFields.has_value())
    {
        suffix += geometry.runtimeFields->second ? "_seconds" : "_no_seconds";
    }
    return geometry.format + suffix + "_" + std::to_string(static_cast<long long>(second));
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

TEST(TextLayoutCacheTests, ResolveStaticTextFallsBackWhenRaylibFontHasNoGlyphs)
{
    mfd::TextLayoutCache cache;
    const Font invalidFont {};

    const mfd::CachedTextLayout& layout =
        cache.ResolveStaticText("UTC", invalidFont, 14.0f, 2.0f, mfd::Align::Right);

    EXPECT_EQ(layout.text, "UTC");
    EXPECT_TRUE(std::isfinite(layout.size.x));
    EXPECT_TRUE(std::isfinite(layout.size.y));
    EXPECT_GT(layout.size.x, 0.0f);
    EXPECT_GT(layout.size.y, 0.0f);
    EXPECT_FLOAT_EQ(layout.origin.x, layout.size.x);
}

TEST(TextLayoutCacheTests, TimeFormatValidationRejectsIncompleteAndUnknownDirectives)
{
    EXPECT_TRUE(mfd::detail::IsValidTimeFormatString("%H:%M:%S"));
    EXPECT_TRUE(mfd::detail::IsValidTimeFormatString("UTC %Y-%m-%d %H:%M"));
    EXPECT_TRUE(mfd::detail::IsValidTimeFormatString("%%H"));
#if defined(_WIN32)
    EXPECT_TRUE(mfd::detail::IsValidTimeFormatString("%#H:%M"));
#endif

    EXPECT_FALSE(mfd::detail::IsValidTimeFormatString(""));
    EXPECT_FALSE(mfd::detail::IsValidTimeFormatString("%"));
    EXPECT_FALSE(mfd::detail::IsValidTimeFormatString("%H:%M:%"));
    EXPECT_FALSE(mfd::detail::IsValidTimeFormatString("%Q"));
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

TEST(TextLayoutCacheTests, ResolveTimeTextFallsBackWhenFormatIsInvalid)
{
    ResetCounters();
    mfd::TextLayoutCache cache(&FakeMeasureText);
    const Font font = MakeFont();
    mfd::TimeGeometry geometry;
    geometry.format = "%H:%M:%";
    geometry.utc = false;

    const auto second = std::chrono::system_clock::from_time_t(1'700'000'000);
    const mfd::CachedTextLayout& layout = cache.ResolveTimeText(geometry, font, 18.0f, 1.0f, second);

    EXPECT_EQ(layout.text, "--:--:--");
    EXPECT_EQ(gMeasureCallCount, 1);
    EXPECT_EQ(gFormatCallCount, 0);
}

TEST(TextLayoutCacheTests, ResolveTimeTextTracksRuntimeTimeStateInCacheKey)
{
    ResetCounters();
    mfd::TextLayoutCache cache(&FakeMeasureText, &FakeFormatTime);
    const Font font = MakeFont();
    mfd::TimeGeometry geometry;
    geometry.format = "clock";

    const auto second = std::chrono::system_clock::from_time_t(1'700'000'000);
    cache.ResolveTimeText(geometry, font, 18.0f, 1.0f, second);

    geometry.runtimeUtc = true;
    cache.ResolveTimeText(geometry, font, 18.0f, 1.0f, second);

    geometry.runtimeValueOverride = mfd::TimeValue {2026, 6, 5, 14, 3, 9};
    cache.ResolveTimeText(geometry, font, 18.0f, 1.0f, second);

    geometry.runtimeFields = mfd::TimeFieldVisibility {false, false, false, true, true, false};
    cache.ResolveTimeText(geometry, font, 18.0f, 1.0f, second);

    EXPECT_EQ(gFormatCallCount, 4);
    EXPECT_EQ(gMeasureCallCount, 4);
    EXPECT_EQ(cache.TimeEntryCount(), 4U);
}

TEST(TextLayoutCacheTests, ResolveTimeTextFormatsStructuredNumericOverride)
{
    ResetCounters();
    mfd::TextLayoutCache cache(&FakeMeasureText);
    const Font font = MakeFont();
    mfd::TimeGeometry geometry;
    geometry.runtimeValueOverride = mfd::TimeValue {2026, 6, 5, 14, 3, 9};
    geometry.runtimeFields = mfd::TimeFieldVisibility {true, true, true, true, true, false};

    const auto second = std::chrono::system_clock::from_time_t(1'700'000'000);
    const mfd::CachedTextLayout& layout = cache.ResolveTimeText(geometry, font, 18.0f, 1.0f, second);

    EXPECT_EQ(layout.text, "2026-06-05 14:03");
    EXPECT_EQ(gMeasureCallCount, 1);
    EXPECT_EQ(gFormatCallCount, 0);
}
