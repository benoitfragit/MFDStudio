/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for BezierPolylineCache.
 */

#include "BezierPolylineCache.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>

namespace mfd
{
namespace
{
constexpr int kMaxPrimitiveSegments = 1024;
constexpr std::size_t kMaxCachedBezierEntries = 256U;
constexpr std::uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

int SanitizeSegmentCount(const int requested, const int minimum) noexcept
{
    return std::clamp(requested, minimum, kMaxPrimitiveSegments);
}

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

Vec2 Lerp(const Vec2& lhs, const Vec2& rhs, const float factor) noexcept
{
    return {
        lhs.x + (rhs.x - lhs.x) * factor,
        lhs.y + (rhs.y - lhs.y) * factor};
}

Vec2 EvaluateBezier(const std::vector<Vec2>& controlPoints,
                    const float factor,
                    std::vector<Vec2>& workingPoints)
{
    if (controlPoints.empty())
    {
        return {};
    }

    workingPoints.assign(controlPoints.begin(), controlPoints.end());

    for (std::size_t activePointCount = workingPoints.size(); activePointCount > 1; --activePointCount)
    {
        for (std::size_t index = 0; index + 1U < activePointCount; ++index)
        {
            workingPoints[index] = Lerp(workingPoints[index], workingPoints[index + 1U], factor);
        }
    }

    return workingPoints.front();
}
} // namespace

const std::vector<Vec2>& BezierPolylineCache::Resolve(const BezierGeometry& geometry)
{
    const std::size_t fingerprint = ComputeFingerprint(geometry);
    auto iterator = entries_.find(&geometry);
    if (iterator != entries_.end() && iterator->second.fingerprint == fingerprint)
    {
        ++stats_.hits;
        iterator->second.lastUseSerial = nextUseSerial_++;
        return iterator->second.polyline;
    }

    if (iterator == entries_.end() && entries_.size() >= kMaxCachedBezierEntries)
    {
        EvictLeastRecentlyUsed();
    }

    Entry& entry = entries_[&geometry];
    BuildPolyline(geometry, entry.polyline);
    entry.fingerprint = fingerprint;
    entry.lastUseSerial = nextUseSerial_++;
    ++stats_.misses;
    return entry.polyline;
}

void BezierPolylineCache::Clear() noexcept
{
    entries_.clear();
    nextUseSerial_ = 1U;
    stats_ = {};
}

std::size_t BezierPolylineCache::EntryCount() const noexcept
{
    return entries_.size();
}

BezierPolylineCache::Stats BezierPolylineCache::CacheStats() const noexcept
{
    return stats_;
}

void BezierPolylineCache::BuildPolyline(const BezierGeometry& geometry, std::vector<Vec2>& destination)
{
    if (geometry.controlPoints.size() <= 1U)
    {
        destination = geometry.controlPoints;
        return;
    }

    const int segmentCount = SanitizeSegmentCount(geometry.segments, 2);
    destination.clear();
    destination.reserve(static_cast<std::size_t>(segmentCount) + 1U);

    std::vector<Vec2> workingPoints;
    workingPoints.reserve(geometry.controlPoints.size());

    for (int index = 0; index <= segmentCount; ++index)
    {
        const float factor = static_cast<float>(index) / static_cast<float>(segmentCount);
        destination.push_back(EvaluateBezier(geometry.controlPoints, factor, workingPoints));
    }
}

void BezierPolylineCache::EvictLeastRecentlyUsed() noexcept
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

std::size_t BezierPolylineCache::ComputeFingerprint(const BezierGeometry& geometry) noexcept
{
    std::uint64_t hash = kFnvOffsetBasis;
    HashCombine(hash, static_cast<std::uint32_t>(geometry.segments));
    HashCombine(hash, static_cast<std::uint32_t>(geometry.controlPoints.size()));

    for (const Vec2& point : geometry.controlPoints)
    {
        HashCombine(hash, FloatBits(point.x));
        HashCombine(hash, FloatBits(point.y));
    }

    return static_cast<std::size_t>(hash);
}
} // namespace mfd
