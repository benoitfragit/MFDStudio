/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal cache for measured text layouts and formatted time strings.
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <raylib.h>

#include "mfd/model/Reticle.h"

namespace mfd
{
/**
 * @brief Measured text payload reused by the renderer across frames.
 */
struct CachedTextLayout
{
    /** @brief Text rendered by the layout. */
    std::string text {};
    /** @brief Full measured text bounds in pixels. */
    Vector2 size {};
    /** @brief Alignment-aware origin used by `DrawTextPro`. */
    Vector2 origin {};
};

/**
 * @brief Reuses measured layouts for static text and second-granularity time primitives.
 *
 * @note This helper stays private to the raylib renderer. It avoids repeated
 * `MeasureTextEx` and `strftime` work while keeping the public model and draw
 * behaviour unchanged.
 *
 * @note Entries are indexed by a 64-bit fingerprint of the lookup inputs so a
 * cache hit never allocates. Every hit re-verifies the stored inputs against
 * the probe; a fingerprint collision is therefore handled as a miss that
 * rebuilds and replaces the colliding entry.
 */
class TextLayoutCache
{
public:
    /**
     * @brief Lightweight cache counters exposed to unit tests.
     */
    struct Stats
    {
        std::size_t staticHits = 0U;
        std::size_t staticMisses = 0U;
        std::size_t timeHits = 0U;
        std::size_t timeMisses = 0U;
    };

    /**
     * @brief Callback used to measure one text run.
     */
    using MeasureTextCallback = Vector2 (*)(std::string_view text,
                                            const Font& font,
                                            float fontSize,
                                            float letterSpacing);

    /**
     * @brief Callback used to format one time primitive for one second bucket.
     */
    using FormatTimeCallback = std::string (*)(const TimeGeometry& geometry, std::time_t second);

    /**
     * @brief Creates one cache with optional injectable formatting and measurement callbacks.
     * @param measureText Optional measurement callback used on cache misses.
     * @param formatTime Optional time-format callback used on time cache misses.
     */
    explicit TextLayoutCache(MeasureTextCallback measureText = nullptr,
                             FormatTimeCallback formatTime = nullptr) noexcept;

    /**
     * @brief Returns one cached layout for static text.
     * @param text Text payload to render.
     * @param font Font used to measure and draw the text.
     * @param fontSize Font size in pixels.
     * @param letterSpacing Letter spacing in pixels.
     * @param align Horizontal alignment resolved by the caller.
     * @return Cached layout owned by the cache.
     */
    const CachedTextLayout& ResolveStaticText(std::string_view text,
                                              const Font& font,
                                              float fontSize,
                                              float letterSpacing,
                                              Align align);

    /**
     * @brief Returns one cached layout for a time primitive in one second bucket.
     * @param geometry Time primitive geometry to format.
     * @param font Font used to measure and draw the text.
     * @param fontSize Font size in pixels.
     * @param letterSpacing Letter spacing in pixels.
     * @param now Current time point resolved by the caller.
     * @return Cached layout owned by the cache.
     */
    const CachedTextLayout& ResolveTimeText(const TimeGeometry& geometry,
                                            const Font& font,
                                            float fontSize,
                                            float letterSpacing,
                                            std::chrono::system_clock::time_point now =
                                                std::chrono::system_clock::now());

    /**
     * @brief Clears every cached text and time layout.
     */
    void Clear() noexcept;

    /**
     * @brief Returns the number of cached static-text layouts currently retained.
     * @return Number of static-text entries.
     */
    [[nodiscard]] std::size_t StaticEntryCount() const noexcept;

    /**
     * @brief Returns the number of cached time layouts currently retained.
     * @return Number of time entries.
     */
    [[nodiscard]] std::size_t TimeEntryCount() const noexcept;

    /**
     * @brief Returns cumulative cache hit and miss counters.
     * @return Current cache statistics.
     */
    [[nodiscard]] Stats CacheStats() const noexcept;

private:
    /**
     * @brief Cached static-text layout with the inputs verified on every fingerprint hit.
     *
     * @note `layout.text` stores the exact key text, so no separate text copy is kept.
     */
    struct StaticEntry
    {
        std::size_t fontFingerprint = 0U;
        std::uint32_t fontSizeBits = 0U;
        std::uint32_t letterSpacingBits = 0U;
        Align align = Align::Center;
        std::size_t lastUseSerial = 0U;
        CachedTextLayout layout {};
    };

    /**
     * @brief Cached time layout with the inputs verified on every fingerprint hit.
     */
    struct TimeEntry
    {
        std::string format {};
        bool utc = false;
        std::time_t second = 0;
        std::optional<TimeValue> valueOverride {};
        std::optional<TimeFieldVisibility> structuredFields {};
        std::size_t fontFingerprint = 0U;
        std::uint32_t fontSizeBits = 0U;
        std::uint32_t letterSpacingBits = 0U;
        Align align = Align::Center;
        std::size_t lastUseSerial = 0U;
        CachedTextLayout layout {};
    };

    static std::uint64_t FingerprintStaticText(std::string_view text,
                                               std::size_t fontFingerprint,
                                               std::uint32_t fontSizeBits,
                                               std::uint32_t letterSpacingBits,
                                               Align align) noexcept;
    static bool MatchesStaticEntry(const StaticEntry& entry,
                                   std::string_view text,
                                   std::size_t fontFingerprint,
                                   std::uint32_t fontSizeBits,
                                   std::uint32_t letterSpacingBits,
                                   Align align) noexcept;
    static std::uint64_t FingerprintTimeText(const TimeGeometry& geometry,
                                             bool utc,
                                             std::time_t second,
                                             std::size_t fontFingerprint,
                                             std::uint32_t fontSizeBits,
                                             std::uint32_t letterSpacingBits) noexcept;
    static bool MatchesTimeEntry(const TimeEntry& entry,
                                 const TimeGeometry& geometry,
                                 bool utc,
                                 std::time_t second,
                                 std::size_t fontFingerprint,
                                 std::uint32_t fontSizeBits,
                                 std::uint32_t letterSpacingBits) noexcept;

    static Vector2 MeasureTextWithRaylib(std::string_view text,
                                         const Font& font,
                                         float fontSize,
                                         float letterSpacing);
    static std::string FormatTimeWithStrftime(const TimeGeometry& geometry, std::time_t second);
    static CachedTextLayout BuildLayout(std::string text,
                                        const Font& font,
                                        float fontSize,
                                        float letterSpacing,
                                        Align align,
                                        MeasureTextCallback measureText);

    template <typename Map>
    static void EvictLeastRecentlyUsed(Map& entries) noexcept;

    MeasureTextCallback measureText_ = nullptr;
    FormatTimeCallback formatTime_ = nullptr;
    std::unordered_map<std::uint64_t, StaticEntry> staticEntries_ {};
    std::unordered_map<std::uint64_t, TimeEntry> timeEntries_ {};
    std::size_t nextUseSerial_ = 1U;
    Stats stats_ {};
};
} // namespace mfd
