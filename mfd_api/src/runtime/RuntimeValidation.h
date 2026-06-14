/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Shared runtime safety budgets reused by JSON loading and live scene mutation validation.
 */

#include <cmath>
#include <cstddef>
#include <string_view>
#include <vector>

#include "mfd/model/Reticle.h"
#include "mfd/model/Types.h"

namespace mfd::runtime_validation
{
/** @brief Maximum absolute logical coordinate accepted by the runtime safety budget. */
constexpr float kMaxAbsCoordinate = 1'000'000.0f;
/** @brief Maximum absolute scale accepted by the runtime safety budget. */
constexpr float kMaxAbsScale = 1'000.0f;
/** @brief Maximum absolute rotation angle accepted by the runtime safety budget. */
constexpr float kMaxAbsAngleDegrees = 1'000'000.0f;
/** @brief Maximum logical size accepted by the runtime safety budget. */
constexpr float kMaxLogicalSize = 1'000'000.0f;
/** @brief Maximum stroke thickness accepted by the runtime safety budget. */
constexpr float kMaxThickness = 100.0f;
/** @brief Maximum page zoom accepted by the runtime safety budget. */
constexpr float kMaxZoom = 1'000.0f;
/** @brief Maximum number of generated line segments accepted by the runtime safety budget. */
constexpr int kMaxPrimitiveSegments = 1024;
/** @brief Maximum number of points accepted by runtime polyline and bezier payloads. */
constexpr std::size_t kMaxPrimitivePoints = 2048U;
/**
 * @brief Maximum number of bezier control points accepted by the runtime.
 *
 * @details Bezier sampling evaluates De Casteljau in `O(n^2)` per sampled point and is repeated for
 * every segment, so the cost grows as `O(n^2 * segments)`. A high control-point count combined with a
 * high segment count can stall a frame for seconds. A degree well below this bound already covers every
 * practical authored curve, so the budget caps `n` far lower than @ref kMaxPrimitivePoints to keep
 * bezier rebuilds bounded on the JSON, command and editor paths.
 */
constexpr std::size_t kMaxBezierControlPoints = 64U;
/** @brief Maximum number of points accepted by one filled polygon path. */
constexpr std::size_t kMaxFilledPolygonPoints = 512U;
/** @brief Maximum number of UTF-8 bytes accepted by runtime text payloads. */
constexpr std::size_t kMaxTextBytes = 4096U;
/** @brief Minimum accepted year for runtime numeric time overrides. */
constexpr int kMinTimeYear = 1;
/** @brief Maximum accepted year for runtime numeric time overrides. */
constexpr int kMaxTimeYear = 9999;

/**
 * @brief Returns whether one logical vector contains only finite coordinates.
 * @param value Logical vector to inspect.
 * @return `true` when both coordinates are finite.
 */
inline bool IsFiniteVec2(const Vec2& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

/**
 * @brief Returns whether one scalar is finite and stays within a symmetric absolute bound.
 * @param value Scalar to inspect.
 * @param maxAbs Maximum accepted absolute value.
 * @return `true` when the scalar is finite and within bounds.
 */
inline bool IsFiniteAbsWithin(const float value, const float maxAbs) noexcept
{
    return std::isfinite(value) && std::abs(value) <= maxAbs;
}

/**
 * @brief Returns whether one scalar is positive, finite, and bounded.
 * @param value Scalar to inspect.
 * @param maxValue Maximum accepted value.
 * @return `true` when the scalar is in `(0, maxValue]`.
 */
inline bool IsPositiveFiniteWithin(const float value, const float maxValue) noexcept
{
    return std::isfinite(value) && value > 0.0f && value <= maxValue;
}

/**
 * @brief Returns whether one logical vector stays finite and within the configured bound.
 * @param value Logical vector to inspect.
 * @param maxAbs Maximum accepted absolute coordinate.
 * @return `true` when both coordinates are finite and within bounds.
 */
inline bool IsValidVec2(const Vec2& value, const float maxAbs = kMaxAbsCoordinate) noexcept
{
    return IsFiniteVec2(value) &&
           std::abs(value.x) <= maxAbs &&
           std::abs(value.y) <= maxAbs;
}

/**
 * @brief Returns whether one logical scale vector stays finite and within the runtime budget.
 * @param value Scale vector to inspect.
 * @return `true` when both scale axes stay within the scale budget.
 */
inline bool IsValidScale(const Vec2& value) noexcept
{
    return IsValidVec2(value, kMaxAbsScale);
}

/**
 * @brief Returns whether one text payload stays within the runtime byte budget.
 * @param value UTF-8 payload to inspect.
 * @return `true` when the payload size does not exceed the runtime budget.
 */
inline bool IsValidTextPayload(const std::string_view value) noexcept
{
    return value.size() <= kMaxTextBytes;
}

/**
 * @brief Returns whether a Gregorian year is a leap year.
 * @param year Year to inspect.
 * @return `true` when February has 29 days.
 */
inline bool IsLeapYear(const int year) noexcept
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * @brief Returns the accepted day count for one month.
 * @param year Gregorian year.
 * @param month Month in [1, 12].
 * @return Number of days in the month, or 31 for out-of-range months.
 */
inline int DaysInMonth(const int year, const int month) noexcept
{
    switch (month)
    {
    case 2:
        return IsLeapYear(year) ? 29 : 28;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    default:
        return 31;
    }
}

/**
 * @brief Returns whether one structured time field selection displays something.
 * @param fields Field selection to inspect.
 * @return `true` when at least one field is visible.
 */
inline bool HasVisibleTimeField(const TimeFieldVisibility& fields) noexcept
{
    return fields.year || fields.month || fields.day || fields.hour || fields.minute || fields.second;
}

/**
 * @brief Returns whether one numeric runtime time value is in accepted bounds.
 * @param value Numeric value to inspect.
 * @return `true` when the date and clock components are valid.
 */
inline bool IsValidTimeValue(const TimeValue& value) noexcept
{
    if (value.year < kMinTimeYear || value.year > kMaxTimeYear)
    {
        return false;
    }

    if (value.month < 1 || value.month > 12)
    {
        return false;
    }

    if (value.day < 1 || value.day > DaysInMonth(value.year, value.month))
    {
        return false;
    }

    return value.hour >= 0 && value.hour <= 23 &&
           value.minute >= 0 && value.minute <= 59 &&
           value.second >= 0 && value.second <= 59;
}

/**
 * @brief Returns whether one point list stays within the runtime count and coordinate budgets.
 * @param points Point list to inspect.
 * @param minimumCount Minimum number of points required by the caller.
 * @return `true` when the point list is large enough, bounded, and finite.
 */
inline bool AreValidPoints(const std::vector<Vec2>& points, const std::size_t minimumCount) noexcept
{
    if (points.size() < minimumCount || points.size() > kMaxPrimitivePoints)
    {
        return false;
    }

    for (const Vec2& point : points)
    {
        if (!IsValidVec2(point))
        {
            return false;
        }
    }

    return true;
}
} // namespace mfd::runtime_validation
