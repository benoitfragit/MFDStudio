/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal bounded cache for closed-polyline fill topology.
 */

#include <cstddef>
#include <unordered_map>
#include <vector>

#include <raylib.h>

#include "mfd/core/ArrayView.h"

namespace mfd
{
/**
 * @brief Reuses closed-polyline validation and triangulation across frames.
 *
 * @note Callers provide screen-relative points. Removing translation keeps
 * moving reticles cacheable while preserving the exact scale and rotation
 * used by the polygon predicates.
 */
class PolylineFillCache
{
public:
    /** @brief Fill strategy selected for a validated closed polyline. */
    enum class FillKind
    {
        Invalid,
        Convex,
        Triangulated
    };

    /** @brief Cached fill topology for one exact point sequence. */
    struct Result
    {
        FillKind kind = FillKind::Invalid;
        std::vector<std::size_t> triangleIndices {};
    };

    /** @brief Lightweight cache counters exposed to unit tests. */
    struct Stats
    {
        std::size_t hits = 0U;
        std::size_t misses = 0U;
    };

    /**
     * @brief Resolves the fill topology for one exact point sequence.
     * @param points Finite screen-relative polygon vertices in contour order.
     * @return Cache-owned result, valid until the next non-const cache operation.
     */
    const Result& Resolve(ArrayView<const Vector2> points);

    /** @brief Clears every cached topology and resets the statistics. */
    void Clear() noexcept;

    /**
     * @brief Returns the number of retained point sequences.
     * @return Current bounded cache size.
     */
    [[nodiscard]] std::size_t EntryCount() const noexcept;

    /**
     * @brief Returns cumulative hit and miss counters.
     * @return Current cache statistics.
     */
    [[nodiscard]] Stats CacheStats() const noexcept;

private:
    struct Entry
    {
        std::size_t lastUseSerial = 0U;
        std::vector<Vector2> points {};
        Result result {};
    };

    using EntryMap = std::unordered_multimap<std::size_t, Entry>;

    static std::size_t ComputeFingerprint(ArrayView<const Vector2> points) noexcept;
    static bool PointsEqual(ArrayView<const Vector2> lhs,
                            const std::vector<Vector2>& rhs) noexcept;
    static Result BuildResult(ArrayView<const Vector2> points);
    void EvictLeastRecentlyUsed() noexcept;

    EntryMap entries_ {};
    std::size_t nextUseSerial_ = 1U;
    Stats stats_ {};
};
} // namespace mfd
