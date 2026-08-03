/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal cache flattening Bézier primitives into reusable polylines.
 */

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "mfd/model/Reticle.h"

namespace mfd
{
/** @brief Maximum number of De Casteljau interpolations used to flatten one curve. */
constexpr std::size_t kMaxBezierLerpOperations = 131072U;

/**
 * @brief Reuses sampled Bézier polylines across frames while geometry stays unchanged.
 *
 * @note This helper is internal to the raylib render layer. It keeps the
 * public model untouched while avoiding repeated `O(segments * n^2)`
 * flattening work for high-degree Bézier primitives.
 */
class BezierPolylineCache
{
public:
    /**
     * @brief Lightweight cache counters exposed to unit tests.
     */
    struct Stats
    {
        std::size_t hits = 0U;
        std::size_t misses = 0U;
    };

    /**
     * @brief Returns the sampled polyline for one Bézier geometry.
     * @param geometry Geometry to flatten.
     * @return Cached sampled points owned by the cache.
     */
    const std::vector<Vec2>& Resolve(const BezierGeometry& geometry);

    /**
     * @brief Clears every cached flattened polyline.
     */
    void Clear() noexcept;

    /**
     * @brief Returns the number of cached entries currently retained.
     * @return Number of cached Bézier instances.
     */
    [[nodiscard]] std::size_t EntryCount() const noexcept;

    /**
     * @brief Returns cumulative hit and miss counters.
     * @return Current cache statistics.
     */
    [[nodiscard]] Stats CacheStats() const noexcept;

    /**
     * @brief Flattens one Bézier into a polyline without using the cache.
     * @param geometry Geometry to flatten.
     * @param destination Output sampled points.
     */
    static void BuildPolyline(const BezierGeometry& geometry, std::vector<Vec2>& destination);

private:
    struct Entry
    {
        std::size_t fingerprint = 0U;
        std::size_t lastUseSerial = 0U;
        std::vector<Vec2> polyline {};
    };

    void EvictLeastRecentlyUsed() noexcept;
    static std::size_t ComputeFingerprint(const BezierGeometry& geometry) noexcept;

    std::unordered_map<const BezierGeometry*, Entry> entries_ {};
    std::size_t nextUseSerial_ = 1U;
    Stats stats_ {};
};
} // namespace mfd
