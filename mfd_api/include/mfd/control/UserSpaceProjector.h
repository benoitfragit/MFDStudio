/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Client-side helper projecting a user-space frame into MFD page coordinates.
 */

#include "mfd/MfdExport.h"
#include "mfd/model/Types.h"

namespace mfd
{
/**
 * @brief Client-side frame used to project user-space data to one MFD page.
 *
 * @note This frame is not known by the window runtime. It is only a helper for
 * external applications that work in a domain-specific coordinate system and
 * need to convert positions and rotations before sending commands.
 */
struct UserSpaceFrame
{
    /**
     * @brief User-space origin mapped to @ref pageAnchor.
     *
     * @note This is the origin of the client-side reference frame expressed in
     * user-space units. In many integrations this can simply stay at
     * `{0.0f, 0.0f}`. It becomes useful when the client wants to project a
     * moving local frame, for example one centered on an aircraft, a vehicle,
     * or a sensor.
     */
    Vec2 userOrigin {};

    /**
     * @brief Logical page position where @ref userOrigin appears.
     *
     * @note This value is expressed in the usual MFD logical space `[-1, 1]`.
     * For example, setting `pageAnchor = {0.0f, -0.3f}` means that the
     * user-space origin `(0, 0)` must appear slightly below the page center.
     */
    Vec2 pageAnchor {};

    /**
     * @brief Rotation of the user-space frame origin, expressed in radians.
     *
     * @note This rotation is removed from projected positions and rotations so
     * that the page can stay aligned with the moving origin frame.
     */
    float originRotationRadians = 0.0f;

    /**
     * @brief Scale converting one user-space unit to MFD page units.
     *
     * @note This field answers the question: "how many page units does one
     * user-space unit represent?"
     *
     * Example:
     * - if one user-space unit is one nautical mile
     * - and `pageUnitsPerUserUnit = 0.04f`
     * then:
     * - `1 NM` becomes `0.04` in page coordinates
     * - `5 NM` becomes `0.20`
     * - `10 NM` becomes `0.40`
     */
    float pageUnitsPerUserUnit = 1.0f;

    /**
     * @brief Page direction representing the positive user-space X axis.
     *
     * @note The projector orthonormalizes the user-space basis internally.
     */
    Vec2 userXAxisInPage {1.0f, 0.0f};

    /**
     * @brief Page direction representing the positive user-space Y axis.
     *
     * @note The projector orthonormalizes the user-space basis internally.
     */
    Vec2 userYAxisInPage {0.0f, 1.0f};
};

/**
 * @brief Projects client-side user-space positions and rotations into page space.
 *
 * @note This helper keeps the MFD runtime unchanged. The window still receives
 * regular logical coordinates in `[-1, 1]` and regular reticle rotations in
 * degrees. The helper simply performs the conversion on the client side.
 *
 * @note If an integration does not need any user-space conversion, it can skip
 * this helper entirely and keep sending normal page-space coordinates directly
 * in `[-1, 1]`.
 */
class MFD_API UserSpaceProjector
{
public:
    /**
     * @brief Creates a projector from one client-side frame definition.
     * @param frame User-space frame driving the projection.
     */
    explicit UserSpaceProjector(UserSpaceFrame frame = {});

    /**
     * @brief Returns the current user-space frame.
     * @return Read-only frame reference.
     */
    const UserSpaceFrame& Frame() const noexcept;

    /**
     * @brief Replaces the current user-space frame.
     * @param frame New frame used for subsequent projections.
     */
    void SetFrame(UserSpaceFrame frame) noexcept;

    /**
     * @brief Projects an offset already expressed relative to the user origin.
     * @param userOffset Offset expressed in user-space units.
     * @return Projected offset expressed in MFD page logical coordinates.
     */
    Vec2 ToPageOffset(Vec2 userOffset) const noexcept;

    /**
     * @brief Projects one absolute user-space position to the page.
     * @param userPosition Absolute user-space position.
     * @return Projected logical page position.
     */
    Vec2 ToPagePosition(Vec2 userPosition) const noexcept;

    /**
     * @brief Projects a user-space rotation to the page.
     * @param userRotationRadians Rotation expressed in user-space radians.
     * @return Equivalent page rotation in degrees.
     */
    float ToPageRotationDegrees(float userRotationRadians) const noexcept;

    /**
     * @brief Projects a full user-space pose to a page-space transform.
     * @param userPosition Absolute user-space position.
     * @param userRotationRadians Rotation expressed in user-space radians.
     * @param scale Optional scale copied to the resulting transform.
     * @return Transform ready to be consumed by page-space APIs.
     */
    Transform2D ToPageTransform(Vec2 userPosition,
                                float userRotationRadians,
                                Vec2 scale = {1.0f, 1.0f}) const noexcept;

private:
    UserSpaceFrame frame_ {};
};
} // namespace mfd
