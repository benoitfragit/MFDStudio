/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for TextLayoutCache.
 */

#include "TextLayoutCache.h"
#include "TimeFormatValidation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <utility>

namespace mfd
{
namespace
{
constexpr std::size_t kMaxStaticTextEntries = 512U;
constexpr std::size_t kMaxTimeTextEntries = 64U;
constexpr std::uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::uint32_t FloatBits(const float value) noexcept
{
    static_assert(sizeof(float) == sizeof(std::uint32_t));

    std::uint32_t bits = 0U;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void HashCombine(std::uint64_t& hash, const std::uint64_t value) noexcept
{
    hash ^= value;
    hash *= kFnvPrime;
}

std::size_t ComputeFontFingerprint(const Font& font) noexcept
{
    std::uint64_t hash = kFnvOffsetBasis;
    HashCombine(hash, static_cast<std::uint64_t>(font.texture.id));
    HashCombine(hash, static_cast<std::uint64_t>(font.baseSize));
    HashCombine(hash, static_cast<std::uint64_t>(font.glyphCount));
    HashCombine(hash, static_cast<std::uint64_t>(font.glyphPadding));
    return static_cast<std::size_t>(hash);
}

float HorizontalOrigin(const float width, const Align align) noexcept
{
    switch (align)
    {
    case Align::Left:
        return 0.0f;
    case Align::Right:
        return width;
    case Align::Center:
    default:
        return width * 0.5f;
    }
}

bool CanMeasureTextWithRaylib(const Font& font) noexcept
{
    return font.texture.id != 0 &&
           font.glyphs != nullptr &&
           font.recs != nullptr &&
           font.glyphCount > 0;
}

Vector2 EstimateTextSize(const std::string_view text,
                         const float fontSize,
                         const float letterSpacing) noexcept
{
    const std::size_t characterCount = std::max<std::size_t>(std::size_t {1}, text.size());
    const float safeFontSize = std::isfinite(fontSize) ? std::max(1.0f, std::abs(fontSize)) : 1.0f;
    const float safeLetterSpacing = std::isfinite(letterSpacing) ? std::max(0.0f, letterSpacing) : 0.0f;
    const float spacingWidth = safeLetterSpacing * static_cast<float>(characterCount - 1U);

    return Vector2 {
        safeFontSize * static_cast<float>(characterCount) * 0.60f + spacingWidth,
        safeFontSize};
}

std::tm ToCalendarTime(const std::time_t rawTime, const bool utc) noexcept
{
    std::tm calendarTime {};

#if defined(_WIN32)
    if (utc)
    {
        gmtime_s(&calendarTime, &rawTime);
    }
    else
    {
        localtime_s(&calendarTime, &rawTime);
    }
#else
    if (utc)
    {
        gmtime_r(&rawTime, &calendarTime);
    }
    else
    {
        localtime_r(&rawTime, &calendarTime);
    }
#endif

    return calendarTime;
}
} // namespace

TextLayoutCache::TextLayoutCache(const MeasureTextCallback measureText,
                                 const FormatTimeCallback formatTime) noexcept
    : measureText_(measureText != nullptr ? measureText : &TextLayoutCache::MeasureTextWithRaylib)
    , formatTime_(formatTime != nullptr ? formatTime : &TextLayoutCache::FormatTimeWithStrftime)
{
}

const CachedTextLayout& TextLayoutCache::ResolveStaticText(const std::string_view text,
                                                           const Font& font,
                                                           const float fontSize,
                                                           const float letterSpacing,
                                                           const Align align)
{
    StaticKey key;
    key.text = std::string(text);
    key.fontFingerprint = ComputeFontFingerprint(font);
    key.fontSizeBits = FloatBits(fontSize);
    key.letterSpacingBits = FloatBits(letterSpacing);
    key.align = align;

    auto iterator = staticEntries_.find(key);
    if (iterator != staticEntries_.end())
    {
        ++stats_.staticHits;
        iterator->second.lastUseSerial = nextUseSerial_++;
        return iterator->second.layout;
    }

    if (staticEntries_.size() >= kMaxStaticTextEntries)
    {
        EvictLeastRecentlyUsed(staticEntries_);
    }

    Entry entry;
    entry.lastUseSerial = nextUseSerial_++;
    entry.layout = BuildLayout(key.text, font, fontSize, letterSpacing, align, measureText_);
    ++stats_.staticMisses;
    return staticEntries_.emplace(std::move(key), std::move(entry)).first->second.layout;
}

const CachedTextLayout& TextLayoutCache::ResolveTimeText(const TimeGeometry& geometry,
                                                         const Font& font,
                                                         const float fontSize,
                                                         const float letterSpacing,
                                                         const std::chrono::system_clock::time_point now)
{
    TimeKey key;
    key.format = geometry.format;
    key.utc = geometry.utc;
    key.second = std::chrono::system_clock::to_time_t(now);
    key.fontFingerprint = ComputeFontFingerprint(font);
    key.fontSizeBits = FloatBits(fontSize);
    key.letterSpacingBits = FloatBits(letterSpacing);
    key.align = geometry.align;

    auto iterator = timeEntries_.find(key);
    if (iterator != timeEntries_.end())
    {
        ++stats_.timeHits;
        iterator->second.lastUseSerial = nextUseSerial_++;
        return iterator->second.layout;
    }

    if (timeEntries_.size() >= kMaxTimeTextEntries)
    {
        EvictLeastRecentlyUsed(timeEntries_);
    }

    Entry entry;
    entry.lastUseSerial = nextUseSerial_++;
    entry.layout = BuildLayout(formatTime_(geometry, key.second), font, fontSize, letterSpacing, geometry.align, measureText_);
    ++stats_.timeMisses;
    return timeEntries_.emplace(std::move(key), std::move(entry)).first->second.layout;
}

void TextLayoutCache::Clear() noexcept
{
    staticEntries_.clear();
    timeEntries_.clear();
    nextUseSerial_ = 1U;
    stats_ = {};
}

std::size_t TextLayoutCache::StaticEntryCount() const noexcept
{
    return staticEntries_.size();
}

std::size_t TextLayoutCache::TimeEntryCount() const noexcept
{
    return timeEntries_.size();
}

TextLayoutCache::Stats TextLayoutCache::CacheStats() const noexcept
{
    return stats_;
}

bool TextLayoutCache::StaticKey::operator==(const StaticKey& other) const noexcept
{
    return text == other.text &&
           fontFingerprint == other.fontFingerprint &&
           fontSizeBits == other.fontSizeBits &&
           letterSpacingBits == other.letterSpacingBits &&
           align == other.align;
}

bool TextLayoutCache::TimeKey::operator==(const TimeKey& other) const noexcept
{
    return format == other.format &&
           utc == other.utc &&
           second == other.second &&
           fontFingerprint == other.fontFingerprint &&
           fontSizeBits == other.fontSizeBits &&
           letterSpacingBits == other.letterSpacingBits &&
           align == other.align;
}

std::size_t TextLayoutCache::StaticKeyHasher::operator()(const StaticKey& key) const noexcept
{
    std::uint64_t hash = kFnvOffsetBasis;
    for (const char character : key.text)
    {
        HashCombine(hash, static_cast<std::uint64_t>(static_cast<unsigned char>(character)));
    }
    HashCombine(hash, static_cast<std::uint64_t>(key.fontFingerprint));
    HashCombine(hash, static_cast<std::uint64_t>(key.fontSizeBits));
    HashCombine(hash, static_cast<std::uint64_t>(key.letterSpacingBits));
    HashCombine(hash, static_cast<std::uint64_t>(key.align == Align::Left ? 0U : (key.align == Align::Right ? 2U : 1U)));
    return static_cast<std::size_t>(hash);
}

std::size_t TextLayoutCache::TimeKeyHasher::operator()(const TimeKey& key) const noexcept
{
    std::uint64_t hash = kFnvOffsetBasis;
    for (const char character : key.format)
    {
        HashCombine(hash, static_cast<std::uint64_t>(static_cast<unsigned char>(character)));
    }
    HashCombine(hash, static_cast<std::uint64_t>(key.utc ? 1U : 0U));
    HashCombine(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(key.second)));
    HashCombine(hash, static_cast<std::uint64_t>(key.fontFingerprint));
    HashCombine(hash, static_cast<std::uint64_t>(key.fontSizeBits));
    HashCombine(hash, static_cast<std::uint64_t>(key.letterSpacingBits));
    HashCombine(hash, static_cast<std::uint64_t>(key.align == Align::Left ? 0U : (key.align == Align::Right ? 2U : 1U)));
    return static_cast<std::size_t>(hash);
}

Vector2 TextLayoutCache::MeasureTextWithRaylib(const std::string_view text,
                                               const Font& font,
                                               const float fontSize,
                                               const float letterSpacing)
{
    if (!CanMeasureTextWithRaylib(font))
    {
        return EstimateTextSize(text, fontSize, letterSpacing);
    }

    const std::string ownedText(text);
    const Vector2 measuredSize = MeasureTextEx(font, ownedText.c_str(), fontSize, letterSpacing);
    if (!std::isfinite(measuredSize.x) || !std::isfinite(measuredSize.y))
    {
        return EstimateTextSize(text, fontSize, letterSpacing);
    }

    return measuredSize;
}

std::string TextLayoutCache::FormatTimeWithStrftime(const TimeGeometry& geometry, const std::time_t second)
{
    if (!detail::IsValidTimeFormatString(geometry.format))
    {
        return geometry.utc ? "UTC" : "--:--:--";
    }

    const std::tm calendarTime = ToCalendarTime(second, geometry.utc);

    std::array<char, 128> buffer {};
    if (std::strftime(buffer.data(), buffer.size(), geometry.format.c_str(), &calendarTime) == 0U)
    {
        return geometry.utc ? "UTC" : "--:--:--";
    }

    return std::string(buffer.data());
}

CachedTextLayout TextLayoutCache::BuildLayout(std::string text,
                                              const Font& font,
                                              const float fontSize,
                                              const float letterSpacing,
                                              const Align align,
                                              const MeasureTextCallback measureText)
{
    CachedTextLayout layout;
    layout.text = std::move(text);
    layout.size = measureText(layout.text, font, fontSize, letterSpacing);
    layout.origin = Vector2 {HorizontalOrigin(layout.size.x, align), layout.size.y * 0.5f};
    return layout;
}

template <typename Map>
void TextLayoutCache::EvictLeastRecentlyUsed(Map& entries) noexcept
{
    if (entries.empty())
    {
        return;
    }

    auto oldest = entries.begin();
    for (auto iterator = std::next(entries.begin()); iterator != entries.end(); ++iterator)
    {
        if (iterator->second.lastUseSerial < oldest->second.lastUseSerial)
        {
            oldest = iterator;
        }
    }

    entries.erase(oldest);
}
} // namespace mfd
