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
lhld::RadarContact MakeContact(const float azimuthNorm, const float rangeNm)
{
    lhld::RadarContact contact;
    contact.active = true;
    contact.azimuthNorm = azimuthNorm;
    contact.rangeNm = rangeNm;
    contact.headingDeg = 90.0f;
    contact.altitudeFt = 20000.0f;
    contact.hostile = true;
    return contact;
}
} // namespace

TEST(LhldProjection, RwsHidesContactOutsideScanVolume)
{
    // azScanDeg 60 => +/-30 deg. azimuthNorm 1.0 maps to 60 deg, outside the scan.
    const lhld::RadarContact contact = MakeContact(1.0f, 20.0f);
    const lhld::RadarSettings radar {};
    const lhld::OwnshipState ownship {};

    EXPECT_FALSE(lhld::ProjectRadarContact(contact, radar, ownship).visible);
}

TEST(LhldProjection, RadarBarsExpandElevationSearchVolume)
{
    lhld::RadarContact contact = MakeContact(0.0f, 20.0f);
    contact.altitudeFt = 27000.0f;

    lhld::RadarSettings radar;
    radar.scanBars = 1;
    const lhld::OwnshipState ownship {};

    EXPECT_FALSE(lhld::ProjectRadarContact(contact, radar, ownship).visible);

    radar.scanBars = 4;
    EXPECT_TRUE(lhld::ProjectRadarContact(contact, radar, ownship).visible);
}

TEST(LhldProjection, SttKeepsLockedContactVisibleOutsideScanVolume)
{
    const lhld::RadarContact contact = MakeContact(1.0f, 20.0f);
    const lhld::RadarSettings radar {};
    const lhld::OwnshipState ownship {};

    const lhld::RadarContactView view = lhld::ProjectBuggedTarget(contact, radar, ownship);
    EXPECT_TRUE(view.visible);
    EXPECT_TRUE(std::isfinite(view.position.x));
    EXPECT_TRUE(std::isfinite(view.position.y));
    // The bearing is clamped to the right edge of the B-scope rather than dropped.
    EXPECT_NEAR(view.position.x, 0.70f, 1.0e-3f);
}

TEST(LhldProjection, SttKeepsLockedContactVisibleBeyondRangeScale)
{
    // Range 60 NM on a 40 NM scale would filter a search contact out.
    const lhld::RadarContact contact = MakeContact(0.0f, 60.0f);
    const lhld::RadarSettings radar {};
    const lhld::OwnshipState ownship {};

    EXPECT_FALSE(lhld::ProjectRadarContact(contact, radar, ownship).visible);

    const lhld::RadarContactView view = lhld::ProjectBuggedTarget(contact, radar, ownship);
    EXPECT_TRUE(view.visible);
    // Clamped to the top (maximum range) of the scope.
    EXPECT_NEAR(view.position.y, 0.75f, 1.0e-3f);
}

TEST(LhldProjection, BuildFrameKeepsBuggedTargetOutsideScanVolume)
{
    lhld::MfdInputSample input;
    input.contacts[0] = MakeContact(1.0f, 20.0f);
    input.radar.submode = lhld::RadarSubmode::Stt;
    input.radar.buggedContact = 0;

    const lhld::MfdFrame frame = lhld::BuildMfdFrame(input);
    EXPECT_TRUE(frame.radar.buggedVisible);
    EXPECT_TRUE(frame.radar.datablockVisible);
    EXPECT_TRUE(std::isfinite(frame.radar.buggedPosition.x));
    EXPECT_TRUE(std::isfinite(frame.radar.buggedPosition.y));
    // The plain search brick for the same contact still respects the scan gate.
    EXPECT_FALSE(frame.radar.contacts[0].visible);
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
    input.radar.buggedContact = 3; // slot left inactive

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
    for (std::size_t index = 0; index < lhld::kMaxContacts; ++index)
    {
        EXPECT_EQ(a.contacts[index].active, b.contacts[index].active);
        EXPECT_FLOAT_EQ(a.contacts[index].rangeNm, b.contacts[index].rangeNm);
        EXPECT_TRUE(std::isfinite(a.contacts[index].rangeNm));
        EXPECT_TRUE(std::isfinite(a.contacts[index].azimuthNorm));
        EXPECT_TRUE(std::isfinite(a.contacts[index].closureKts));
    }
    EXPECT_GE(a.radar.scanSweepNorm, -1.0f);
    EXPECT_LE(a.radar.scanSweepNorm, 1.0f);
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

    EXPECT_GT(oneBar.Inputs().radar.scanSweepNorm, fourBar.Inputs().radar.scanSweepNorm);
    EXPECT_GT(fourBar.Inputs().radar.scanSweepNorm, 0.0f);
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
