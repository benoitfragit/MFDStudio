/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for PolylineFillCache.
 */

#include "PolylineFillCache.h"

#include <cstdint>
#include <cstring>
#include <iterator>
#include <utility>

#include "PolygonTriangulation.h"

namespace mfd
{
namespace
{
constexpr std::size_t kMaxCachedPolylineFills = 256U;
constexpr std::uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::uint32_t FloatBits(const float value) noexcept
{
    static_assert(sizeof(float) == sizeof(std::uint32_t));

    std::uint32_t bits = 0U;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void HashCombine(std::uint64_t& hash, const std::uint32_t value) noexcept
{
    hash ^= value;
    hash *= kFnvPrime;
}

bool FloatBitsEqual(const float lhs, const float rhs) noexcept
{
    return FloatBits(lhs) == FloatBits(rhs);
}
} // namespace

const PolylineFillCache::Result& PolylineFillCache::Resolve(const ArrayView<const Vector2> points)
{
    const std::size_t fingerprint = ComputeFingerprint(points);
    const auto matchingRange = entries_.equal_range(fingerprint);
    for (auto iterator = matchingRange.first; iterator != matchingRange.second; ++iterator)
    {
        if (!PointsEqual(points, iterator->second.points))
        {
            continue;
        }

        ++stats_.hits;
        iterator->second.lastUseSerial = nextUseSerial_++;
        return iterator->second.result;
    }

    if (entries_.size() >= kMaxCachedPolylineFills)
    {
        EvictLeastRecentlyUsed();
    }

    Entry entry;
    entry.lastUseSerial = nextUseSerial_++;
    entry.points.assign(points.begin(), points.end());
    entry.result = BuildResult(points);
    ++stats_.misses;
    return entries_.emplace(fingerprint, std::move(entry))->second.result;
}

void PolylineFillCache::Clear() noexcept
{
    entries_.clear();
    nextUseSerial_ = 1U;
    stats_ = {};
}

std::size_t PolylineFillCache::EntryCount() const noexcept
{
    return entries_.size();
}

PolylineFillCache::Stats PolylineFillCache::CacheStats() const noexcept
{
    return stats_;
}

std::size_t PolylineFillCache::ComputeFingerprint(const ArrayView<const Vector2> points) noexcept
{
    std::uint64_t hash = kFnvOffsetBasis;
    HashCombine(hash, static_cast<std::uint32_t>(points.size()));
    for (const Vector2& point : points)
    {
        HashCombine(hash, FloatBits(point.x));
        HashCombine(hash, FloatBits(point.y));
    }

    return static_cast<std::size_t>(hash);
}

bool PolylineFillCache::PointsEqual(const ArrayView<const Vector2> lhs,
                                    const std::vector<Vector2>& rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (std::size_t index = 0U; index < lhs.size(); ++index)
    {
        if (!FloatBitsEqual(lhs[index].x, rhs[index].x) ||
            !FloatBitsEqual(lhs[index].y, rhs[index].y))
        {
            return false;
        }
    }
    return true;
}

PolylineFillCache::Result PolylineFillCache::BuildResult(const ArrayView<const Vector2> points)
{
    Result result;
    if (detail::PolygonIsConvex(points))
    {
        result.kind = FillKind::Convex;
        return result;
    }

    if (detail::TriangulateSimplePolygon(points, result.triangleIndices))
    {
        result.kind = FillKind::Triangulated;
    }
    return result;
}

void PolylineFillCache::EvictLeastRecentlyUsed() noexcept
{
    if (entries_.empty())
    {
        return;
    }

    auto oldest = entries_.begin();
    for (auto iterator = std::next(entries_.begin()); iterator != entries_.end(); ++iterator)
    {
        if (iterator->second.lastUseSerial < oldest->second.lastUseSerial)
        {
            oldest = iterator;
        }
    }
    entries_.erase(oldest);
}
} // namespace mfd
