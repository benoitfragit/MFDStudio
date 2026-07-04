/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Executable entry point.
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "mfd/client/ClientSdk.h"
#include "mfd/control/UserSpaceProjector.h"
#include "TutorialUi.h"

namespace
{
constexpr std::string_view kWindowFile = "assets/windows/mfd_tutorial.json";
constexpr std::size_t kMaxTransientTracks = 10U;
constexpr auto kTrackInterval = std::chrono::seconds(2);
constexpr auto kPageSwitchInterval = std::chrono::seconds(30);
constexpr auto kStrobeSwitchInterval = std::chrono::seconds(8);
constexpr float kProgressBarMaxWidth = 0.44f;
constexpr float kProgressBarHeight = 0.06f;
constexpr float kProgressBarLeftEdge = -0.22f;
constexpr float kProgressLoopDurationSeconds = 6.0f;
constexpr float kOwnshipAnchorX = 0.0f;
constexpr float kOwnshipAnchorY = -0.7f;
constexpr float kMinTrackRangeNm = 4.0f;
constexpr float kMaxTrackRangeNm = 40.0f;
// Page-space upper bound used for the farthest straight-ahead contact.
// With the ownship anchored at y = -0.7, keeping the forward limit at y = +0.9
// leaves a small margin to the page border.
constexpr float kForwardVisibleLimitY = 0.9f;
// Vertical page span available for the forward sector:
//   0.9 - (-0.7) = 1.6 page units.
constexpr float kForwardVisibleSpanPageUnits = kForwardVisibleLimitY - kOwnshipAnchorY;
// NM -> page conversion ratio chosen so that 40 NM occupies the full forward
// span above:
//   kPageUnitsPerNm = 1.6 / 40 = 0.04.
constexpr float kPageUnitsPerNm = kForwardVisibleSpanPageUnits / kMaxTrackRangeNm;
constexpr float kMaxTrackAzimuthRadians = 1.0f;
constexpr float kNominalTrackThickness = 0.0038f;
constexpr float kCapturedTrackThickness = 0.0056f;
constexpr float kTrackLinkThickness = 0.0034f;
constexpr float kMaxFrameDeltaSeconds = 0.1f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kStrobeRotationAmplitudeDegrees = 34.0f;
constexpr float kStrobeScaleAmplitude = 0.28f;
constexpr float kStrobeLabelManualScale = 1.18f;
constexpr float kStrobeManualLabelPhaseSeconds = 4.0f;

/**
 * @brief Simple ownship state expressed in domain units.
 *
 * Distances stay in nautical miles and the heading stays in radians. The
 * rendered page never sees these raw values directly. The client uses
 * `UserSpaceProjector` to convert them to regular page coordinates only when it
 * is about to send commands.
 */
struct OwnshipState
{
    mfd::Vec2 worldPositionNm {};
    float headingRadians = 0.0f;
};

/**
 * @brief Runtime state for one track kept in business coordinates.
 *
 * The track is authored and animated in aircraft-relative polar data:
 * - `distanceNm` is the range ahead of the aircraft in nautical miles
 * - `azimuthRadians` is the offset angle around the aircraft nose
 * - `relativeHeadingRadians` is the track heading relative to the aircraft
 *
 * Page-space coordinates are derived later through `UserSpaceProjector`.
 */
struct TutorialTrackState
{
    tutorial_ui::MfdTutorialRadarTrackDynamicReticle* reticle = nullptr;
    float distanceNm = 0.0f;
    float azimuthRadians = 0.0f;
    float relativeHeadingRadians = 0.0f;
    float distanceRateNmPerSecond = 0.0f;
    float azimuthRateRadiansPerSecond = 0.0f;
    float relativeHeadingRateRadiansPerSecond = 0.0f;
    std::uint32_t serial = 0U;
    bool visible = true;
    bool persistent = false;
};

/**
 * @brief Throws when one command-client action fails.
 * @param success Result returned by the client helper.
 * @param client Command client used for the operation.
 * @param action Human-readable action name.
 */
void Require(const bool success, const mfd::CommandClient& client, const std::string_view action)
{
    if (!success)
    {
        throw std::runtime_error(std::string(action) + ": " + client.LastError());
    }
}

/**
 * @brief Rotates one 2D vector by a radian angle.
 * @param value Vector to rotate.
 * @param radians Rotation angle in radians.
 * @return Rotated vector.
 */
mfd::Vec2 RotateRadians(const mfd::Vec2 value, const float radians) noexcept
{
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine};
}

/**
 * @brief Mirrors one scalar value on a bounded interval.
 * @param value Value updated in place.
 * @param rate Signed speed updated in place when a rebound occurs.
 * @param minimum Inclusive minimum bound.
 * @param maximum Inclusive maximum bound.
 */
void ReflectOnInterval(float& value, float& rate, const float minimum, const float maximum) noexcept
{
    if (value < minimum)
    {
        value = minimum + (minimum - value);
        rate = std::abs(rate);
    }
    else if (value > maximum)
    {
        value = maximum - (value - maximum);
        rate = -std::abs(rate);
    }
}

/**
 * @brief Wraps one angle back to the canonical `[-pi, pi]` interval.
 * @param radians Angle updated in place.
 */
void WrapRadians(float& radians) noexcept
{
    // Guard against non-finite input: a while-loop normalization would never converge on +/-inf
    // (inf - 2pi == inf) and could spin for a very long time on a huge finite value. std::remainder
    // performs the reduction to [-pi, pi] in O(1).
    if (!std::isfinite(radians))
    {
        return;
    }

    radians = std::remainder(radians, 2.0f * kPi);
}

/**
 * @brief Builds one aircraft-relative offset from polar business coordinates.
 *
 * Convention used by this file:
 * - local `+X` means "ahead of the aircraft"
 * - local `+Y` means "to the aircraft right"
 *
 * @param azimuthRadians Relative azimuth around the aircraft nose, in radians.
 * @param distanceNm Range from the aircraft, in nautical miles.
 * @return Local offset expressed in nautical miles.
 */
mfd::Vec2 BuildOwnshipRelativeOffsetNm(const float azimuthRadians, const float distanceNm) noexcept
{
    return {
        distanceNm * std::cos(azimuthRadians),
        distanceNm * std::sin(azimuthRadians)};
}

/**
 * @brief Rebuilds the absolute world position of one track from its business data.
 *
 * The track itself is stored in aircraft-relative distance/azimuth. This helper
 * converts that local polar state into one world-space point, still expressed
 * in nautical miles.
 *
 * Example:
 * - `distanceNm = 20` and `azimuthRadians = 0` means "20 NM straight ahead"
 * - `distanceNm = 20` and positive azimuth means "20 NM ahead and to the right"
 * - page-space coordinates are not involved at this stage
 *
 * @param track Track expressed in aircraft-relative business coordinates.
 * @param ownship Current ownship origin and heading.
 * @return Absolute world position in nautical miles.
 */
mfd::Vec2 BuildTrackWorldPositionNm(const TutorialTrackState& track, const OwnshipState& ownship) noexcept
{
    const mfd::Vec2 localOffsetNm = BuildOwnshipRelativeOffsetNm(track.azimuthRadians, track.distanceNm);
    return ownship.worldPositionNm + RotateRadians(localOffsetNm, ownship.headingRadians);
}

/**
 * @brief Rebuilds the absolute world heading of one track.
 * @param track Track heading stored relative to the aircraft.
 * @param ownship Current ownship heading.
 * @return Absolute world heading in radians.
 */
float BuildTrackWorldHeadingRadians(const TutorialTrackState& track, const OwnshipState& ownship) noexcept
{
    float headingRadians = ownship.headingRadians + track.relativeHeadingRadians;
    WrapRadians(headingRadians);
    return headingRadians;
}

/**
 * @brief Builds the client-side user-space projector for Page1.
 *
 * The page keeps using normal logical coordinates in `[-1, 1]`. The projector
 * below makes the ownship anchor stay fixed at `{0.0, -0.7}` and converts the
 * domain-space frame:
 * - nautical miles -> page units through `kPageUnitsPerNm`
 * - radians -> page-space degrees
 * - aircraft-forward axes -> page axes
 *
 * @param ownship Current ownship domain state.
 * @return Projector ready to convert domain-space positions and headings.
 */
mfd::UserSpaceProjector BuildTutorialProjector(const OwnshipState& ownship)
{
    mfd::UserSpaceFrame frame;

    // Track coordinates are defined relative to the aircraft origin in NM.
    frame.userOrigin = ownship.worldPositionNm;

    // The authored aircraft symbol in Page1 is placed at this page anchor.
    frame.pageAnchor = {kOwnshipAnchorX, kOwnshipAnchorY};

    // The local frame rotates with the aircraft heading.
    frame.originRotationRadians = ownship.headingRadians;

    // This ratio is derived from the available forward page span:
    //   kForwardVisibleSpanPageUnits = kForwardVisibleLimitY - kOwnshipAnchorY
    //                                = 0.9 - (-0.7)
    //                                = 1.6
    // and from the maximum forward range:
    //   kPageUnitsPerNm = 1.6 / 40 = 0.04.
    frame.pageUnitsPerUserUnit = kPageUnitsPerNm;

    // Local +X (aircraft forward) is rendered upward on the page.
    frame.userXAxisInPage = {0.0f, 1.0f};

    // Local +Y (aircraft right) is rendered toward page +X.
    frame.userYAxisInPage = {1.0f, 0.0f};
    return mfd::UserSpaceProjector(frame);
}

/**
 * @brief Computes the diamond size used to display one track.
 * @param track Track whose range drives the symbol size.
 * @return Logical page-space size.
 */
float ComputeTrackSymbolSize(const TutorialTrackState& track) noexcept
{
    const float baseSize = 0.22f - track.distanceNm * 0.0020f;
    const float serialBias = 0.01f * static_cast<float>(track.serial % 3U);
    return std::clamp(baseSize + serialBias, 0.12f, 0.22f);
}

/**
 * @brief Applies the projected page pose to one generated dynamic track.
 *
 * The reticle API still consumes page-space
 * coordinates and degrees, while the source state remains in nautical miles and
 * radians until the very last moment.
 *
 * @param track Track state to project.
 * @param ownship Current ownship business frame.
 * @param projector Client-side user-space projector.
 */
void ApplyProjectedTrackPose(TutorialTrackState& track,
                             const OwnshipState& ownship,
                             const mfd::UserSpaceProjector& projector)
{
    if (track.reticle == nullptr)
    {
        return;
    }

    track.reticle->SetVisible(track.visible);
    track.reticle->SetPosition(projector.ToPagePosition(BuildTrackWorldPositionNm(track, ownship)));
    track.reticle->SetRotationDegrees(
        projector.ToPageRotationDegrees(BuildTrackWorldHeadingRadians(track, ownship)));
    track.reticle->Primitive01().SetSize({ComputeTrackSymbolSize(track), ComputeTrackSymbolSize(track)});
    track.reticle->Primitive01().SetLineStyle(
        track.persistent ? mfd::LineStyle::Solid
                         : ((track.serial % 2U) == 0U ? mfd::LineStyle::Dashed : mfd::LineStyle::Dotted));
}

/**
 * @brief Applies one visibility state to transient tracks only.
 * @param transientTracks Current transient tutorial tracks.
 * @param visible Desired visibility for the transient list.
 */
void SetTransientTrackVisibility(std::vector<TutorialTrackState>& transientTracks, const bool visible)
{
    for (TutorialTrackState& track : transientTracks)
    {
        track.visible = visible;
        if (track.reticle != nullptr)
        {
            track.reticle->SetVisible(visible);
        }
    }
}

/**
 * @brief Updates the appearance of one track from the authoritative strobe feedback.
 * @param track Track to restyle.
 */
void UpdateTrackVisualFromStrobeFeedback(TutorialTrackState& track)
{
    if (track.reticle == nullptr)
    {
        return;
    }

    const bool captured = track.reticle->IsStrobeCaptured();
    track.reticle->SetColor(
        captured ? mfd::ColorRgba {255, 112, 96, 255} : mfd::ColorRgba {80, 255, 185, 255});
    track.reticle->SetThickness(captured ? kCapturedTrackThickness : kNominalTrackThickness);
}

/**
 * @brief Updates all track visuals from the latest runtime feedback.
 * @param persistentTracks Persistent linked pair.
 * @param transientTracks Transient tutorial tracks.
 */
void UpdateCapturedTrackVisuals(std::array<TutorialTrackState, 2>& persistentTracks,
                                std::vector<TutorialTrackState>& transientTracks)
{
    for (TutorialTrackState& track : persistentTracks)
    {
        UpdateTrackVisualFromStrobeFeedback(track);
    }

    for (TutorialTrackState& track : transientTracks)
    {
        UpdateTrackVisualFromStrobeFeedback(track);
    }
}

/**
 * @brief Updates the static circle driven by transient-track visibility.
 *
 * @param circle Generated static circle handle.
 * @param transientTrackCount Current number of transient tracks.
 * @param declutterVisible Visibility flag applied to transient tracks.
 */
void UpdateTransientCircleReticle(tutorial_ui::Page1MfdTutorialCircleReticle& circle,
                                  const std::size_t transientTrackCount,
                                  const bool declutterVisible)
{
    circle.SetVisible(true);
    circle.SetColor(
        declutterVisible ? mfd::ColorRgba {0, 255, 128, 255} : mfd::ColorRgba {0, 96, 48, 255});
    circle.Primitive01().SetRadius(0.42f + 0.015f * static_cast<float>(transientTrackCount));
    circle.Primitive01().SetThickness(0.0045f);
    circle.Primitive01().SetLineStyle(declutterVisible ? mfd::LineStyle::Solid : mfd::LineStyle::Dotted);
}

/**
 * @brief Animates the default Page1 strobe reticle through its exposed line primitives.
 *
 * This is the generated-API demonstration for the "active strobe primitive"
 * feature: when `Default` is the selected strobe, the two exposed line
 * primitives can be restyled directly through `page1.defaultReticle`.
 *
 * @tparam TStrobeReticle Generated default-strobe reticle wrapper type.
 * @param reticle Generated strobe reticle wrapper to mutate.
 * @param elapsedSeconds Elapsed tutorial time in seconds.
 */
template <typename TStrobeReticle>
void UpdateDefaultStrobeReticle(TStrobeReticle& reticle, const float elapsedSeconds)
{
    const float styleBlend = 0.5f + 0.5f * std::sin(elapsedSeconds * 1.4f);

    reticle.SetRotationDegrees(0.0f);
    reticle.SetScale({1.0f, 1.0f});
    reticle.HorizontalLine().SetLineStyle(styleBlend >= 0.5f ? mfd::LineStyle::Solid : mfd::LineStyle::Dotted);
    reticle.HorizontalLine().SetThickness(0.0038f + 0.0014f * styleBlend);
    reticle.HorizontalLine().SetColor({255, 224, 120, 255});
    reticle.VerticalLine().SetLineStyle(styleBlend >= 0.5f ? mfd::LineStyle::Dashed : mfd::LineStyle::Solid);
    reticle.VerticalLine().SetThickness(0.0038f + 0.0010f * (1.0f - styleBlend));
    reticle.VerticalLine().SetColor({255, 244, 210, 255});
}

/**
 * @brief Animates the alternative Page1 strobe reticle and its exposed label.
 *
 * The reticle itself rotates and scales. Because the authored `aircraft_label`
 * primitive opts out of parent reticle rotation and scale, the label stays
 * upright and size-stable until this helper explicitly rotates or scales the
 * primitive again during the manual-override phase.
 *
 * @tparam TStrobeReticle Generated alternative-strobe reticle wrapper type.
 * @param reticle Generated strobe reticle wrapper to mutate.
 * @param elapsedSeconds Elapsed tutorial time in seconds.
 */
template <typename TStrobeReticle>
void UpdateAlternativeStrobeReticle(TStrobeReticle& reticle, const float elapsedSeconds)
{
    const float parentPulse = std::sin(elapsedSeconds * 0.9f);
    const float parentScale = 1.0f + kStrobeScaleAmplitude * (0.5f + 0.5f * std::sin(elapsedSeconds * 0.7f));
    const float manualPhaseSeconds = std::fmod(elapsedSeconds, 2.0f * kStrobeManualLabelPhaseSeconds);
    const bool manualPrimitiveOverride = manualPhaseSeconds >= kStrobeManualLabelPhaseSeconds;

    reticle.SetRotationDegrees(kStrobeRotationAmplitudeDegrees * parentPulse);
    reticle.SetScale({parentScale, parentScale});
    reticle.AircraftLabel().SetText(manualPrimitiveOverride ? "MAN" : "UPRT");
    reticle.AircraftLabel().SetRotationDegrees(manualPrimitiveOverride ? -18.0f : 0.0f);
    reticle.AircraftLabel().SetScale(
        manualPrimitiveOverride
            ? mfd::Vec2 {kStrobeLabelManualScale, kStrobeLabelManualScale}
            : mfd::Vec2 {1.0f, 1.0f});
}

/**
 * @brief Advances one persistent linked track in business space.
 *
 * The pair connected by the cue stays in aircraft-relative domain
 * coordinates instead of raw page coordinates. The animation therefore updates
 * azimuth/range rates directly, then the projector turns the result into page
 * coordinates afterwards.
 *
 * @param track Persistent track to update.
 * @param deltaSeconds Elapsed frame time in seconds.
 */
void AdvancePersistentTrackInUserSpace(TutorialTrackState& track, const float deltaSeconds) noexcept
{
    track.distanceNm += track.distanceRateNmPerSecond * deltaSeconds;
    track.azimuthRadians += track.azimuthRateRadiansPerSecond * deltaSeconds;
    track.relativeHeadingRadians += track.relativeHeadingRateRadiansPerSecond * deltaSeconds;

    ReflectOnInterval(track.distanceNm, track.distanceRateNmPerSecond, kMinTrackRangeNm, kMaxTrackRangeNm);
    ReflectOnInterval(track.azimuthRadians,
                      track.azimuthRateRadiansPerSecond,
                      -kMaxTrackAzimuthRadians,
                      kMaxTrackAzimuthRadians);
    WrapRadians(track.relativeHeadingRadians);
}

/**
 * @brief Updates the dynamic cue linking the two persistent business-space tracks.
 *
 * The cue itself still needs page-space points because the generated polyline
 * API is already the final render-space layer. The important point is that the
 * two linked tracks keep their source data in business coordinates until this
 * conversion.
 *
 * @param link Dynamic polyline reticle connecting the persistent pair.
 * @param persistentTracks Persistent pair expressed in business coordinates.
 * @param ownship Current ownship frame.
 * @param projector Projector used to convert to page positions.
 */
void UpdatePersistentTrackLink(tutorial_ui::InspiredSteeringCueDynamicReticle& link,
                               const std::array<TutorialTrackState, 2>& persistentTracks,
                               const OwnshipState& ownship,
                               const mfd::UserSpaceProjector& projector)
{
    std::vector<mfd::Vec2> points;
    points.reserve(2U);
    points.push_back(projector.ToPagePosition(BuildTrackWorldPositionNm(persistentTracks[0], ownship)));
    points.push_back(projector.ToPagePosition(BuildTrackWorldPositionNm(persistentTracks[1], ownship)));

    link.SetVisible(true);
    link.SetPosition({0.0f, 0.0f});
    link.Cue().SetClosed(false);
    link.Cue().SetPoints(std::move(points));
}

/**
 * @brief Creates one persistent linked track with deterministic business data.
 * @param handle Generated dynamic reticle handle.
 * @param serial Stable serial used for styling.
 * @param distanceNm Initial range in nautical miles.
 * @param azimuthRadians Initial azimuth in radians.
 * @param relativeHeadingRadians Initial heading relative to the aircraft.
 * @param distanceRateNmPerSecond Range animation speed.
 * @param azimuthRateRadiansPerSecond Azimuth animation speed.
 * @param relativeHeadingRateRadiansPerSecond Heading animation speed.
 * @return Fully initialized persistent track state.
 */
TutorialTrackState CreatePersistentTrack(tutorial_ui::MfdTutorialRadarTrackDynamicReticle& handle,
                                         const std::uint32_t serial,
                                         const float distanceNm,
                                         const float azimuthRadians,
                                         const float relativeHeadingRadians,
                                         const float distanceRateNmPerSecond,
                                         const float azimuthRateRadiansPerSecond,
                                         const float relativeHeadingRateRadiansPerSecond)
{
    TutorialTrackState track;
    track.reticle = &handle;
    track.distanceNm = distanceNm;
    track.azimuthRadians = azimuthRadians;
    track.relativeHeadingRadians = relativeHeadingRadians;
    track.distanceRateNmPerSecond = distanceRateNmPerSecond;
    track.azimuthRateRadiansPerSecond = azimuthRateRadiansPerSecond;
    track.relativeHeadingRateRadiansPerSecond = relativeHeadingRateRadiansPerSecond;
    track.serial = serial;
    track.visible = true;
    track.persistent = true;
    return track;
}

/**
 * @brief Recreates the persistent tutorial dynamic handles after one authored reset.
 * @param trackSet Generated track set used to spawn the persistent pair.
 * @param linkSet Generated cue set used to spawn the linking cue.
 * @param serial Serial counter updated for each recreated track.
 * @param persistentTracks Destination persistent pair.
 * @param persistentTrackLink Destination link handle pointer.
 */
void RebuildPersistentTutorialScene(
    tutorial_ui::MfdTutorialRadarTrackDynamicReticleSet& trackSet,
    tutorial_ui::InspiredSteeringCueDynamicReticleSet& linkSet,
    std::uint32_t& serial,
    std::array<TutorialTrackState, 2>& persistentTracks,
    tutorial_ui::InspiredSteeringCueDynamicReticle*& persistentTrackLink)
{
    persistentTracks = {
        CreatePersistentTrack(trackSet.Create(), serial++, 12.0f, -0.42f, 0.20f, 1.20f, 0.18f, 0.30f),
        CreatePersistentTrack(trackSet.Create(), serial++, 21.0f, 0.34f, -0.35f, -1.05f, -0.15f, -0.24f)};

    persistentTrackLink = &linkSet.Create();

    // Read-back demonstration: Get* accessors resolve override -> authored JSON
    // -> model default without ever staging a command, so this still reflects
    // the authored baseline because no Set* call has touched this handle yet.
    const bool authoredLinkVisible = persistentTrackLink->GetVisible();
    const mfd::Vec2 authoredLinkPosition = persistentTrackLink->GetPosition();

    persistentTrackLink->SetVisible(true);
    persistentTrackLink->SetColor({255, 192, 96, 255});
    persistentTrackLink->SetThickness(kTrackLinkThickness);
    persistentTrackLink->Cue().SetLineStyle(mfd::LineStyle::Solid);

    std::cout << "Tutorial: persistentTrackLink authored visible=" << (authoredLinkVisible ? "true" : "false")
              << " position=(" << authoredLinkPosition.x << ", " << authoredLinkPosition.y << ")"
              << " visible after SetVisible=" << (persistentTrackLink->GetVisible() ? "true" : "false") << '\n';
}

/**
 * @brief Generates one transient track ahead of the aircraft in business coordinates.
 *
 * Range and azimuth are generated directly in domain
 * units:
 * - range in nautical miles
 * - azimuth around the aircraft nose in radians
 *
 * The 40 NM upper bound matches the scale used by `kPageUnitsPerNm`.
 *
 * @param rng Random generator used by the tutorial.
 * @param serial Monotonic serial used to vary the visuals slightly.
 * @return Newly generated transient track state.
 */
TutorialTrackState GenerateTransientTrackState(std::mt19937& rng, const std::uint32_t serial)
{
    std::uniform_real_distribution<float> azimuthDistribution(-kMaxTrackAzimuthRadians, kMaxTrackAzimuthRadians);
    std::uniform_real_distribution<float> rangeDistribution(kMinTrackRangeNm, kMaxTrackRangeNm);
    std::uniform_real_distribution<float> headingDistribution(-kPi, kPi);

    TutorialTrackState track;
    track.distanceNm = rangeDistribution(rng);
    track.azimuthRadians = azimuthDistribution(rng);
    track.relativeHeadingRadians = headingDistribution(rng);
    track.serial = serial;
    track.visible = true;
    track.persistent = false;
    return track;
}

/**
 * @brief Logs one transient track with both business-space and page-space values.
 * @param track Newly spawned transient track.
 * @param ownship Ownship state used as the projection frame.
 * @param pagePosition Projected page-space position.
 */
void LogSpawnedTrack(const TutorialTrackState& track, const OwnshipState& ownship, const mfd::Vec2 pagePosition)
{
    std::cout << "Tutorial: spawned transient track #" << track.serial
              << " ownshipHeading=" << ownship.headingRadians << " rad"
              << " azimuth=" << track.azimuthRadians << " rad"
              << " distance=" << track.distanceNm << " NM"
              << " page=(" << pagePosition.x << ", " << pagePosition.y << ")"
              << '\n';
}

/**
 * @brief Counts all tracks currently reported captured by the active page strobe.
 * @param persistentTracks Persistent linked pair.
 * @param transientTracks Transient tutorial tracks.
 * @return Number of captured tracks.
 */
std::size_t CountCapturedTracks(const std::array<TutorialTrackState, 2>& persistentTracks,
                                const std::vector<TutorialTrackState>& transientTracks)
{
    const auto capturedPredicate =
        [](const TutorialTrackState& track)
        {
            return track.reticle != nullptr && track.reticle->IsStrobeCaptured();
        };

    return static_cast<std::size_t>(std::count_if(persistentTracks.begin(), persistentTracks.end(), capturedPredicate)) +
           static_cast<std::size_t>(std::count_if(transientTracks.begin(), transientTracks.end(), capturedPredicate));
}

/**
 * @brief Rebuilds the tutorial transports after the window was detected closed.
 *
 * Recreating the command client and the feedback receiver drops the stale
 * sockets, and `ui.Initialize()` re-sends the full authored state on the next
 * batch so a freshly relaunched window starts from a clean baseline.
 *
 * @param loaded Loaded window configuration used to rebuild the transports.
 * @param client Command client rebuilt in place.
 * @param feedbackChannel Feedback receiver rebuilt in place when UDP feedback is configured.
 * @param ui Generated tutorial UI re-armed for a full reinitialization.
 * @param monitor Liveness monitor re-armed for a fresh connection.
 */
void ResetTutorialConnections(const mfd::LoadedWindowConfiguration& loaded,
                              mfd::CommandClient& client,
                              std::unique_ptr<mfd::IExchangeChannel>& feedbackChannel,
                              tutorial_ui::TutorialUi& ui,
                              mfd::client::WindowLivenessMonitor& monitor)
{
    client = mfd::CommandClient(*loaded.window.commandTransports.udp, loaded.generatedTransportMap);
    if (loaded.window.feedbackTransports.udp.has_value())
    {
        feedbackChannel = mfd::CreateFeedbackReceiverChannel(*loaded.window.feedbackTransports.udp);
    }

    ui.Initialize();
    monitor.Reset();
}

int mainImpl()
{
    mfd::JsonLoader loader;
    const mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(std::string(kWindowFile));
    if (!loaded.window.commandTransports.udp.has_value())
    {
        throw std::runtime_error("Tutorial window must expose an UDP command transport");
    }
    if (!loaded.generatedTransportMap.has_value())
    {
        throw std::runtime_error("Tutorial window must expose a generated transport map next to the JSON file");
    }

    mfd::CommandClient client(*loaded.window.commandTransports.udp, loaded.generatedTransportMap);
    if (!client.IsReady())
    {
        throw std::runtime_error("Unable to initialize command client: " + client.LastError());
    }

    std::unique_ptr<mfd::IExchangeChannel> feedbackChannel;
    if (loaded.window.feedbackTransports.udp.has_value())
    {
        feedbackChannel = mfd::CreateFeedbackReceiverChannel(*loaded.window.feedbackTransports.udp);
    }

    // Detect a window shutdown (graceful close or abrupt termination) so the
    // client can drop its stale transports and rebuild them cleanly.
    mfd::client::WindowLivenessMonitor livenessMonitor(2.0);

    std::mt19937 rng(42U);
    std::uniform_real_distribution<float> ownshipHeadingDistribution(-kPi, kPi);

    tutorial_ui::TutorialUi generatedUi;
    auto& page1 = generatedUi.Page1();
    auto& page2 = generatedUi.Page2();
    auto& page1TrackSet = page1.DynamicMfdTutorialRadarTrack();
    auto& page1TrackLinkSet = page1.DynamicInspiredSteeringCue();
    auto& page1Circle = page1.mfdTutorialCircle;
    auto& page2ProgressBar = page2.mfdTutorialProgressBar;
    auto& progressFill = page2ProgressBar.FillBar();
    auto& page1Strobe = page1.strobe;
    auto& page1DefaultStrobeReticle = page1.defaultReticle;
    auto& page1AlternativeStrobeReticle = page1.strobe1Reticle;

    // The radar-track set acts as the waypoint-like magnet target catalog for
    // this tutorial: the strobe may snap to those dynamic contacts.
    page1TrackSet.SetStrobeMagnetEnabled(true);

    // The steering cue still remains capturable through runtime feedback, but
    // it must never magnetize the strobe or pull it along.
    page1TrackLinkSet.SetStrobeMagnetEnabled(false);

    // The aircraft symbol is authored statically in Page1 JSON at the same
    // anchor used by `BuildTutorialProjector`. The runtime loop updates only
    // track business coordinates; page-space poses are derived later.
    OwnshipState ownship;
    ownship.worldPositionNm = {0.0f, 0.0f};
    ownship.headingRadians = ownshipHeadingDistribution(rng);

    std::vector<TutorialTrackState> transientTracks;
    transientTracks.reserve(kMaxTransientTracks);

    page2ProgressBar.SetVisible(true);
    progressFill.SetVisible(true);

    Require(client.ActivatePage(page1), client, "Unable to activate tutorial Page1");
    bool page1Active = true;
    bool useAlternativeStrobe = false;
    bool transientDeclutterVisible = true;

    std::uint32_t serial = 1U;
    std::array<TutorialTrackState, 2> persistentTracks {};
    tutorial_ui::InspiredSteeringCueDynamicReticle* persistentTrackLink = nullptr;
    RebuildPersistentTutorialScene(page1TrackSet, page1TrackLinkSet, serial, persistentTracks, persistentTrackLink);

    const mfd::UserSpaceProjector startupProjector = BuildTutorialProjector(ownship);
    for (TutorialTrackState& track : persistentTracks)
    {
        ApplyProjectedTrackPose(track, ownship, startupProjector);
    }
    UpdatePersistentTrackLink(*persistentTrackLink, persistentTracks, ownship, startupProjector);
    UpdateCapturedTrackVisuals(persistentTracks, transientTracks);
    // Read-back demonstration: GetVisible/GetPosition resolve override -> authored
    // JSON -> model default without staging a command, so this reflects the
    // authored baseline because no Set* call has touched this reticle yet.
    const bool authoredCircleVisible = page1Circle.GetVisible();
    const mfd::Vec2 authoredCirclePosition = page1Circle.GetPosition();

    UpdateTransientCircleReticle(page1Circle, 0U, transientDeclutterVisible);
    UpdateDefaultStrobeReticle(page1DefaultStrobeReticle, 0.0f);

    std::cout << "Tutorial: page1Circle authored visible=" << (authoredCircleVisible ? "true" : "false")
              << " position=(" << authoredCirclePosition.x << ", " << authoredCirclePosition.y << ")"
              << " visible after UpdateTransientCircleReticle=" << (page1Circle.GetVisible() ? "true" : "false") << '\n';

    // Read-back demonstration: AircraftLabel().GetText() before/after the SetText
    // call performed inside UpdateAlternativeStrobeReticle. GetText() returns a
    // std::string_view aliasing handle-owned storage, so snapshot it into an
    // owning std::string before the SetText override below changes that storage.
    const std::string authoredAircraftLabelText {page1AlternativeStrobeReticle.AircraftLabel().GetText()};

    UpdateAlternativeStrobeReticle(page1AlternativeStrobeReticle, 0.0f);

    std::cout << "Tutorial: AircraftLabel authored text=\"" << authoredAircraftLabelText << "\""
              << " text after UpdateAlternativeStrobeReticle=\"" << page1AlternativeStrobeReticle.AircraftLabel().GetText() << "\"\n";
    if (page1Strobe.IsValid())
    {
        page1Strobe.SetActive(true);
        page1Strobe.SetPosition(startupProjector.ToPagePosition(BuildTrackWorldPositionNm(persistentTracks[0], ownship)));
    }

    generatedUi.Initialize();

    auto nextTrackTime = std::chrono::steady_clock::now() + kTrackInterval;
    auto nextPageTime = std::chrono::steady_clock::now() + kPageSwitchInterval;
    auto nextStrobeSwitchTime = std::chrono::steady_clock::now() + kStrobeSwitchInterval;
    const auto progressStartTime = std::chrono::steady_clock::now();
    auto previousFrameTime = progressStartTime;

    while (true)
    {
        const auto now = std::chrono::steady_clock::now();
        const float deltaSeconds =
            std::min(std::chrono::duration<float>(now - previousFrameTime).count(), kMaxFrameDeltaSeconds);
        previousFrameTime = now;

        const float elapsedProgressSeconds = std::chrono::duration<float>(now - progressStartTime).count();
        const float progressPhase =
            std::fmod(elapsedProgressSeconds, kProgressLoopDurationSeconds) / kProgressLoopDurationSeconds;
        const float progressWidth = std::max(0.001f, kProgressBarMaxWidth * progressPhase);
        const float progressCenterX = kProgressBarLeftEdge + progressWidth * 0.5f;
        progressFill.SetSize({progressWidth, kProgressBarHeight});
        progressFill.SetPosition({progressCenterX, 0.0f});
        UpdateDefaultStrobeReticle(page1DefaultStrobeReticle, elapsedProgressSeconds);
        UpdateAlternativeStrobeReticle(page1AlternativeStrobeReticle, elapsedProgressSeconds);

        for (TutorialTrackState& track : persistentTracks)
        {
            AdvancePersistentTrackInUserSpace(track, deltaSeconds);
        }

        if (now >= nextPageTime)
        {
            page1Active = !page1Active;
            Require(page1Active ? client.ActivatePage(page1) : client.ActivatePage(page2),
                    client,
                    "Unable to activate tutorial page");
            nextPageTime += kPageSwitchInterval;
        }

        if (page1Strobe.IsValid() && now >= nextStrobeSwitchTime)
        {
            useAlternativeStrobe = !useAlternativeStrobe;
            page1Strobe = useAlternativeStrobe ? page1.strobe1 : page1.defaultStrobe;
            std::cout << "Tutorial: switched Page1 to strobe `"
                      << (useAlternativeStrobe ? page1.strobe1.Name() : page1.defaultStrobe.Name()) << "`."
                      << '\n';
            nextStrobeSwitchTime += kStrobeSwitchInterval;
        }

        if (now >= nextTrackTime)
        {
            if (transientTracks.size() >= kMaxTransientTracks)
            {
                page1TrackSet.Remove(*transientTracks.front().reticle);
                transientTracks.erase(transientTracks.begin());
                UpdateTransientCircleReticle(page1Circle, transientTracks.size(), transientDeclutterVisible);
            }

            // The heading change affects every projected contact because the
            // aircraft-local frame is rebuilt from `ownship.headingRadians`.
            ownship.headingRadians = ownshipHeadingDistribution(rng);

            const std::uint32_t trackSerial = serial++;
            if ((trackSerial % 5U) == 0U)
            {
                transientDeclutterVisible = !transientDeclutterVisible;
                SetTransientTrackVisibility(transientTracks, transientDeclutterVisible);
            }

            auto& generatedTrack = page1TrackSet.Create();
            TutorialTrackState track = GenerateTransientTrackState(rng, trackSerial);
            track.reticle = &generatedTrack;
            track.visible = transientDeclutterVisible;
            transientTracks.push_back(track);

            UpdateTransientCircleReticle(page1Circle, transientTracks.size(), transientDeclutterVisible);

            const mfd::UserSpaceProjector projector = BuildTutorialProjector(ownship);
            ApplyProjectedTrackPose(transientTracks.back(), ownship, projector);
            UpdateCapturedTrackVisuals(persistentTracks, transientTracks);

            if (page1Strobe.IsValid())
            {
                page1Strobe.SetActive(true);
                page1Strobe.SetPosition(
                    projector.ToPagePosition(BuildTrackWorldPositionNm(transientTracks.back(), ownship)));
            }

            LogSpawnedTrack(
                transientTracks.back(),
                ownship,
                projector.ToPagePosition(BuildTrackWorldPositionNm(transientTracks.back(), ownship)));

            nextTrackTime += kTrackInterval;
        }

        if (feedbackChannel != nullptr)
        {
            std::string feedbackError;
            const std::size_t appliedFeedbackCount = generatedUi.PollFeedback(*feedbackChannel, 8U, &feedbackError);
            if (!feedbackError.empty())
            {
                std::cerr << "Runtime feedback decode error: " << feedbackError << '\n';
            }

            livenessMonitor.Observe(generatedUi.TotalDecodedFeedbackPackets(),
                                    generatedUi.WindowReportedClosing(),
                                    static_cast<double>(elapsedProgressSeconds));
            if (livenessMonitor.ConsumeDisconnect())
            {
                std::cout << "Tutorial: window closed, resetting client connections.\n";
                ResetTutorialConnections(loaded, client, feedbackChannel, generatedUi, livenessMonitor);
                continue;
            }

            if (appliedFeedbackCount > 0U)
            {
                const tutorial_ui::StrobeType* selectedPage1Strobe = page1Strobe.SelectedType();
                const std::size_t capturedCount = CountCapturedTracks(persistentTracks, transientTracks);

                std::cout << "Runtime feedback: Page1.active=" << (page1.IsActive() ? "true" : "false")
                          << " Page2.active=" << (page2.IsActive() ? "true" : "false")
                          << " Page1.strobe="
                          << (selectedPage1Strobe == nullptr ? "<none>" : selectedPage1Strobe->Name())
                          << " capturedTracks=" << capturedCount << '\n';
            }
        }

        const mfd::UserSpaceProjector projector = BuildTutorialProjector(ownship);
        for (TutorialTrackState& track : persistentTracks)
        {
            ApplyProjectedTrackPose(track, ownship, projector);
        }
        for (TutorialTrackState& track : transientTracks)
        {
            ApplyProjectedTrackPose(track, ownship, projector);
        }

        UpdatePersistentTrackLink(*persistentTrackLink, persistentTracks, ownship, projector);
        UpdateCapturedTrackVisuals(persistentTracks, transientTracks);

        if (page1Strobe.IsValid())
        {
            const TutorialTrackState& strobeTarget = transientTracks.empty() ? persistentTracks[0] : transientTracks.back();
            page1Strobe.SetActive(true);
            page1Strobe.SetPosition(projector.ToPagePosition(BuildTrackWorldPositionNm(strobeTarget, ownship)));
        }

        const std::vector<mfd::UserCommand> commands = generatedUi.BuildBatch();
        if (!commands.empty())
        {
            Require(client.SendBatch(commands), client, "Unable to send tutorial batch");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
} // namespace

int main()
{
    try
    {
        return mainImpl();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "tutorial_client error: " << exception.what() << '\n';
        return 1;
    }
}
