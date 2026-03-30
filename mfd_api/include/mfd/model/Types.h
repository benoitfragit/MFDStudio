/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Core math and view types shared by the public MFD API.
 */

#include <cstddef>
#include <cstdint>

#include "mfd/MfdExport.h"

namespace mfd
{
/**
 * @brief Hash helper for enum classes used as keys in unordered containers.
 */
struct EnumClassHash
{
    template <typename EnumType>
    std::size_t operator()(const EnumType value) const noexcept
    {
        return static_cast<std::size_t>(value);
    }
};

/**
 * @brief Two-dimensional vector expressed in normalized logical coordinates.
 *
 * @note In the MFD model, `(0, 0)` is the center of the page and the logical
 * range is typically `[-1, 1]` on both axes.
 */
struct Vec2
{
    /** @brief Horizontal coordinate in normalized logical space. */
    float x = 0.0f;
    /** @brief Vertical coordinate in normalized logical space. */
    float y = 0.0f;
};

/**
 * @brief RGBA color stored on 8 bits per channel.
 */
struct ColorRgba
{
    /** @brief Red channel. */
    std::uint8_t r = 0;
    /** @brief Green channel. */
    std::uint8_t g = 255;
    /** @brief Blue channel. */
    std::uint8_t b = 0;
    /** @brief Alpha channel. */
    std::uint8_t a = 255;
};

/**
 * @brief Generic 2D transform combining translation, rotation and scale.
 */
struct Transform2D
{
    /** @brief Translation expressed in logical coordinates. */
    Vec2 position {};
    /** @brief Counter-clockwise rotation in degrees. */
    float rotationDegrees = 0.0f;
    /** @brief Non-uniform scale applied on each axis. */
    Vec2 scale {1.0f, 1.0f};
};

/**
 * @brief View parameters applied to a page before rendering.
 */
struct PageViewState
{
    /** @brief Logical point placed at the center of the viewport. */
    Vec2 center {};
    /** @brief Zoom factor applied to the page. `1.0f` keeps the original scale. */
    float zoom = 1.0f;
};

/**
 * @brief Adds two vectors component-wise.
 * @param lhs Left-hand operand.
 * @param rhs Right-hand operand.
 * @return Sum of the two vectors.
 */
MFD_API Vec2 operator+(const Vec2& lhs, const Vec2& rhs) noexcept;

/**
 * @brief Subtracts two vectors component-wise.
 * @param lhs Left-hand operand.
 * @param rhs Right-hand operand.
 * @return Difference between the two vectors.
 */
MFD_API Vec2 operator-(const Vec2& lhs, const Vec2& rhs) noexcept;

/**
 * @brief Multiplies a vector by a scalar.
 * @param lhs Vector to scale.
 * @param scalar Scalar factor.
 * @return Scaled vector.
 */
MFD_API Vec2 operator*(const Vec2& lhs, float scalar) noexcept;

/**
 * @brief Applies a non-uniform scale to a vector.
 * @param value Vector to scale.
 * @param scale Scale to apply on each axis.
 * @return Scaled vector.
 */
MFD_API Vec2 Scale(const Vec2& value, const Vec2& scale) noexcept;

/**
 * @brief Rotates a vector around the origin.
 * @param value Vector to rotate.
 * @param degrees Rotation angle in degrees, counter-clockwise.
 * @return Rotated vector.
 */
MFD_API Vec2 Rotate(const Vec2& value, float degrees) noexcept;

/**
 * @brief Applies a full 2D transform to a point.
 * @param point Point expressed in local coordinates.
 * @param transform Transform to apply.
 * @return Transformed point.
 */
MFD_API Vec2 ApplyTransform(const Vec2& point, const Transform2D& transform) noexcept;

/**
 * @brief Applies the page view center and zoom to a logical point.
 * @param point Point expressed in page logical coordinates.
 * @param view Page view state.
 * @return Point expressed in view space.
 */
MFD_API Vec2 ApplyPageView(const Vec2& point, const PageViewState& view) noexcept;

/**
 * @brief Combines a parent transform with a child transform.
 * @param parent Parent transform.
 * @param child Child transform.
 * @return Combined transform equivalent to applying the child in the parent space.
 */
MFD_API Transform2D CombineTransforms(const Transform2D& parent, const Transform2D& child) noexcept;

/**
 * @brief Returns an average absolute scale factor for two chained transforms.
 * @param parent Parent transform.
 * @param child Child transform.
 * @return Average scale value across X and Y.
 */
MFD_API float AverageScale(const Transform2D& parent, const Transform2D& child) noexcept;

/**
 * @brief Sanitizes a page zoom level.
 * @param zoom Requested zoom value.
 * @return A valid strictly positive zoom, or `1.0f` if the input is invalid.
 */
MFD_API float SanitizeZoom(float zoom) noexcept;
} // namespace mfd
