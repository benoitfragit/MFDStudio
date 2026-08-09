/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Stateless HUD projection and EEGS funnel implementation.
 */

#include "hud/HudProjection.h"

#include "HudLayout.h"
#include "HudSpatialTransform.h"
#include "mfd/control/UserSpaceProjector.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hud
{
namespace
{
constexpr float kDegreesToRadians = 0.017453292519943295f;
constexpr float kRadiansToDegrees = 57.29577951308232f;
constexpr float kFeetToMeters = 0.3048f;
constexpr float kMetersToFeet = 3.280839895013123f;
constexpr float kMetersPerSecondToKnots = 1.94384449f;
constexpr float kNauticalMileToMeters = 1852.0f;
constexpr float kMetersToNauticalMiles = 1.0f / kNauticalMileToMeters;
constexpr float kMetersPerSecondToFeetPerMinute = 196.850394f;
constexpr float kSeaLevelMachMetersPerSecond = 340.294f;
constexpr float kHudUnitsPerMil = 0.0056f;
constexpr float kDefaultTargetWingspanMeters = 35.0f * kFeetToMeters;

// --- Single HUD angular projection model -------------------------------------
// One explicit field of view drives every conformal symbol (pitch ladder,
// target/radar cues, missile cues and A-G cues). Changing the HUD field of view
// is done here and nowhere else.
//
// Product decision: a 30-degree total vertical field of view for the pitch
// ladder reduces the visual saturation of the previous ~58-degree scale. At
// level flight only the +/-15 degree bars reach the aperture, so the +/-20,
// +/-25 and +/-30 bars are no longer drawn.
constexpr float kHudPitchLadderVerticalFovDeg = 30.0f;
constexpr float kHudConformalVerticalFovDeg = 30.0f;
constexpr float kHudConformalHorizontalFovDeg = 30.0f;
// The authored HUD page spans [-1, +1]; half the field of view maps to this
// half-extent, so a symbol at the field-of-view edge sits at the page edge.
constexpr float kHudConformalHalfWidthUnits = 1.0f;
constexpr float kHudConformalHalfHeightUnits = 1.0f;
// HUD units per degree. Horizontal and vertical are equal, giving conformal
// symbols the 1:1 angular scale required by the BMS attitude bars.
constexpr float kHudUnitsPerHorizontalDegree =
    kHudConformalHalfWidthUnits / (kHudConformalHorizontalFovDeg * 0.5f);
constexpr float kHudUnitsPerVerticalDegree =
    kHudConformalHalfHeightUnits / (kHudConformalVerticalFovDeg * 0.5f);
// The pitch ladder shares the vertical conformal scale over its 2.0-unit height.
constexpr float kPitchToHudUnits =
    (2.0f * kHudConformalHalfHeightUnits) / kHudPitchLadderVerticalFovDeg;

// Pitch beyond which the true horizon leaves the visible aperture and ghosts to
// the edge. Derived from the pitch ladder scale so it tracks the HUD field of
// view instead of being a magic HUD-unit constant.
constexpr float kGhostHorizonLimitDeg = 8.7f;
constexpr float kHudHorizonLimit = kGhostHorizonLimitDeg * kPitchToHudUnits;
constexpr float kDlzBottomY = -0.245f;
constexpr float kDlzHeight = 0.49f;
constexpr float kFunnelGeometryEpsilon = 0.000001f;
// The Level-II funnel must remain recognizable when a physically short or
// abruptly changing ballistic history is projected into the finite HUD page.
// These bounds affect only its display mechanization; physical endpoint widths
// still come from target wingspan and the 600/3000-foot slant ranges.
constexpr float kMinimumFunnelAxisLengthHudUnits = 0.24f;
constexpr float kMaximumFunnelAxisLengthHudUnits = 0.72f;
constexpr float kMaximumNearStationOffsetHudUnits = 0.16f;
constexpr float kMaximumFunnelBendFraction = 0.18f;
constexpr float kMinimumFunnelControlProgress = 0.20f;
constexpr float kMaximumFunnelControlProgress = 0.80f;
constexpr float kFunnelBallisticDirectionWeight = 0.10f;
constexpr float kMinimumFunnelWidthReductionFraction = 0.01f;
constexpr float kMaximumFunnelHalfWidthHudUnits = 0.40f;
constexpr float kFunnelApertureMarginHudUnits = 0.02f;
constexpr float kFpmHorizontalLimitHudUnits = 0.54f;
constexpr float kFpmVerticalLimitHudUnits = 0.52f;

float Clamp(const float value, const float low, const float high) noexcept
{
    return std::max(low, std::min(value, high));
}

float FiniteOr(const float value, const float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float WrapDegrees360(float value) noexcept
{
    value = std::fmod(value, 360.0f);
    if (value < 0.0f)
    {
        value += 360.0f;
    }

    return value;
}

float NormalizeDegrees180(float value) noexcept
{
    value = std::fmod(value + 180.0f, 360.0f);
    if (value < 0.0f)
    {
        value += 360.0f;
    }

    return value - 180.0f;
}

float HorizontalSpeedMetersPerSecond(const AircraftInputSample& aircraft) noexcept
{
    const float north = FiniteOr(aircraft.northSpeedMps, 0.0f);
    const float east = FiniteOr(aircraft.eastSpeedMps, 0.0f);
    return std::sqrt(north * north + east * east);
}

float TrueSpeedMetersPerSecond(const AircraftInputSample& aircraft) noexcept
{
    const float horizontal = HorizontalSpeedMetersPerSecond(aircraft);
    const float down = FiniteOr(aircraft.downSpeedMps, 0.0f);
    return std::sqrt(horizontal * horizontal + down * down);
}

float FlightPathSlopeRadians(const AircraftInputSample& aircraft) noexcept
{
    const float horizontal = HorizontalSpeedMetersPerSecond(aircraft);
    if (horizontal < 0.01f)
    {
        return 0.0f;
    }

    return std::atan2(-FiniteOr(aircraft.downSpeedMps, 0.0f), horizontal);
}

HudVec2 RotateHudVector(const HudVec2 offset, const float degrees) noexcept
{
    const float radians = degrees * kDegreesToRadians;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return HudVec2 {
        offset.x * cosine - offset.y * sine,
        offset.x * sine + offset.y * cosine};
}

struct ProjectedFunnelStations
{
    static constexpr std::size_t kCapacity = kGunTrajectoryPointCount;
    std::array<HudVec2, kCapacity> centers {};
    std::array<float, kCapacity> halfWidths {};
    std::size_t count = 0U;
};

struct QuadraticHudCurve
{
    HudVec2 start {};
    HudVec2 control {};
    HudVec2 end {};
};

struct FunnelRails
{
    HudFunnelControlPoints left {};
    HudFunnelControlPoints right {};
};

HudVec2 AddHudVectors(const HudVec2 first, const HudVec2 second) noexcept
{
    return HudVec2 {first.x + second.x, first.y + second.y};
}

HudVec2 SubtractHudVectors(const HudVec2 first, const HudVec2 second) noexcept
{
    return HudVec2 {first.x - second.x, first.y - second.y};
}

HudVec2 ScaleHudVector(const HudVec2 value, const float scale) noexcept
{
    return HudVec2 {value.x * scale, value.y * scale};
}

float DotHudVectors(const HudVec2 first, const HudVec2 second) noexcept
{
    return first.x * second.x + first.y * second.y;
}

float HudVectorLength(const HudVec2 value) noexcept
{
    return std::sqrt(DotHudVectors(value, value));
}

HudVec2 NormalizeHudVectorOr(const HudVec2 value, const HudVec2 fallback) noexcept
{
    const float length = HudVectorLength(value);
    if (length <= kFunnelGeometryEpsilon)
    {
        return fallback;
    }

    return ScaleHudVector(value, 1.0f / length);
}

HudVec2 LimitHudVectorLength(const HudVec2 value, const float maximumLength) noexcept
{
    const float length = HudVectorLength(value);
    if (length <= maximumLength || length <= kFunnelGeometryEpsilon)
    {
        return value;
    }

    return ScaleHudVector(value, maximumLength / length);
}

mfd::UserSpaceProjector BuildAngularProjector() noexcept
{
    mfd::UserSpaceFrame frame;
    frame.userOrigin = mfd::Vec2 {0.0f, 0.0f};
    frame.pageAnchor = mfd::Vec2 {0.0f, 0.0f};
    frame.originRotationRadians = 0.0f;
    frame.pageUnitsPerUserUnit = 1.0f;
    frame.userXAxisInPage = mfd::Vec2 {1.0f, 0.0f};
    frame.userYAxisInPage = mfd::Vec2 {0.0f, 1.0f};
    return mfd::UserSpaceProjector(frame);
}

HudVec2 ProjectAnglesUnclamped(const float azimuthRad, const float elevationRad) noexcept
{
    const float horizontalHalfFovRad = kHudConformalHorizontalFovDeg * 0.5f * kDegreesToRadians;
    const float verticalHalfFovRad = kHudConformalVerticalFovDeg * 0.5f * kDegreesToRadians;
    const mfd::Vec2 page = BuildAngularProjector().ToPagePosition(mfd::Vec2 {
        FiniteOr(azimuthRad, 0.0f) / horizontalHalfFovRad * kHudConformalHalfWidthUnits,
        FiniteOr(elevationRad, 0.0f) / verticalHalfFovRad * kHudConformalHalfHeightUnits});
    return HudVec2 {page.x, page.y};
}

bool ProjectTrajectoryCenter(const GunTrajectoryPointNed& point,
                             const AircraftInputSample& aircraft,
                             HudVec2& center,
                             float& slantRangeMeters) noexcept
{
    if (!point.valid || !std::isfinite(point.northMeters) || !std::isfinite(point.eastMeters) ||
        !std::isfinite(point.downMeters))
    {
        return false;
    }
    const float north = point.northMeters;
    const float east = point.eastMeters;
    const float down = point.downMeters;
    const detail::BodyAngularDirection direction = detail::ResolveBodyAngularDirection(
        aircraft,
        detail::NedVector {north, east, down},
        0.001f);
    if (!direction.valid)
    {
        return false;
    }
    slantRangeMeters = std::sqrt(north * north + east * east + down * down);
    if (!std::isfinite(slantRangeMeters))
    {
        return false;
    }
    center = ProjectAnglesUnclamped(direction.azimuthRad, direction.elevationRad);
    return std::isfinite(center.x) && std::isfinite(center.y);
}

ProjectedFunnelStations BuildProjectedFunnelStations(const HudInputSample& input,
                                                      const float wingspanMeters) noexcept
{
    ProjectedFunnelStations stations;
    for (const GunTrajectoryPointNed& point : input.gunTrajectory.points)
    {
        HudVec2 center;
        float range = 0.0f;
        if (!ProjectTrajectoryCenter(point, input.aircraft, center, range))
        {
            continue;
        }

        const float halfAngle = std::atan2(wingspanMeters * 0.5f, std::max(range, 1.0f));
        const float projectedHalfWidth = halfAngle /
            (kHudConformalHorizontalFovDeg * 0.5f * kDegreesToRadians) *
            kHudConformalHalfWidthUnits;
        stations.centers[stations.count] = center;
        stations.halfWidths[stations.count] = Clamp(
            projectedHalfWidth,
            kFunnelGeometryEpsilon,
            kMaximumFunnelHalfWidthHudUnits);
        ++stations.count;
    }

    return stations;
}

QuadraticHudCurve FitQuadraticCenterline(const ProjectedFunnelStations& stations) noexcept
{
    QuadraticHudCurve curve;
    curve.start = stations.centers[0];
    curve.end = stations.centers[stations.count - 1U];
    curve.control = ScaleHudVector(AddHudVectors(curve.start, curve.end), 0.5f);
    if (stations.count <= 2U)
    {
        return curve;
    }

    HudVec2 weightedResidual {};
    float squaredBasisSum = 0.0f;
    for (std::size_t index = 1U; index + 1U < stations.count; ++index)
    {
        const float t = static_cast<float>(index) / static_cast<float>(stations.count - 1U);
        const float oneMinusT = 1.0f - t;
        const float controlBasis = 2.0f * oneMinusT * t;
        const HudVec2 endpointContribution = AddHudVectors(
            ScaleHudVector(curve.start, oneMinusT * oneMinusT),
            ScaleHudVector(curve.end, t * t));
        const HudVec2 residual = SubtractHudVectors(
            stations.centers[index],
            endpointContribution);
        weightedResidual = AddHudVectors(
            weightedResidual,
            ScaleHudVector(residual, controlBasis));
        squaredBasisSum += controlBasis * controlBasis;
    }

    if (squaredBasisSum > kFunnelGeometryEpsilon)
    {
        curve.control = ScaleHudVector(weightedResidual, 1.0f / squaredBasisSum);
    }

    return curve;
}

HudVec2 GravityLeadDirection(const AircraftInputSample& aircraft) noexcept
{
    const float roll = FiniteOr(aircraft.rollRad, 0.0f);
    return HudVec2 {std::sin(roll), -std::cos(roll)};
}

QuadraticHudCurve ConstrainFunnelCenterline(const QuadraticHudCurve& fitted,
                                            const AircraftInputSample& aircraft) noexcept
{
    QuadraticHudCurve constrained;
    const HudVec2 gravityDirection = GravityLeadDirection(aircraft);
    const HudVec2 rawAxis = SubtractHudVectors(fitted.end, fitted.start);
    const float rawAxisLength = HudVectorLength(rawAxis);
    const HudVec2 rawAxisDirection = NormalizeHudVectorOr(rawAxis, gravityDirection);
    const float ballisticConfidence = Clamp(
        rawAxisLength / kMinimumFunnelAxisLengthHudUnits,
        0.0f,
        1.0f);
    const HudVec2 axisDirection = NormalizeHudVectorOr(
        AddHudVectors(
            gravityDirection,
            ScaleHudVector(
                rawAxisDirection,
                kFunnelBallisticDirectionWeight * ballisticConfidence)),
        gravityDirection);

    constrained.start = LimitHudVectorLength(
        fitted.start,
        kMaximumNearStationOffsetHudUnits);
    const float constrainedAxisLength = Clamp(
        rawAxisLength,
        kMinimumFunnelAxisLengthHudUnits,
        kMaximumFunnelAxisLengthHudUnits);
    constrained.end = AddHudVectors(
        constrained.start,
        ScaleHudVector(axisDirection, constrainedAxisLength));

    const HudVec2 rightNormal {-axisDirection.y, axisDirection.x};
    float controlProgress = 0.5f;
    float scaledBend = 0.0f;
    if (rawAxisLength > kFunnelGeometryEpsilon)
    {
        const HudVec2 rawRightNormal {-rawAxisDirection.y, rawAxisDirection.x};
        const HudVec2 rawControlOffset = SubtractHudVectors(fitted.control, fitted.start);
        const float fittedControlProgress = Clamp(
            DotHudVectors(rawControlOffset, rawAxisDirection) / rawAxisLength,
            kMinimumFunnelControlProgress,
            kMaximumFunnelControlProgress);
        controlProgress = 0.5f +
            (fittedControlProgress - 0.5f) * ballisticConfidence;
        scaledBend = DotHudVectors(rawControlOffset, rawRightNormal) *
            (constrainedAxisLength /
             std::max(rawAxisLength, kMinimumFunnelAxisLengthHudUnits)) *
            ballisticConfidence;
    }

    const float maximumBend = constrainedAxisLength * kMaximumFunnelBendFraction;
    scaledBend = Clamp(scaledBend, -maximumBend, maximumBend);
    constrained.control = AddHudVectors(
        AddHudVectors(
            constrained.start,
            ScaleHudVector(axisDirection, constrainedAxisLength * controlProgress)),
        ScaleHudVector(rightNormal, scaledBend));
    return constrained;
}

HudFunnelControlPoints ElevateQuadraticToQuartic(const QuadraticHudCurve& curve) noexcept
{
    HudFunnelControlPoints controls {};
    controls[0] = curve.start;
    controls[1] = ScaleHudVector(AddHudVectors(curve.start, curve.control), 0.5f);
    controls[2] = AddHudVectors(
        AddHudVectors(
            ScaleHudVector(curve.start, 1.0f / 6.0f),
            ScaleHudVector(curve.control, 2.0f / 3.0f)),
        ScaleHudVector(curve.end, 1.0f / 6.0f));
    controls[3] = ScaleHudVector(AddHudVectors(curve.control, curve.end), 0.5f);
    controls[4] = curve.end;
    return controls;
}

float NearestApertureTranslation(const float minimumValue,
                                 const float maximumValue,
                                 const float minimumAllowed,
                                 const float maximumAllowed) noexcept
{
    const float minimumTranslation = minimumAllowed - minimumValue;
    const float maximumTranslation = maximumAllowed - maximumValue;
    if (minimumTranslation > maximumTranslation)
    {
        return (minimumTranslation + maximumTranslation) * 0.5f;
    }

    return Clamp(0.0f, minimumTranslation, maximumTranslation);
}

HudVec2 FunnelApertureTranslation(const FunnelRails& rails,
                                  const HudVec2 anchor) noexcept
{
    float minimumX = rails.left[0].x;
    float maximumX = rails.left[0].x;
    float minimumY = rails.left[0].y;
    float maximumY = rails.left[0].y;
    for (std::size_t index = 0U; index < rails.left.size(); ++index)
    {
        minimumX = std::min(minimumX, std::min(rails.left[index].x, rails.right[index].x));
        maximumX = std::max(maximumX, std::max(rails.left[index].x, rails.right[index].x));
        minimumY = std::min(minimumY, std::min(rails.left[index].y, rails.right[index].y));
        maximumY = std::max(maximumY, std::max(rails.left[index].y, rails.right[index].y));
    }

    const float horizontalLimit =
        kHudConformalHalfWidthUnits - kFunnelApertureMarginHudUnits;
    const float verticalLimit =
        kHudConformalHalfHeightUnits - kFunnelApertureMarginHudUnits;
    return HudVec2 {
        NearestApertureTranslation(
            minimumX,
            maximumX,
            -horizontalLimit - anchor.x,
            horizontalLimit - anchor.x),
        NearestApertureTranslation(
            minimumY,
            maximumY,
            -verticalLimit - anchor.y,
            verticalLimit - anchor.y)};
}

void TranslateFunnelRails(FunnelRails& rails, const HudVec2 translation) noexcept
{
    for (std::size_t index = 0U; index < rails.left.size(); ++index)
    {
        rails.left[index] = AddHudVectors(rails.left[index], translation);
        rails.right[index] = AddHudVectors(rails.right[index], translation);
    }
}

FunnelRails BuildConstrainedFunnelRails(const ProjectedFunnelStations& stations,
                                        const AircraftInputSample& aircraft) noexcept
{
    FunnelRails rails;
    const QuadraticHudCurve fittedCenterline = FitQuadraticCenterline(stations);
    const QuadraticHudCurve centerline = ConstrainFunnelCenterline(
        fittedCenterline,
        aircraft);
    const HudFunnelControlPoints centerControls = ElevateQuadraticToQuartic(centerline);
    const HudVec2 axisDirection = NormalizeHudVectorOr(
        SubtractHudVectors(centerline.end, centerline.start),
        GravityLeadDirection(aircraft));
    const HudVec2 rightNormal {-axisDirection.y, axisDirection.x};
    const float nearHalfWidth = std::max(
        stations.halfWidths[0],
        kFunnelGeometryEpsilon * 2.0f);
    const float minimumWidthReduction = std::max(
        kFunnelGeometryEpsilon,
        nearHalfWidth * kMinimumFunnelWidthReductionFraction);
    const float maximumFarHalfWidth = std::max(
        kFunnelGeometryEpsilon,
        nearHalfWidth - minimumWidthReduction);
    const float farHalfWidth = Clamp(
        stations.halfWidths[stations.count - 1U],
        kFunnelGeometryEpsilon,
        maximumFarHalfWidth);

    for (std::size_t index = 0U; index < centerControls.size(); ++index)
    {
        const float t = static_cast<float>(index) /
            static_cast<float>(centerControls.size() - 1U);
        const float halfWidth = nearHalfWidth + (farHalfWidth - nearHalfWidth) * t;
        const HudVec2 halfWidthOffset = ScaleHudVector(rightNormal, halfWidth);
        rails.left[index] = SubtractHudVectors(centerControls[index], halfWidthOffset);
        rails.right[index] = AddHudVectors(centerControls[index], halfWidthOffset);
    }
    // The reticle is translated to the high-mounted Gun Bore Cross later. Keep
    // the complete convex hull inside the rendered aperture now; this prevents
    // the otherwise valid inverted funnel from disappearing beyond the top edge.
    const HudVec2 apertureTranslation = FunnelApertureTranslation(
        rails,
        detail::GunBoreCrossHudPosition());
    TranslateFunnelRails(rails, apertureTranslation);
    return rails;
}

// Weapon-specific facts (launch zone, time of flight, labels, quantities and
// per-weapon symbol scale) are caller-resolved inputs of `WeaponInputSample`.
// The projection only sanitizes them before use, so the HUD runtime never
// embeds airframe- or client-specific armament models.
float SanitizedMissileDiamondScale(const float scale) noexcept
{
    const float finiteScale = FiniteOr(scale, 1.0f);
    return finiteScale > 0.0f ? finiteScale : 1.0f;
}

float SanitizedNonNegative(const float value) noexcept
{
    return std::max(FiniteOr(value, 0.0f), 0.0f);
}

LaunchZone SanitizedLaunchZone(const LaunchZone& launchZone) noexcept
{
    LaunchZone zone = launchZone;
    zone.rmax1Nm = SanitizedNonNegative(zone.rmax1Nm);
    zone.rmax2Nm = SanitizedNonNegative(zone.rmax2Nm);
    zone.rmin2Nm = SanitizedNonNegative(zone.rmin2Nm);
    zone.rmin1Nm = SanitizedNonNegative(zone.rmin1Nm);
    return zone;
}

HudAttitudeFrame ResolveHudAttitude(const AircraftInputSample& aircraftInput) noexcept
{
    const AircraftState aircraft = BuildAircraftStateForHud(aircraftInput);
    HudAttitudeFrame frame;

    float displayPitch = NormalizeDegrees180(aircraft.pitchDegrees);
    float displayRoll = NormalizeDegrees180(aircraft.rollDegrees);
    if (displayPitch > 90.0f)
    {
        displayPitch = 180.0f - displayPitch;
        displayRoll = NormalizeDegrees180(displayRoll + 180.0f);
        frame.inverted = true;
    }
    else if (displayPitch < -90.0f)
    {
        displayPitch = -180.0f - displayPitch;
        displayRoll = NormalizeDegrees180(displayRoll + 180.0f);
        frame.inverted = true;
    }

    const float horizonRawY = -displayPitch * kPitchToHudUnits;
    const float horizonY = Clamp(horizonRawY, -kHudHorizonLimit, kHudHorizonLimit);
    const float rollRotationDegrees = -displayRoll;
    frame.displayPitchDegrees = displayPitch;
    frame.displayRollDegrees = displayRoll;
    frame.ladderPosition = RotateHudVector(HudVec2 {0.0f, horizonRawY}, rollRotationDegrees);
    frame.ladderRotationDegrees = rollRotationDegrees;
    frame.trueHorizonPosition = RotateHudVector(HudVec2 {0.0f, horizonY}, rollRotationDegrees);
    frame.trueHorizonRotationDegrees = rollRotationDegrees;
    frame.trueHorizonVisible = std::fabs(horizonRawY) <= kHudHorizonLimit;
    frame.ghostHorizonVisible = !frame.trueHorizonVisible;
    const float ghostHorizonY = horizonRawY < 0.0f ? -kHudHorizonLimit : kHudHorizonLimit;
    frame.ghostHorizonPosition = RotateHudVector(HudVec2 {0.0f, ghostHorizonY}, rollRotationDegrees);
    frame.ghostHorizonRotationDegrees = rollRotationDegrees;

    const detail::BodyAngularDirection flightPathDirection =
        detail::ResolveAircraftVelocityDirection(aircraftInput);
    if (flightPathDirection.valid)
    {
        const HudVec2 fpmRawPosition = ProjectAnglesUnclamped(
            flightPathDirection.azimuthRad,
            flightPathDirection.elevationRad);
        frame.fpmPosition = HudVec2 {
            Clamp(
                fpmRawPosition.x,
                -kFpmHorizontalLimitHudUnits,
                kFpmHorizontalLimitHudUnits),
            Clamp(
                fpmRawPosition.y,
                -kFpmVerticalLimitHudUnits,
                kFpmVerticalLimitHudUnits)};
        frame.fpmLimited =
            std::fabs(fpmRawPosition.x) > kFpmHorizontalLimitHudUnits ||
            std::fabs(fpmRawPosition.y) > kFpmVerticalLimitHudUnits;
    }
    frame.bankAngleIndicatorPosition = frame.fpmPosition;
    frame.bankAngleIndicatorRotationDegrees = -displayRoll;
    frame.zenithVisible = displayPitch > 68.0f;
    frame.zenithPosition = RotateHudVector(HudVec2 {0.0f, 0.48f}, rollRotationDegrees);
    frame.nadirVisible = displayPitch < -68.0f;
    frame.nadirPosition = RotateHudVector(HudVec2 {0.0f, -0.48f}, rollRotationDegrees);
    frame.levelRecoveryVisible =
        frame.inverted ||
        (aircraft.radarAltitudeFeet < 1500.0f && aircraft.verticalSpeedFpm < -2500.0f);

    return frame;
}

bool IsRadarAltitudeVisible(const AircraftState& aircraft, const HudAttitudeFrame& attitude) noexcept
{
    if (aircraft.radarAltitudeFeet <= 0.0f)
    {
        return false;
    }

    const float absPitch = std::fabs(attitude.displayPitchDegrees);
    const float absRoll = std::fabs(attitude.displayRollDegrees);
    if (aircraft.radarAltitudeFeet < 5000.0f)
    {
        return absPitch <= 30.0f && absRoll <= 90.0f;
    }

    return absPitch <= 10.0f && absRoll <= 75.0f;
}

HudAirDataFrame BuildAirDataFrame(const AircraftInputSample& aircraftInput, const HudAttitudeFrame& attitude) noexcept
{
    const AircraftState aircraft = BuildAircraftStateForHud(aircraftInput);
    HudAirDataFrame frame;
    frame.radarAltitudeVisible = IsRadarAltitudeVisible(aircraft, attitude);
    frame.verticalVelocityCueY = Clamp(aircraft.verticalSpeedFpm / 6000.0f * 0.40f, -0.40f, 0.40f);

    const float energyCueOffset = Clamp(std::fabs(aircraft.energyRate) * 0.028f, 0.075f, 0.155f);
    frame.energyChevronUpVisible = aircraft.energyRate > 0.45f;
    frame.energyChevronDownVisible = aircraft.energyRate < -0.45f;
    frame.energyChevronUpPosition =
        HudVec2 {attitude.fpmPosition.x, Clamp(attitude.fpmPosition.y + energyCueOffset, -0.50f, 0.56f)};
    frame.energyChevronDownPosition =
        HudVec2 {attitude.fpmPosition.x, Clamp(attitude.fpmPosition.y - energyCueOffset, -0.56f, 0.50f)};
    return frame;
}

// Single conformal projector: every boresight-relative symbol goes through this
// helper, so the HUD keeps one angular scale and one field-of-view rule.
ProjectedHudPoint ProjectConformalPoint(const float azimuthRad, const float elevationRad) noexcept
{
    const float safeAzimuthRad = FiniteOr(azimuthRad, 0.0f);
    const float safeElevationRad = FiniteOr(elevationRad, 0.0f);
    const float azimuthDeg = safeAzimuthRad * kRadiansToDegrees;
    const float elevationDeg = safeElevationRad * kRadiansToDegrees;
    const HudVec2 raw = ProjectAnglesUnclamped(safeAzimuthRad, safeElevationRad);
    const float rawX = raw.x;
    const float rawY = raw.y;
    const float x = Clamp(rawX, -kHudConformalHalfWidthUnits, kHudConformalHalfWidthUnits);
    const float y = Clamp(rawY, -kHudConformalHalfHeightUnits, kHudConformalHalfHeightUnits);

    ProjectedHudPoint point;
    point.position = HudVec2 {x, y};
    point.insideFov =
        std::fabs(azimuthDeg) <= kHudConformalHorizontalFovDeg * 0.5f &&
        std::fabs(elevationDeg) <= kHudConformalVerticalFovDeg * 0.5f;
    point.limited = x != rawX || y != rawY;
    return point;
}

ProjectedHudPoint ProjectTargetToHud(const TargetInputSample& target) noexcept
{
    return ProjectConformalPoint(target.azimuthRad, target.elevationRad);
}

bool IsWeaponArmedForHud(const WeaponInputSample& weapon) noexcept
{
    return weapon.masterArm || weapon.simulateMode;
}

bool IsAirToAirMissileMode(const WeaponInputSample& weapon) noexcept
{
    if (weapon.masterMode != HudMasterMode::AirToAir)
    {
        return false;
    }

    return weapon.weaponMode == HudWeaponMode::AirToAirMissile || weapon.weaponMode == HudWeaponMode::None;
}

HudWeaponFrame BuildWeaponFrame(const HudInputSample& input) noexcept
{
    HudWeaponFrame frame;
    frame.airToAirVisible = IsAirToAirMissileMode(input.weapon) && IsWeaponArmedForHud(input.weapon);
    // A target outside the HUD field of view is clamped to the edge and flagged
    // as limited, not hidden: BMS keeps the AIM-120 diamond at the FOV edge and
    // overlays a geometric limit-X on it.
    const ProjectedHudPoint targetPoint = ProjectTargetToHud(input.target);
    frame.targetVisible = frame.airToAirVisible && input.target.valid;
    frame.targetPosition = targetPoint.position;
    frame.targetLimited = frame.targetVisible && !targetPoint.insideFov;
    frame.missileDiamondLimited = frame.targetLimited;
    frame.missileLimitXVisible = frame.targetLimited;
    frame.missileLimitXPosition = frame.targetPosition;
    frame.missileDiamondPosition = frame.targetPosition;
    frame.missileDiamondScale = SanitizedMissileDiamondScale(input.weapon.missileDiamondScale);
    frame.attackSteeringCueVisible = frame.targetVisible;
    frame.attackSteeringCuePosition = HudVec2 {
        Clamp(frame.targetPosition.x * 0.55f, -0.30f, 0.30f),
        Clamp(frame.targetPosition.y * 0.45f - 0.04f, -0.26f, 0.26f)};
    // The launch zone and time of flight are avionics facts computed by the
    // caller; the projection only positions the range cue on the DLZ scale.
    frame.launchZone = SanitizedLaunchZone(input.weapon.launchZone);
    frame.dynamicLaunchZoneVisible = frame.targetVisible;
    frame.rangeCueVisible = frame.targetVisible;
    const TargetState target = BuildTargetStateForHud(input.target);
    const float rangeFactor = Clamp(target.rangeNm / std::max(frame.launchZone.rmax1Nm, 0.1f), 0.0f, 1.0f);
    frame.rangeCuePosition = HudVec2 {0.0f, kDlzBottomY + rangeFactor * kDlzHeight};
    frame.breakXVisible = frame.airToAirVisible && input.target.valid && frame.launchZone.tooClose;
    frame.missileCircleVisible = frame.airToAirVisible;
    frame.selectedMissileTimeOfFlightSeconds =
        SanitizedNonNegative(input.weapon.selectedMissileTimeOfFlightSeconds);

    frame.missileInFlight = input.weapon.missileInFlight;
    frame.activeMissileTimeRemainingSeconds = input.weapon.activeMissileTimeRemainingSeconds;
    frame.activeMissilePhase = input.weapon.activeMissilePhase;

    if (input.weapon.selectedWeaponQuantity <= 0)
    {
        frame.rangeCueVisible = false;
    }

    return frame;
}

HudGunFrame BuildGunFrame(const HudInputSample& input) noexcept
{
    HudGunFrame frame;
    const AircraftState aircraft = BuildAircraftStateForHud(input.aircraft);
    const bool armed = IsWeaponArmedForHud(input.weapon);
    const bool airToAirGun =
        input.weapon.masterMode == HudMasterMode::AirToAir &&
        input.weapon.weaponMode == HudWeaponMode::AirToAirGun &&
        input.weapon.gunMode == HudGunMode::Eegs &&
        armed &&
        input.weapon.gunRoundsRemaining > 0;
    const bool strafeGun =
        input.weapon.masterMode == HudMasterMode::AirToGround &&
        input.weapon.weaponMode == HudWeaponMode::AirToGroundStrafe &&
        input.weapon.gunMode == HudGunMode::Strafe &&
        armed &&
        input.weapon.gunRoundsRemaining > 0 &&
        input.airGround.valid;

    const ProjectedHudPoint targetPoint = ProjectTargetToHud(input.target);
    const HudVec2 targetPosition = targetPoint.position;
    frame.airToAirGunVisible = airToAirGun;
    frame.eegsFunnelVisible = airToAirGun && !input.weapon.targetLocked;
    frame.mrgsVisible = frame.eegsFunnelVisible;
    frame.fedsVisible = frame.eegsFunnelVisible && input.weapon.triggerHeld;
    // The locked target designator circle is conformal: when the track leaves the
    // field of view it is clamped to the edge and flagged, not hidden.
    frame.tdCircleVisible = airToAirGun && input.weapon.targetLocked && input.target.valid;
    frame.tdCirclePosition = targetPosition;
    frame.tdCircleLimited = frame.tdCircleVisible && !targetPoint.insideFov;
    frame.tdCircleLimitXVisible = frame.tdCircleLimited;
    frame.tdCircleLimitXPosition = targetPosition;
    frame.targetRangeFeet = std::max(FiniteOr(input.target.rangeMeters, 0.0f), 0.0f) * kMetersToFeet;

    // A neutral or non-finite wingspan input falls back to the authored HUD
    // default; the fallback is a projection concern, not part of the contract.
    const float providedWingspanMeters = FiniteOr(input.weapon.targetWingspanMeters, 0.0f);
    const float targetWingspanMeters =
        providedWingspanMeters > 0.0f ? providedWingspanMeters : kDefaultTargetWingspanMeters;
    const ProjectedFunnelStations funnelStations =
        BuildProjectedFunnelStations(input, targetWingspanMeters);
    frame.eegsFunnelVisible = frame.eegsFunnelVisible && funnelStations.count >= 2U;
    if (frame.eegsFunnelVisible)
    {
        // Both walls derive from one constrained quadratic centerline and one
        // transverse axis. Degree elevation preserves that centerline exactly
        // while matching the authored five-control-point primitive. A linearly
        // decreasing physical half-width makes crossings and twists impossible
        // for every rendered Bezier parameter.
        const FunnelRails rails = BuildConstrainedFunnelRails(
            funnelStations,
            input.aircraft);
        frame.eegsFunnelLeftControlPoints = rails.left;
        frame.eegsFunnelRightControlPoints = rails.right;
    }
    frame.eegsFunnelScaleX = 1.0f;
    frame.eegsFunnelScaleY = 1.0f;
    // Control points are angular offsets from the gun line. Translating the
    // reticle to the authored gun bore cross establishes that reference without
    // introducing any dependency on the separately computed flight-path marker.
    frame.eegsFunnelPosition = detail::GunBoreCrossHudPosition();
    frame.eegsFunnelRotationDegrees = 0.0f;

    frame.mrgsPosition = HudVec2 {
        0.0f,
        0.065f};
    frame.mrgsRotationDegrees = frame.eegsFunnelRotationDegrees;

    if (frame.tdCircleVisible)
    {
        const float settlingOffset = 0.018f;
        frame.oneGPipperVisible = true;
        frame.oneGPipperPosition = HudVec2 {
            Clamp(targetPosition.x * 0.82f, -0.48f, 0.48f),
            Clamp(targetPosition.y - settlingOffset, -0.46f, 0.46f)};
        frame.maxGPipperVisible = true;
        frame.maxGPipperPosition = HudVec2 {
            Clamp(targetPosition.x * 0.68f, -0.48f, 0.48f),
            Clamp(targetPosition.y - 0.12f, -0.48f, 0.42f)};
        frame.outOfPlaneBarsVisible = true;
        frame.solutionCircleVisible = true;
        frame.solutionCirclePosition = HudVec2 {
            Clamp(targetPosition.x * 0.76f, -0.48f, 0.48f),
            Clamp(targetPosition.y - 0.070f, -0.48f, 0.44f)};
        frame.batrVisible = input.weapon.triggerHeld;
        frame.batrPosition = frame.solutionCirclePosition;
    }

    const ProjectedHudPoint strafePipper = ProjectConformalPoint(
        input.airGround.pipperAzimuthRad, -FiniteOr(input.airGround.pipperDepressionRad, 0.0f));
    // The strafe pipper is conformal; when the aim point leaves the HUD field of
    // view it is clamped to the edge and flagged, not hidden. The current model
    // has no dedicated strafe limit-X reticle, so only the flag is exposed.
    frame.strafeVisible = strafeGun;
    frame.strafeSlantRangeFeet = std::max(FiniteOr(input.airGround.slantRangeMeters, 0.0f), 0.0f) * kMetersToFeet;
    frame.strafePipperPosition = strafePipper.position;
    frame.strafePipperLimited = frame.strafeVisible && !strafePipper.insideFov;
    // The in-range threshold is a caller-resolved fact; the runtime never
    // invents an ammunition default. A non-positive value hides the cue.
    const float strafeInRangeFeet = FiniteOr(input.weapon.strafeInRangeFeet, 0.0f);
    frame.strafeInRangeCueVisible =
        frame.strafeVisible && strafeInRangeFeet > 0.0f && frame.strafeSlantRangeFeet <= strafeInRangeFeet;
    frame.bulletTrackEndPosition = HudVec2 {
        Clamp(frame.strafePipperPosition.x * 0.72f, -0.42f, 0.42f),
        Clamp(frame.strafePipperPosition.y + 0.22f, -0.42f, 0.52f)};
    return frame;
}

HudAirGroundFrame BuildAirGroundFrame(const HudInputSample& input) noexcept
{
    HudAirGroundFrame frame;
    const bool ccipMode =
        input.weapon.masterMode == HudMasterMode::AirToGround &&
        input.weapon.weaponMode == HudWeaponMode::AirToGroundCcip &&
        IsWeaponArmedForHud(input.weapon) &&
        input.airGround.valid;
    // The CCIP pipper is conformal; when it reaches the total field-of-view edge
    // it is clamped there and a limit-X is overlaid, matching BMS, instead of the
    // pipper disappearing.
    const ProjectedHudPoint ccipPipper = ProjectConformalPoint(
        input.airGround.pipperAzimuthRad, -FiniteOr(input.airGround.pipperDepressionRad, 0.0f));
    frame.ccipVisible = ccipMode;
    frame.ccipPipperPosition = ccipPipper.position;
    frame.ccipPipperLimited = frame.ccipVisible && !ccipPipper.insideFov;
    frame.ccipLimitXVisible = frame.ccipPipperLimited;
    frame.ccipLimitXPosition = ccipPipper.position;
    frame.bombFallLineX = ProjectConformalPoint(input.airGround.fallLineAzimuthRad, 0.0f).position.x;
    const ProjectedHudPoint solutionCue = ProjectConformalPoint(
        input.airGround.fallLineAzimuthRad, -FiniteOr(input.airGround.solutionCueDepressionRad, 0.0f));
    frame.solutionCueVisible = frame.ccipVisible && input.airGround.solutionCueValid;
    frame.solutionCuePosition = solutionCue.position;
    frame.solutionCueLimited = frame.solutionCueVisible && !solutionCue.insideFov;
    const ProjectedHudPoint pullupCue = ProjectConformalPoint(
        input.airGround.fallLineAzimuthRad, -FiniteOr(input.airGround.pullupAnticipationCueDepressionRad, 0.0f));
    frame.pullupAnticipationCueVisible = frame.ccipVisible && input.airGround.pullupAnticipationCueValid;
    frame.pullupAnticipationCuePosition = pullupCue.position;
    frame.pullupAnticipationCueLimited = frame.pullupAnticipationCueVisible && !pullupCue.insideFov;
    frame.slantRangeFeet = std::max(FiniteOr(input.airGround.slantRangeMeters, 0.0f), 0.0f) * kMetersToFeet;
    frame.timeToReleaseSeconds = std::max(FiniteOr(input.airGround.timeToReleaseSeconds, 0.0f), 0.0f);
    frame.timeToGoSeconds = std::max(FiniteOr(input.airGround.timeToGoSeconds, 0.0f), 0.0f);
    return frame;
}

HudApproachFrame BuildApproachFrame(const HudInputSample& input, const HudAttitudeFrame& attitude) noexcept
{
    HudApproachFrame frame;
    frame.landingVisible =
        input.approach.landingModeActive ||
        input.approach.landingGearDown ||
        input.weapon.masterMode == HudMasterMode::Landing;
    frame.forceCalibratedAirspeed = input.approach.landingGearDown;
    frame.fineAltitudeScale = frame.landingVisible;
    frame.minusTwoPointFivePitchLineVisible = frame.landingVisible;
    const float minusTwoPointFiveOffsetY =
        (-2.5f - attitude.displayPitchDegrees) * kPitchToHudUnits;
    frame.minusTwoPointFivePitchLinePosition =
        RotateHudVector(HudVec2 {0.0f, Clamp(minusTwoPointFiveOffsetY, -0.52f, 0.52f)}, -attitude.displayRollDegrees);
    frame.declutterActive =
        frame.landingVisible &&
        input.approach.landingDeclutterActive &&
        !input.approach.weightOnWheels;
    frame.rollIndicatorVisible = !frame.declutterActive;
    frame.headingTapeShiftedUp = frame.landingVisible;
    return frame;
}

HudIlsFrame BuildIlsFrame(const HudInputSample& input, const HudApproachFrame& approach) noexcept
{
    HudIlsFrame frame;
    const bool ilsAvailable =
        input.ils.powered &&
        input.ils.selected &&
        input.ils.signalValid;
    frame.barsVisible = ilsAvailable && !approach.declutterActive;
    frame.localizerDeviationDots = Clamp(FiniteOr(input.ils.localizerDeviationDots, 0.0f), -2.5f, 2.5f);
    frame.glideslopeDeviationDots = Clamp(FiniteOr(input.ils.glideslopeDeviationDots, 0.0f), -2.5f, 2.5f);
    frame.localizerBarPosition = HudVec2 {frame.localizerDeviationDots * 0.075f, 0.0f};
    frame.glideslopeBarPosition = HudVec2 {0.0f, frame.glideslopeDeviationDots * 0.075f};
    frame.commandSteeringVisible =
        ilsAvailable &&
        input.ils.commandSteeringActive &&
        !approach.declutterActive &&
        input.approach.flightPathMarkerAvailable;
    frame.commandSteeringPosition = HudVec2 {
        Clamp(frame.localizerDeviationDots * 0.065f, -0.22f, 0.22f),
        Clamp(frame.glideslopeDeviationDots * 0.065f, -0.22f, 0.22f)};
    frame.wSteeringVisible =
        ilsAvailable &&
        !approach.declutterActive &&
        !input.approach.flightPathMarkerAvailable;
    const HudVec2 gunBoreCrossPosition = detail::GunBoreCrossHudPosition();
    frame.wSteeringPosition = HudVec2 {
        gunBoreCrossPosition.x,
        gunBoreCrossPosition.y - 11.0f * kHudUnitsPerMil};
    return frame;
}
} // namespace

HudAngularProjection HudConformalProjection() noexcept
{
    return HudAngularProjection {
        kHudConformalHorizontalFovDeg,
        kHudConformalVerticalFovDeg,
        kHudConformalHalfWidthUnits,
        kHudConformalHalfHeightUnits};
}

float HudPitchLadderVerticalFovDegrees() noexcept
{
    return kHudPitchLadderVerticalFovDeg;
}

ProjectedHudPoint ProjectBoresightAngularOffsetToHud(const float azimuthRad, const float elevationRad) noexcept
{
    return ProjectConformalPoint(azimuthRad, elevationRad);
}

HudVec2 ProjectPitchOffsetToHud(const float pitchOffsetDeg) noexcept
{
    return HudVec2 {0.0f, FiniteOr(pitchOffsetDeg, 0.0f) * kPitchToHudUnits};
}

bool IsInsideHudFov(const float azimuthRad, const float elevationRad) noexcept
{
    return ProjectConformalPoint(azimuthRad, elevationRad).insideFov;
}

AircraftState BuildAircraftStateForHud(const AircraftInputSample& aircraft) noexcept
{
    AircraftState state;
    const float trueSpeedMps = TrueSpeedMetersPerSecond(aircraft);
    state.elapsedSeconds = FiniteOr(aircraft.elapsedSeconds, 0.0f);
    state.pitchDegrees = NormalizeDegrees180(FiniteOr(aircraft.pitchRad, 0.0f) * kRadiansToDegrees);
    state.rollDegrees = NormalizeDegrees180(FiniteOr(aircraft.rollRad, 0.0f) * kRadiansToDegrees);
    state.headingDegrees = WrapDegrees360(FiniteOr(aircraft.headingRad, 0.0f) * kRadiansToDegrees);
    state.flightPathAngleDegrees = FlightPathSlopeRadians(aircraft) * kRadiansToDegrees;
    state.speedKts = std::max(trueSpeedMps, 0.0f) * kMetersPerSecondToKnots;
    const float inputMach = FiniteOr(aircraft.mach, 0.0f);
    state.mach = inputMach > 0.01f ? inputMach : trueSpeedMps / kSeaLevelMachMetersPerSecond;
    state.altitudeFeet = std::max(FiniteOr(aircraft.altitudeMeters, 0.0f), 0.0f) * kMetersToFeet;
    state.radarAltitudeFeet = std::max(FiniteOr(aircraft.radioAltitudeMeters, 0.0f), 0.0f) * kMetersToFeet;
    state.verticalSpeedFpm = -FiniteOr(aircraft.downSpeedMps, 0.0f) * kMetersPerSecondToFeetPerMinute;
    state.normalLoadG = FiniteOr(aircraft.normalLoadFactor, 1.0f);
    state.throttle = Clamp(FiniteOr(aircraft.throttleRatio, 0.0f), 0.0f, 1.0f);
    state.energyRate = FiniteOr(aircraft.specificEnergyRateMps, 0.0f) / 12.0f;
    state.afterburnerActive = aircraft.afterburnerActive;
    return state;
}

TargetState BuildTargetStateForHud(const TargetInputSample& target) noexcept
{
    TargetState state;
    state.valid = target.valid;
    state.rangeNm = std::max(FiniteOr(target.rangeMeters, 0.0f), 0.0f) * kMetersToNauticalMiles;
    state.closureKts = FiniteOr(target.closingSpeedMps, 0.0f) * kMetersPerSecondToKnots;
    state.aspectDegrees = WrapDegrees360(FiniteOr(target.aspectRad, 0.0f) * kRadiansToDegrees);
    state.azimuthDegrees = FiniteOr(target.azimuthRad, 0.0f) * kRadiansToDegrees;
    state.elevationDegrees = FiniteOr(target.elevationRad, 0.0f) * kRadiansToDegrees;
    state.altitudeFeet = std::max(FiniteOr(target.altitudeMeters, 0.0f), 0.0f) * kMetersToFeet;
    return state;
}

HudFrame BuildHudFrame(const HudInputSample& input) noexcept
{
    HudFrame frame;
    frame.attitude = ResolveHudAttitude(input.aircraft);
    frame.airData = BuildAirDataFrame(input.aircraft, frame.attitude);
    frame.weapon = BuildWeaponFrame(input);
    frame.gun = BuildGunFrame(input);
    frame.airGround = BuildAirGroundFrame(input);
    frame.approach = BuildApproachFrame(input, frame.attitude);
    frame.ils = BuildIlsFrame(input, frame.approach);
    return frame;
}

std::string FormatHeading(const float headingDegrees)
{
    char buffer[16] {};
    std::snprintf(buffer, sizeof(buffer), "%03d", static_cast<int>(std::lround(WrapDegrees360(headingDegrees))) % 360);
    return buffer;
}

const char* MissilePhaseMnemonic(const MissileFlightPhase phase) noexcept
{
    switch (phase)
    {
    case MissileFlightPhase::Boost:
        return "BST";
    case MissileFlightPhase::Midcourse:
        return "MID";
    case MissileFlightPhase::Active:
        return "ACT";
    case MissileFlightPhase::Terminal:
        return "TERM";
    case MissileFlightPhase::Impact:
        return "HIT";
    }

    return "UNK";
}
} // namespace hud
