/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Regression tests for the LHLD projection and mini-simulation.
 */

#include <cmath>
#include <cstddef>

#include <gtest/gtest.h>

#include "MfdProjection.h"
#include "MfdRadarSimulation.h"

namespace
{
lhld::RadarTrack MakeTrack(const float azimuthDeg, const float rangeNm)
{
    lhld::RadarTrack track;
    track.active = true;
    track.rangeNm = rangeNm;
    track.azimuthDeg = azimuthDeg;
    track.elevationDeg = 0.0f;
    track.headingDeg = 90.0f;
    track.hostile = true;
    return track;
}
} // namespace

TEST(LhldProjection, RwsHidesTrackOutsideScanVolume)
{
    // azScanDeg 60 represents a +/-30 degree search volume.
    const lhld::RadarTrack track = MakeTrack(60.0f, 20.0f);
    const lhld::RadarSettings radar {};
    const lhld::OwnshipState ownship {};

    EXPECT_FALSE(lhld::ProjectRadarTrack(track, radar, ownship).visible);
}

TEST(LhldProjection, RadarBarsExpandElevationSearchVolume)
{
    lhld::RadarTrack track = MakeTrack(0.0f, 20.0f);
    track.elevationDeg = 2.5f;

    lhld::RadarSettings radar;
    radar.scanBars = 1;
    const lhld::OwnshipState ownship {};

    EXPECT_FALSE(lhld::ProjectRadarTrack(track, radar, ownship).visible);

    radar.scanBars = 4;
    EXPECT_TRUE(lhld::ProjectRadarTrack(track, radar, ownship).visible);
}

TEST(LhldProjection, SttKeepsLockedTrackVisibleOutsideScanVolume)
{
    const lhld::RadarTrack track = MakeTrack(60.0f, 20.0f);
    const lhld::RadarSettings radar {};
    const lhld::OwnshipState ownship {};

    const lhld::RadarTrackView view = lhld::ProjectBuggedTrack(track, radar, ownship);
    EXPECT_TRUE(view.visible);
    EXPECT_TRUE(std::isfinite(view.position.x));
    EXPECT_TRUE(std::isfinite(view.position.y));
    // The bearing is clamped to the right edge of the B-scope rather than dropped.
    EXPECT_NEAR(view.position.x, 0.46f, 1.0e-3f);
}

TEST(LhldProjection, SttKeepsLockedTrackVisibleBeyondRangeScale)
{
    // Range 60 NM on a 40 NM scale would filter a search track out.
    const lhld::RadarTrack track = MakeTrack(0.0f, 60.0f);
    const lhld::RadarSettings radar {};
    const lhld::OwnshipState ownship {};

    EXPECT_FALSE(lhld::ProjectRadarTrack(track, radar, ownship).visible);

    const lhld::RadarTrackView view = lhld::ProjectBuggedTrack(track, radar, ownship);
    EXPECT_TRUE(view.visible);
    // Clamped to the top (maximum range) of the scope.
    EXPECT_NEAR(view.position.y, 0.75f, 1.0e-3f);
}

TEST(LhldProjection, BuildFrameKeepsBuggedTargetOutsideScanVolume)
{
    lhld::MfdInputSample input;
    input.tracks[0] = MakeTrack(60.0f, 20.0f);
    input.radar.submode = lhld::RadarSubmode::Stt;
    input.radar.buggedTrack = 0;

    const lhld::MfdFrame frame = lhld::BuildMfdFrame(input);
    EXPECT_TRUE(frame.radar.buggedVisible);
    EXPECT_TRUE(frame.radar.datablockVisible);
    EXPECT_TRUE(std::isfinite(frame.radar.buggedTrack.position.x));
    EXPECT_TRUE(std::isfinite(frame.radar.buggedTrack.position.y));
    EXPECT_FALSE(frame.radar.tracks[0].visible);
}

TEST(LhldProjection, RadarTrackAltitudeIsDerivedFromRangeElevationAndOwnship)
{
    lhld::RadarTrack track = MakeTrack(0.0f, 20.0f);
    track.elevationDeg = 5.0f;
    lhld::OwnshipState ownship;
    ownship.altitudeFt = 20000.0f;
    lhld::RadarSettings radar;
    radar.antennaElevationDeg = 5.0f;

    const lhld::RadarTrackView view = lhld::ProjectRadarTrack(track, radar, ownship);

    EXPECT_TRUE(view.visible);
    EXPECT_NEAR(view.altitudeThousandsFt, 30.59f, 0.05f);
}

TEST(LhldProjection, CursorUsesNormalizedUiCoordinatesOnBothAxes)
{
    lhld::MfdInputSample input;
    input.radar.cursorPosition = {1.0f, -1.0f};

    const lhld::RadarFrame frame = lhld::BuildMfdFrame(input).radar;

    EXPECT_NEAR(frame.cursorPosition.x, 0.46f, 1.0e-4f);
    EXPECT_NEAR(frame.cursorPosition.y, -0.75f, 1.0e-4f);
}

TEST(LhldProjection, SttCrossProvidesSteeringAtTargetRange)
{
    lhld::MfdInputSample input;
    input.ownship.headingDeg = 0.0f;
    input.ownship.speedKts = 420.0f;
    input.tracks[0] = MakeTrack(15.0f, 20.0f);
    input.tracks[0].headingDeg = 0.0f;
    input.tracks[0].speedKts = 0.0f;
    input.radar.submode = lhld::RadarSubmode::Stt;
    input.radar.buggedTrack = 0;

    const lhld::RadarFrame frame = lhld::BuildMfdFrame(input).radar;

    EXPECT_TRUE(frame.sttInterceptVisible);
    EXPECT_NEAR(frame.sttInterceptPosition.x, 0.115f, 1.0e-3f);
    EXPECT_NEAR(frame.sttInterceptPosition.y, frame.buggedTrack.position.y, 1.0e-4f);
}

TEST(LhldProjection, SttCrossIsHiddenOutsideSixtyDegreeCollisionAngle)
{
    lhld::MfdInputSample input;
    input.ownship.headingDeg = 0.0f;
    input.tracks[0] = MakeTrack(70.0f, 20.0f);
    input.tracks[0].headingDeg = 0.0f;
    input.tracks[0].speedKts = 0.0f;
    input.radar.submode = lhld::RadarSubmode::Stt;
    input.radar.buggedTrack = 0;

    const lhld::RadarFrame frame = lhld::BuildMfdFrame(input).radar;

    EXPECT_TRUE(frame.buggedVisible);
    EXPECT_FALSE(frame.sttInterceptVisible);
}

TEST(LhldProjection, RadarPowerStatesSuppressLivePresentation)
{
    lhld::MfdInputSample input;
    input.tracks[0] = MakeTrack(0.0f, 20.0f);
    input.radar.operatingState = lhld::RadarOperatingState::Silent;

    const lhld::RadarFrame silentFrame = lhld::BuildMfdFrame(input).radar;
    EXPECT_FALSE(silentFrame.radarPresentationVisible);
    EXPECT_FALSE(silentFrame.scanLineVisible);
    EXPECT_FALSE(silentFrame.tracks[0].visible);

    input.radar.operatingState = lhld::RadarOperatingState::Off;
    const lhld::RadarFrame offFrame = lhld::BuildMfdFrame(input).radar;
    EXPECT_FALSE(offFrame.radarPresentationVisible);
    EXPECT_FALSE(offFrame.tracks[0].visible);
}

TEST(LhldProjection, ExtrapolatedQualityReachesProjectedTrackView)
{
    lhld::RadarTrack track = MakeTrack(0.0f, 20.0f);
    track.quality = lhld::RadarTrackQuality::Extrapolated;

    const lhld::RadarTrackView view =
        lhld::ProjectRadarTrack(track, lhld::RadarSettings {}, lhld::OwnshipState {});

    EXPECT_TRUE(view.visible);
    EXPECT_TRUE(view.extrapolated);
}

TEST(LhldProjection, BuildFrameConnectsVisibleFlightPlanSteerpoints)
{
    lhld::MfdInputSample input;
    input.ownship.headingDeg = 0.0f;
    input.nav.rangeScaleNm = 80.0f;
    input.nav.steerpoints[0].number = 1;
    input.nav.steerpoints[0].bearingDeg = 0.0f;
    input.nav.steerpoints[0].rangeNm = 20.0f;
    input.nav.steerpoints[1].number = 2;
    input.nav.steerpoints[1].bearingDeg = 90.0f;
    input.nav.steerpoints[1].rangeNm = 30.0f;

    const lhld::MfdFrame frame = lhld::BuildMfdFrame(input);

    EXPECT_TRUE(frame.nav.steerpoints[0].visible);
    EXPECT_TRUE(frame.nav.steerpoints[1].visible);
    EXPECT_TRUE(frame.nav.flightPlanLegs[0].visible);
    EXPECT_TRUE(std::isfinite(frame.nav.flightPlanLegs[0].position.x));
    EXPECT_TRUE(std::isfinite(frame.nav.flightPlanLegs[0].position.y));
    EXPECT_GT(frame.nav.flightPlanLegs[0].length, 0.0f);
}

TEST(LhldProjection, HsdDeclutterHidesSecondaryNavigationSymbols)
{
    lhld::MfdInputSample input;
    input.ownship.headingDeg = 0.0f;
    input.nav.rangeScaleNm = 80.0f;
    input.nav.declutterActive = true;
    input.nav.bullseyeBearingDeg = 45.0f;
    input.nav.bullseyeRangeNm = 20.0f;
    input.nav.steerpoints[0].number = 1;
    input.nav.steerpoints[0].bearingDeg = 0.0f;
    input.nav.steerpoints[0].rangeNm = 20.0f;
    input.nav.steerpoints[1].number = 2;
    input.nav.steerpoints[1].bearingDeg = 90.0f;
    input.nav.steerpoints[1].rangeNm = 30.0f;

    const lhld::MfdFrame frame = lhld::BuildMfdFrame(input);

    EXPECT_TRUE(frame.nav.steerpoints[0].visible);
    EXPECT_TRUE(frame.nav.steerpoints[1].visible);
    EXPECT_FALSE(frame.nav.flightPlanLegs[0].visible);
    EXPECT_FALSE(frame.nav.bullseye.visible);
}

TEST(LhldProjection, HsdProjectsOnlyActiveFlightPlanSlots)
{
    lhld::MfdInputSample input;
    input.ownship.headingDeg = 0.0f;
    input.nav.rangeScaleNm = 80.0f;
    input.nav.waypointCount = 2;
    input.nav.steerpoints[0].bearingDeg = 0.0f;
    input.nav.steerpoints[0].rangeNm = 20.0f;
    input.nav.steerpoints[1].bearingDeg = 90.0f;
    input.nav.steerpoints[1].rangeNm = 30.0f;
    input.nav.steerpoints[2].bearingDeg = 180.0f;
    input.nav.steerpoints[2].rangeNm = 20.0f;

    const lhld::MfdFrame frame = lhld::BuildMfdFrame(input);

    EXPECT_TRUE(frame.nav.steerpoints[0].visible);
    EXPECT_TRUE(frame.nav.steerpoints[1].visible);
    EXPECT_TRUE(frame.nav.flightPlanLegs[0].visible);
    EXPECT_FALSE(frame.nav.steerpoints[2].visible);
    EXPECT_FALSE(frame.nav.flightPlanLegs[1].visible);
}

TEST(LhldProjection, HsdSupportsInsertedWaypointSlots)
{
    lhld::MfdInputSample input;
    input.ownship.headingDeg = 0.0f;
    input.nav.rangeScaleNm = 80.0f;
    input.nav.waypointCount = 7;
    input.nav.steerpoints[5].bearingDeg = 210.0f;
    input.nav.steerpoints[5].rangeNm = 32.0f;
    input.nav.steerpoints[6].bearingDeg = 240.0f;
    input.nav.steerpoints[6].rangeNm = 36.0f;

    const lhld::MfdFrame frame = lhld::BuildMfdFrame(input);

    EXPECT_TRUE(frame.nav.steerpoints[5].visible);
    EXPECT_TRUE(frame.nav.steerpoints[6].visible);
    EXPECT_TRUE(frame.nav.flightPlanLegs[5].visible);
    EXPECT_FALSE(frame.nav.steerpoints[7].visible);
}

TEST(LhldProjection, BuildFrameIgnoresInactiveBuggedIndex)
{
    lhld::MfdInputSample input;
    input.radar.submode = lhld::RadarSubmode::Stt;
    input.radar.buggedTrack = 3; // slot left inactive

    const lhld::MfdFrame frame = lhld::BuildMfdFrame(input);
    EXPECT_FALSE(frame.radar.buggedVisible);
    EXPECT_FALSE(frame.radar.datablockVisible);
}

TEST(LhldSimulation, StepIsDeterministicAndFinite)
{
    lhld::MfdRadarSimulation first;
    lhld::MfdRadarSimulation second;
    for (int step = 0; step < 200; ++step)
    {
        first.Step(0.05f);
        second.Step(0.05f);
    }

    const lhld::MfdInputSample& a = first.Inputs();
    const lhld::MfdInputSample& b = second.Inputs();
    for (std::size_t index = 0; index < lhld::kMaxRadarTracks; ++index)
    {
        EXPECT_EQ(a.tracks[index].active, b.tracks[index].active);
        EXPECT_FLOAT_EQ(a.tracks[index].rangeNm, b.tracks[index].rangeNm);
        EXPECT_TRUE(std::isfinite(a.tracks[index].rangeNm));
        EXPECT_TRUE(std::isfinite(a.tracks[index].azimuthDeg));
        EXPECT_TRUE(std::isfinite(a.tracks[index].elevationDeg));
        EXPECT_TRUE(std::isfinite(a.tracks[index].closureKts));
    }
    EXPECT_GE(a.radar.antennaAzimuthDeg, -30.0f);
    EXPECT_LE(a.radar.antennaAzimuthDeg, 30.0f);
}

TEST(LhldSimulation, FourBarScanSweepsSlowerThanOneBar)
{
    lhld::MfdRadarSimulation oneBar;
    lhld::RadarSettings oneBarControls;
    oneBarControls.scanBars = 1;
    oneBar.ApplyRadarControls(oneBarControls);
    oneBar.Step(0.05f);

    lhld::MfdRadarSimulation fourBar;
    lhld::RadarSettings fourBarControls;
    fourBarControls.scanBars = 4;
    fourBar.ApplyRadarControls(fourBarControls);
    fourBar.Step(0.05f);

    EXPECT_GT(oneBar.Inputs().radar.antennaAzimuthDeg, fourBar.Inputs().radar.antennaAzimuthDeg);
    EXPECT_GT(fourBar.Inputs().radar.antennaAzimuthDeg, 0.0f);
}

TEST(LhldStores, JettisonAllStoresClearsSmsInventory)
{
    lhld::StoresState stores;
    stores.masterArm = true;
    stores.releaseInProgress = true;
    stores.impactTimeRemainingSeconds = 12.0f;
    stores.releasedStation = 3;

    EXPECT_EQ(lhld::CountLoadedStores(stores), static_cast<int>(lhld::kStationCount));
    EXPECT_TRUE(lhld::IsStationLoaded(stores, 3));
    EXPECT_EQ(lhld::CountReadyAirGroundStores(stores), 3);

    lhld::JettisonAllStores(stores);

    EXPECT_EQ(lhld::CountLoadedStores(stores), 0);
    EXPECT_FALSE(lhld::IsStationLoaded(stores, 3));
    EXPECT_EQ(stores.selectedStation, 0);
    EXPECT_EQ(stores.readyCount, 0);
    EXPECT_FALSE(stores.masterArm);
    EXPECT_FALSE(stores.releaseInProgress);
    EXPECT_FLOAT_EQ(stores.impactTimeRemainingSeconds, 0.0f);
    EXPECT_EQ(stores.releasedStation, 0);
}
