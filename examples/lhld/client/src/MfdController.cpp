/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the LHLD generated-UI adapter.
 */

#include "MfdController.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "LhldUi.h"
#include "MfdProjection.h"
#include "mfd/model/Types.h"

namespace lhld
{
namespace
{
constexpr mfd::ColorRgba kBright {140, 255, 190, 255};
constexpr mfd::ColorRgba kDim {40, 150, 95, 255};
constexpr mfd::ColorRgba kRadarSearch {255, 255, 255, 255};
constexpr mfd::ColorRgba kRadarTrackFile {210, 235, 0, 255};
constexpr mfd::ColorRgba kRadarExtrapolated {255, 48, 48, 255};
constexpr mfd::ColorRgba kHsdCurrent {120, 255, 255, 255};
constexpr mfd::ColorRgba kHsdDim {0, 210, 230, 220};
constexpr mfd::ColorRgba kHsdRoute {0, 200, 220, 180};

// Fixed A-G demo loadout used to keep the SMS header weapon consistent with the
// selected station.
constexpr std::array<const char*, kStationCount> kLoadout = {
    "AIM-9", "AIM-9", "GBU-12", "TGP", "TANK", "GBU-12", "GBU-12", "AIM-9", "AIM-9"};

mfd::Vec2 ToVec(const MfdVec2 value) noexcept
{
    return mfd::Vec2 {value.x, value.y};
}

MfdVec2 OffsetPosition(const MfdVec2 value, const MfdVec2 offset) noexcept
{
    return MfdVec2 {value.x + offset.x, value.y + offset.y};
}

std::string FormatInt(const float value)
{
    char buffer[24] {};
    std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::lround(value)));
    return buffer;
}

std::string FormatStationLabel(const StoresState& stores, const int station)
{
    const std::string stationPrefix = FormatInt(static_cast<float>(station)) + " ";
    if (!IsStationLoaded(stores, station))
    {
        return stationPrefix + "---";
    }
    return stationPrefix + kLoadout[static_cast<std::size_t>(station - 1)];
}

std::string FormatHeading(const float degrees)
{
    float wrapped = std::fmod(degrees, 360.0f);
    if (wrapped < 0.0f)
    {
        wrapped += 360.0f;
    }
    char buffer[8] {};
    std::snprintf(buffer, sizeof(buffer), "%03d", static_cast<int>(std::lround(wrapped)) % 360);
    return buffer;
}

const char* MasterModeCaption(const MasterMode mode) noexcept
{
    switch (mode)
    {
    case MasterMode::Nav:
        return "NAV";
    case MasterMode::AirToAir:
        return "A-A";
    case MasterMode::AirToGround:
        return "A-G";
    case MasterMode::Dogfight:
        return "DGFT";
    }
    return "NAV";
}

const char* RadarSubmodeCaption(const RadarSubmode submode) noexcept
{
    switch (submode)
    {
    case RadarSubmode::Rws:
        return "RWS";
    case RadarSubmode::Tws:
        return "TWS";
    case RadarSubmode::Stt:
        return "STT";
    }
    return "RWS";
}

bool HasActiveTrackState(const MfdInputSample& input, const RadarTrackState state) noexcept
{
    for (const RadarTrack& track : input.tracks)
    {
        if (track.active && track.state == state)
        {
            return true;
        }
    }
    return false;
}

const char* RadarStatusCaption(const MfdInputSample& input) noexcept
{
    if (HasActiveTrackState(input, RadarTrackState::SingleTargetTrack))
    {
        return "STT";
    }
    if (input.radar.submode == RadarSubmode::Rws &&
        HasActiveTrackState(input, RadarTrackState::SystemTarget))
    {
        return "SAM";
    }
    return RadarSubmodeCaption(input.radar.submode);
}

const char* SmsSubmodeCaption(const MasterMode mode) noexcept
{
    switch (mode)
    {
    case MasterMode::AirToGround:
        return "CCRP";
    case MasterMode::AirToAir:
        return "BORE";
    case MasterMode::Dogfight:
        return "DGFT";
    case MasterMode::Nav:
        return "NAV";
    }
    return "NAV";
}

const char* DeliveryModeCaption(const AirGroundDeliveryMode mode) noexcept
{
    switch (mode)
    {
    case AirGroundDeliveryMode::Ccip:
        return "CCIP";
    case AirGroundDeliveryMode::Ccrp:
        return "CCRP";
    }
    return "CCRP";
}

std::string FormatFixed(const float value, const int precision)
{
    char buffer[24] {};
    std::snprintf(buffer, sizeof(buffer), "%0.*f", precision, value);
    return buffer;
}

std::string FormatTrackAltitude(const RadarTrackView& view)
{
    return FormatInt(std::clamp(view.altitudeThousandsFt, -99.0f, 999.0f));
}

constexpr mfd::Vec2 kHiddenTrackPosition {2.0f, 2.0f};

mfd::ColorRgba TrackColor(const RadarTrackView& view, const bool trackFile) noexcept
{
    if (view.extrapolated)
    {
        return kRadarExtrapolated;
    }
    return trackFile ? kRadarTrackFile : kRadarSearch;
}

void ApplyTrackView(lhld_ui::RadarRwsTrackDynamicReticle& reticle,
                    const RadarTrackView& view,
                    const bool visible)
{
    reticle.SetVisible(visible);
    reticle.SetBlinkEnabled(view.extrapolated);
    if (!visible)
    {
        reticle.SetPosition(kHiddenTrackPosition);
        return;
    }
    reticle.SetPosition(ToVec(view.position));
    reticle.SetRotationDegrees(view.rotationDegrees);
    reticle.SetColor(TrackColor(view, false));
    reticle.TrackAltitude().SetText(FormatTrackAltitude(view));
}

void ApplyTrackView(lhld_ui::RadarTwsTrackDynamicReticle& reticle,
                    const RadarTrackView& view,
                    const bool visible)
{
    reticle.SetVisible(visible);
    reticle.SetBlinkEnabled(view.extrapolated);
    if (!visible)
    {
        reticle.SetPosition(kHiddenTrackPosition);
        return;
    }
    reticle.SetPosition(ToVec(view.position));
    reticle.SetRotationDegrees(view.rotationDegrees);
    reticle.SetColor(TrackColor(view, true));
    reticle.TrackAltitude().SetText(FormatTrackAltitude(view));
}

void ApplyTrackView(lhld_ui::RadarBuggedTrackDynamicReticle& reticle,
                    const RadarTrackView& view,
                    const bool visible)
{
    reticle.SetVisible(visible);
    reticle.SetBlinkEnabled(view.extrapolated);
    if (!visible)
    {
        reticle.SetPosition(kHiddenTrackPosition);
        return;
    }
    reticle.SetPosition(ToVec(view.position));
    reticle.SetRotationDegrees(view.rotationDegrees);
    reticle.SetColor(TrackColor(view, true));
    reticle.TrackAltitude().SetText(FormatTrackAltitude(view));
}

void ApplyTrackView(lhld_ui::RadarSttTrackDynamicReticle& reticle,
                    const RadarTrackView& view,
                    const bool visible)
{
    reticle.SetVisible(visible);
    reticle.SetBlinkEnabled(view.extrapolated);
    if (!visible)
    {
        reticle.SetPosition(kHiddenTrackPosition);
        return;
    }
    reticle.SetPosition(ToVec(view.position));
    reticle.SetRotationDegrees(view.rotationDegrees);
    reticle.SetColor(TrackColor(view, true));
    reticle.TrackAltitude().SetText(FormatTrackAltitude(view));
}

// The authored steerpoint reticles share one generated contract, so one local
// applicator is clearer than per-reticle copies.
template <typename SteerpointReticle>
void ApplySteerpoint(SteerpointReticle& reticle,
                     const NavSymbolView& view,
                     const int number,
                     const bool current,
                     const MfdVec2 displayOffset)
{
    reticle.SetVisible(view.visible);
    if (!view.visible)
    {
        return;
    }
    reticle.SetPosition(ToVec(OffsetPosition(view.position, displayOffset)));
    reticle.SetColor(current ? kHsdCurrent : kHsdDim);
    reticle.StptNum().SetText(FormatInt(static_cast<float>(number)));
}

void ApplyFlightPlanLeg(lhld_ui::NavFlightLeg1Reticle& reticle,
                        const FlightPlanLegView& view,
                        const MfdVec2 displayOffset)
{
    reticle.SetVisible(view.visible);
    if (!view.visible)
    {
        return;
    }

    reticle.SetPosition(ToVec(OffsetPosition(view.position, displayOffset)));
    reticle.SetRotationDegrees(view.rotationDegrees);
    reticle.SetScale(mfd::Vec2 {view.length, 1.0f});
    reticle.SetColor(kHsdRoute);
}

void ApplyFlightPlanLeg(lhld_ui::NavFlightLeg2Reticle& reticle,
                        const FlightPlanLegView& view,
                        const MfdVec2 displayOffset)
{
    reticle.SetVisible(view.visible);
    if (!view.visible)
    {
        return;
    }

    reticle.SetPosition(ToVec(OffsetPosition(view.position, displayOffset)));
    reticle.SetRotationDegrees(view.rotationDegrees);
    reticle.SetScale(mfd::Vec2 {view.length, 1.0f});
    reticle.SetColor(kHsdRoute);
}

void ApplyFlightPlanLeg(lhld_ui::NavFlightLeg3Reticle& reticle,
                        const FlightPlanLegView& view,
                        const MfdVec2 displayOffset)
{
    reticle.SetVisible(view.visible);
    if (!view.visible)
    {
        return;
    }

    reticle.SetPosition(ToVec(OffsetPosition(view.position, displayOffset)));
    reticle.SetRotationDegrees(view.rotationDegrees);
    reticle.SetScale(mfd::Vec2 {view.length, 1.0f});
    reticle.SetColor(kHsdRoute);
}

void ApplyFlightPlanLeg(lhld_ui::NavFlightLeg4Reticle& reticle,
                        const FlightPlanLegView& view,
                        const MfdVec2 displayOffset)
{
    reticle.SetVisible(view.visible);
    if (!view.visible)
    {
        return;
    }

    reticle.SetPosition(ToVec(OffsetPosition(view.position, displayOffset)));
    reticle.SetRotationDegrees(view.rotationDegrees);
    reticle.SetScale(mfd::Vec2 {view.length, 1.0f});
    reticle.SetColor(kHsdRoute);
}

void ApplyFlightPlanLeg(lhld_ui::NavFlightLeg5Reticle& reticle,
                        const FlightPlanLegView& view,
                        const MfdVec2 displayOffset)
{
    reticle.SetVisible(view.visible);
    if (!view.visible)
    {
        return;
    }

    reticle.SetPosition(ToVec(OffsetPosition(view.position, displayOffset)));
    reticle.SetRotationDegrees(view.rotationDegrees);
    reticle.SetScale(mfd::Vec2 {view.length, 1.0f});
    reticle.SetColor(kHsdRoute);
}

void ApplyFlightPlanLeg(lhld_ui::NavFlightLeg6Reticle& reticle,
                        const FlightPlanLegView& view,
                        const MfdVec2 displayOffset)
{
    reticle.SetVisible(view.visible);
    if (!view.visible)
    {
        return;
    }

    reticle.SetPosition(ToVec(OffsetPosition(view.position, displayOffset)));
    reticle.SetRotationDegrees(view.rotationDegrees);
    reticle.SetScale(mfd::Vec2 {view.length, 1.0f});
    reticle.SetColor(kHsdRoute);
}

void ApplyFlightPlanLeg(lhld_ui::NavFlightLeg7Reticle& reticle,
                        const FlightPlanLegView& view,
                        const MfdVec2 displayOffset)
{
    reticle.SetVisible(view.visible);
    if (!view.visible)
    {
        return;
    }

    reticle.SetPosition(ToVec(OffsetPosition(view.position, displayOffset)));
    reticle.SetRotationDegrees(view.rotationDegrees);
    reticle.SetScale(mfd::Vec2 {view.length, 1.0f});
    reticle.SetColor(kHsdRoute);
}

// The 20 legend slots are the same authored template reused on each page, so
// each page exposes an OsbNN() accessor with an identical contract. One local
// index-to-accessor applicator is clearer than three per-page copies.
template <typename OsbReticle>
void SetOsbLegend(OsbReticle& reticle, const int osbOneBased, const std::string& text)
{
    switch (osbOneBased)
    {
    case 1: reticle.Osb01().SetText(text); break;
    case 2: reticle.Osb02().SetText(text); break;
    case 3: reticle.Osb03().SetText(text); break;
    case 4: reticle.Osb04().SetText(text); break;
    case 5: reticle.Osb05().SetText(text); break;
    case 6: reticle.Osb06().SetText(text); break;
    case 7: reticle.Osb07().SetText(text); break;
    case 8: reticle.Osb08().SetText(text); break;
    case 9: reticle.Osb09().SetText(text); break;
    case 10: reticle.Osb10().SetText(text); break;
    case 11: reticle.Osb11().SetText(text); break;
    case 12: reticle.Osb12().SetText(text); break;
    case 13: reticle.Osb13().SetText(text); break;
    case 14: reticle.Osb14().SetText(text); break;
    case 15: reticle.Osb15().SetText(text); break;
    case 16: reticle.Osb16().SetText(text); break;
    case 17: reticle.Osb17().SetText(text); break;
    case 18: reticle.Osb18().SetText(text); break;
    case 19: reticle.Osb19().SetText(text); break;
    case 20: reticle.Osb20().SetText(text); break;
    default: break;
    }
}

void SetStationLabel(lhld_ui::SmsSmsStationsReticle& reticle, const int station, const std::string& text)
{
    switch (station)
    {
    case 1: reticle.Sta1().SetText(text); break;
    case 2: reticle.Sta2().SetText(text); break;
    case 3: reticle.Sta3().SetText(text); break;
    case 4: reticle.Sta4().SetText(text); break;
    case 5: reticle.Sta5().SetText(text); break;
    case 6: reticle.Sta6().SetText(text); break;
    case 7: reticle.Sta7().SetText(text); break;
    case 8: reticle.Sta8().SetText(text); break;
    case 9: reticle.Sta9().SetText(text); break;
    default: break;
    }
}

void ApplyStationLabels(lhld_ui::SmsSmsStationsReticle& reticle, const StoresState& stores)
{
    for (int station = 1; station <= static_cast<int>(kStationCount); ++station)
    {
        SetStationLabel(reticle, station, FormatStationLabel(stores, station));
    }
}

void ApplyRadar(
    lhld_ui::LhldUi& ui,
    const MfdInputSample& input,
    const RadarFrame& frame,
    std::array<lhld_ui::RadarRwsTrackDynamicReticle*, kMaxRadarTracks>& rwsTracks,
    std::array<lhld_ui::RadarTwsTrackDynamicReticle*, kMaxRadarTracks>& twsTracks,
    std::array<lhld_ui::RadarBuggedTrackDynamicReticle*, kMaxRadarTracks>& buggedTracks,
    std::array<lhld_ui::RadarSttTrackDynamicReticle*, kMaxRadarTracks>& sttTracks)
{
    auto& radar = ui.Radar();

    const bool searchPresentation = frame.radarPresentationVisible && input.radar.submode != RadarSubmode::Stt;
    const bool sttPresentation = frame.buggedVisible &&
        frame.buggedTrack.state == RadarTrackState::SingleTargetTrack;
    radar.strobe = radar.acquisitionStrobe;
    radar.strobe.SetActive(searchPresentation);
    radar.strobe.SetPosition(ToVec(frame.cursorPosition));
    radar.radarAcquisitionCursorVisual.SetVisible(searchPresentation);
    radar.radarAcquisitionCursorVisual.SetPosition(ToVec(frame.cursorPosition));

    radar.radarBscopeFrame.SetVisible(searchPresentation);
    radar.radarHorizon.SetVisible(frame.radarPresentationVisible);
    radar.radarScale.SetVisible(input.radar.operatingState != RadarOperatingState::Off);
    radar.radarOwnship.SetVisible(searchPresentation);
    radar.radarOwnship.SetPosition(mfd::Vec2 {0.0f, -0.52f});
    radar.radarSensorCue.SetVisible(input.radar.operatingState != RadarOperatingState::Off);
    radar.radarTopLabels.Mode().SetText(RadarStatusCaption(input));
    const bool expandedDisplay = input.radar.fieldOfView == RadarFieldOfView::Expanded;
    radar.radarFovLabel.Fov().SetText(expandedDisplay ? "EXP" : "NORM");
    radar.radarFovLabel.SetBlink(expandedDisplay, radar.fast);
    radar.radarElevationCaret.SetVisible(searchPresentation);
    radar.radarScanLine.SetVisible(frame.scanLineVisible);
    radar.radarScanLine.SetPosition(mfd::Vec2 {frame.scanLineX, 0.0f});
    radar.radarExpandReference.SetVisible(frame.expandedReferenceVisible);
    radar.radarExpandReference.SetPosition(ToVec(frame.expandedReferencePosition));
    radar.radarElevationCaret.SetPosition(mfd::Vec2 {0.0f, frame.elevationCaretY});
    radar.radarCursorData.SetVisible(searchPresentation);
    radar.radarCursorData.SetPosition(ToVec(frame.cursorPosition));
    radar.radarCursorData.MaximumAltitude().SetText(FormatInt(frame.cursorMaximumAltitudeThousandsFt));
    radar.radarCursorData.MinimumAltitude().SetText(FormatInt(frame.cursorMinimumAltitudeThousandsFt));
    radar.radarSttInterceptCross.SetVisible(sttPresentation && frame.sttInterceptVisible);
    radar.radarSttInterceptCross.SetPosition(ToVec(frame.sttInterceptPosition));

    radar.DynamicRadarRwsTrack().SetVisible(true);
    radar.DynamicRadarTwsTrack().SetVisible(true);
    radar.DynamicRadarBuggedTrack().SetVisible(true);
    radar.DynamicRadarSttTrack().SetVisible(true);

    // No sensor filtering belongs here: the radar-published state selects one
    // authored symbol family, while active is forwarded by the projection.
    for (std::size_t index = 0; index < kMaxRadarTracks; ++index)
    {
        const RadarTrackView& track = frame.tracks[index];
        const bool rwsVisible = track.visible && track.state == RadarTrackState::Search;
        const bool twsVisible = track.visible && track.state == RadarTrackState::Trackfile;
        const bool buggedVisible = track.visible && track.state == RadarTrackState::SystemTarget;
        const bool sttVisible = track.visible && track.state == RadarTrackState::SingleTargetTrack;

        if (rwsVisible && rwsTracks[index] == nullptr)
        {
            rwsTracks[index] = &radar.DynamicRadarRwsTrack().Create();
        }
        if (twsVisible && twsTracks[index] == nullptr)
        {
            twsTracks[index] = &radar.DynamicRadarTwsTrack().Create();
        }
        if (buggedVisible && buggedTracks[index] == nullptr)
        {
            buggedTracks[index] = &radar.DynamicRadarBuggedTrack().Create();
        }
        if (sttVisible && sttTracks[index] == nullptr)
        {
            sttTracks[index] = &radar.DynamicRadarSttTrack().Create();
        }

        if (rwsTracks[index] != nullptr)
        {
            ApplyTrackView(*rwsTracks[index], track, rwsVisible);
        }
        if (twsTracks[index] != nullptr)
        {
            ApplyTrackView(*twsTracks[index], track, twsVisible);
        }
        if (buggedTracks[index] != nullptr)
        {
            ApplyTrackView(*buggedTracks[index], track, buggedVisible);
        }
        if (sttTracks[index] != nullptr)
        {
            ApplyTrackView(*sttTracks[index], track, sttVisible);
        }
    }

    radar.radarDatablock.SetVisible(frame.radarPresentationVisible);
    const int buggedIndex = frame.designatedTrackIndex;
    if (frame.buggedVisible)
    {
        if (buggedIndex >= 0 && buggedIndex < static_cast<int>(kMaxRadarTracks))
        {
            const RadarTrack& track = input.tracks[static_cast<std::size_t>(buggedIndex)];
            char buffer[24] {};
            std::snprintf(
                buffer,
                sizeof(buffer),
                "%d%c",
                std::abs(static_cast<int>(std::lround(track.aspectDeg / 10.0f))),
                track.aspectDeg < 0.0f ? 'L' : 'R');
            radar.radarDatablock.TgtAspectValue().SetText(buffer);
            std::snprintf(buffer, sizeof(buffer), "%+dK", static_cast<int>(std::lround(track.closureKts)));
            radar.radarDatablock.TgtClosureValue().SetText(buffer);
            radar.radarDatablock.TgtHdgValue().SetText(FormatHeading(track.headingDeg));
            radar.radarDatablock.TgtIdentValue().SetText("WAIT");
            std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::lround(track.speedKts)));
            radar.radarDatablock.TgtGsValue().SetText(buffer);
        }
    }
    else
    {
        radar.radarDatablock.TgtAspectValue().SetText(
            FormatInt(static_cast<float>(input.radar.scanBars)) + "B");
        radar.radarDatablock.TgtHdgValue().SetText(FormatInt(input.radar.azScanDeg));
        radar.radarDatablock.TgtIdentValue().SetText("WAIT");
        radar.radarDatablock.TgtGsValue().SetText(FormatInt(input.ownship.speedKts));
        radar.radarDatablock.TgtClosureValue().SetText("");
    }

    radar.radarPerimeter.RangeTop().SetText(FormatInt(input.radar.rangeScaleNm));
    radar.radarScale.Scan().SetText("A");
    radar.radarScale.Bars().SetText("2");
    radar.radarScale.Iff().SetText("I");
    radar.radarScale.BarCount().SetText("3");
    radar.radarScale.Band().SetText("B");
    radar.radarScale.Prf().SetText(input.radar.highPrf ? "M+" : "M3");
    {
        char buffer[24] {};
        std::snprintf(buffer, sizeof(buffer), "%02d", static_cast<int>(std::lround(input.nav.bullseyeRangeNm)) % 100);
        radar.radarScale.BullRange().SetText(buffer);
        std::snprintf(buffer, sizeof(buffer), "%03d", static_cast<int>(std::lround(input.nav.bullseyeBearingDeg)) % 360);
        radar.radarScale.BullBearing().SetText(buffer);
    }

    radar.radarMessage.SetVisible(input.radar.operatingState != RadarOperatingState::Operating);
    radar.radarMessage.Message().SetText(
        input.radar.operatingState == RadarOperatingState::Off ? "FCR OFF" : "NO RAD");

    for (int osb = 1; osb <= static_cast<int>(kOsbCount); ++osb)
    {
        SetOsbLegend(radar.osbLegends, osb, "");
    }
}

void ApplySms(lhld_ui::LhldUi& ui, const MfdInputSample& input)
{
    auto& sms = ui.Sms();
    const StoresState& stores = input.stores;
    const int station = std::clamp(stores.selectedStation, 0, static_cast<int>(kStationCount));
    const bool selectedStationLoaded = IsStationLoaded(stores, station);
    const int loadedCount = CountLoadedStores(stores);

    ApplyStationLabels(sms.smsStations, stores);

    sms.smsHeader.MasterMode().SetText(MasterModeCaption(input.masterMode));
    sms.smsHeader.Weapon().SetText(
        selectedStationLoaded ? kLoadout[static_cast<std::size_t>(station - 1)] : "EMPTY");
    sms.smsHeader.Submode().SetText(loadedCount == 0 ? "JETT" : SmsSubmodeCaption(input.masterMode));
    sms.smsHeader.Arm().SetText(stores.masterArm ? "ARM" : "SIM");

    sms.smsSelectBox.SetVisible(selectedStationLoaded);
    if (selectedStationLoaded)
    {
        sms.smsSelectBox.SetPosition(ToVec(StationSelectPosition(station)));
    }

    if (loadedCount == 0)
    {
        sms.smsSelected.Param1().SetText("NO STORES");
        sms.smsSelected.Param2().SetText("");
        sms.smsSelected.Param3().SetText("");
        sms.smsSelected.Count().SetText("RDY 0");
        sms.smsSelected.Profile().SetText("JETT");
    }
    else
    {
        const int readyCount = selectedStationLoaded ? stores.readyCount : 0;
        sms.smsSelected.Param1().SetText("RP " + FormatInt(static_cast<float>(stores.rippleQuantity)));
        sms.smsSelected.Param2().SetText("PR " + FormatInt(static_cast<float>(stores.ripplePairs)));
        sms.smsSelected.Param3().SetText("TF " + FormatInt(static_cast<float>(stores.releaseSeconds)));
        sms.smsSelected.Count().SetText("RDY " + FormatInt(static_cast<float>(readyCount)));
        sms.smsSelected.Profile().SetText("PROF " + FormatInt(static_cast<float>(stores.profile)));
    }

    sms.SetStatusCaption(loadedCount == 0 ? "JETT" : "SMS");

    for (int osb = 1; osb <= static_cast<int>(kOsbCount); ++osb)
    {
        SetOsbLegend(sms.osbLegends, osb, MfdController::OsbLegend(MfdPage::Sms, osb, input));
    }
}

void ApplyNav(lhld_ui::LhldUi& ui, const MfdInputSample& input, const NavFrame& frame)
{
    auto& nav = ui.Nav();
    const MfdVec2 displayOffset = input.nav.centeredDisplay ? MfdVec2 {0.0f, 0.0f} : MfdVec2 {0.0f, -0.16f};
    const int waypointCount = std::clamp(input.nav.waypointCount, 1, static_cast<int>(kSteerpointCount));

    nav.navCompass.SetPosition(ToVec(displayOffset));
    nav.navCompass.SetRotationDegrees(frame.compassRotationDegrees);
    nav.navOwnship.SetPosition(ToVec(displayOffset));
    nav.navHeadingBox.HdgValue().SetText(FormatHeading(input.ownship.headingDeg));

    ApplyFlightPlanLeg(nav.flightLeg1, frame.flightPlanLegs[0], displayOffset);
    ApplyFlightPlanLeg(nav.flightLeg2, frame.flightPlanLegs[1], displayOffset);
    ApplyFlightPlanLeg(nav.flightLeg3, frame.flightPlanLegs[2], displayOffset);
    ApplyFlightPlanLeg(nav.flightLeg4, frame.flightPlanLegs[3], displayOffset);
    ApplyFlightPlanLeg(nav.flightLeg5, frame.flightPlanLegs[4], displayOffset);
    ApplyFlightPlanLeg(nav.flightLeg6, frame.flightPlanLegs[5], displayOffset);
    ApplyFlightPlanLeg(nav.flightLeg7, frame.flightPlanLegs[6], displayOffset);

    ApplySteerpoint(
        nav.stpt1, frame.steerpoints[0], input.nav.steerpoints[0].number, input.nav.currentSteerpoint == 1, displayOffset);
    ApplySteerpoint(
        nav.stpt2, frame.steerpoints[1], input.nav.steerpoints[1].number, input.nav.currentSteerpoint == 2, displayOffset);
    ApplySteerpoint(
        nav.stpt3, frame.steerpoints[2], input.nav.steerpoints[2].number, input.nav.currentSteerpoint == 3, displayOffset);
    ApplySteerpoint(
        nav.stpt4, frame.steerpoints[3], input.nav.steerpoints[3].number, input.nav.currentSteerpoint == 4, displayOffset);
    ApplySteerpoint(
        nav.stpt5, frame.steerpoints[4], input.nav.steerpoints[4].number, input.nav.currentSteerpoint == 5, displayOffset);
    ApplySteerpoint(
        nav.stpt6, frame.steerpoints[5], input.nav.steerpoints[5].number, input.nav.currentSteerpoint == 6, displayOffset);
    ApplySteerpoint(
        nav.stpt7, frame.steerpoints[6], input.nav.steerpoints[6].number, input.nav.currentSteerpoint == 7, displayOffset);
    ApplySteerpoint(
        nav.stpt8, frame.steerpoints[7], input.nav.steerpoints[7].number, input.nav.currentSteerpoint == 8, displayOffset);

    nav.navBullseye.SetVisible(frame.bullseye.visible);
    nav.navBullseye.SetPosition(ToVec(OffsetPosition(frame.bullseye.position, displayOffset)));
    nav.navBullseye.SetColor(kHsdCurrent);

    const int current = std::clamp(input.nav.currentSteerpoint, 1, waypointCount);
    const Steerpoint& active = input.nav.steerpoints[static_cast<std::size_t>(current - 1)];
    const float groundSpeed = std::max(1.0f, input.ownship.speedKts);
    const int etaSeconds = static_cast<int>(std::lround(active.rangeNm / groundSpeed * 3600.0f));

    nav.navData.Stpt().SetText("STPT " + FormatInt(static_cast<float>(active.number)));
    nav.navData.Brg().SetText("BRG " + FormatHeading(active.bearingDeg));
    {
        char buffer[24] {};
        std::snprintf(buffer, sizeof(buffer), "RNG %.1f", active.rangeNm);
        nav.navData.Rng().SetText(buffer);
        std::snprintf(buffer, sizeof(buffer), "ETA %02d:%02d", (etaSeconds / 60) % 100, etaSeconds % 60);
        nav.navData.Eta().SetText(buffer);
        std::snprintf(buffer, sizeof(buffer), "M %.2f", input.ownship.mach);
        nav.navData.Mach().SetText(buffer);
    }
    nav.navData.Gs().SetText("GS " + FormatInt(input.ownship.speedKts));

    nav.navStatus.Dep().SetText(input.nav.centeredDisplay ? "DEP" : "DCTR");
    nav.navStatus.Dcpl().SetText(input.nav.declutterActive ? "DCLT" : "DCPL");
    nav.navStatus.Cntl().SetText(input.nav.centeredDisplay ? "CNTR" : "CNTL");
    nav.SetStatusCaption(input.nav.declutterActive ? "HSD DCLT" : "HSD");

    for (int osb = 1; osb <= static_cast<int>(kOsbCount); ++osb)
    {
        SetOsbLegend(nav.osbLegends, osb, MfdController::OsbLegend(MfdPage::Nav, osb, input));
    }
}

void ApplyAg(lhld_ui::LhldUi& ui, const MfdInputSample& input)
{
    auto& ag = ui.Ag();
    const StoresState& stores = input.stores;
    const int station = std::clamp(stores.selectedStation, 0, static_cast<int>(kStationCount));
    const bool selectedAgStore =
        IsAirGroundStoreStation(station) && IsStationLoaded(stores, station);
    const int currentSteerpoint = std::clamp(input.nav.currentSteerpoint, 1, static_cast<int>(kSteerpointCount));
    const Steerpoint& target = input.nav.steerpoints[static_cast<std::size_t>(currentSteerpoint - 1)];

    ag.agHeader.MasterMode().SetText(MasterModeCaption(input.masterMode));
    ag.agHeader.Weapon().SetText(selectedAgStore ? kLoadout[static_cast<std::size_t>(station - 1)] : "NO WPN");
    ag.agHeader.WeaponFamily().SetText(selectedAgStore ? "MK-82" : "");
    ag.agHeader.WeaponWeight().SetText(selectedAgStore ? "500 LB" : "");
    ag.agHeader.Submode().SetText(DeliveryModeCaption(stores.deliveryMode));
    ag.agHeader.Arm().SetText(stores.masterArm ? "ARM" : "SIM");

    ag.agProfile.StationValue().SetText(
        selectedAgStore ? "[ " + FormatInt(static_cast<float>(station)) + " ]" : "[ -- ]");
    ag.agProfile.QtyValue().SetText("[ " + FormatInt(static_cast<float>(stores.rippleQuantity)) + " ]");
    ag.agProfile.ConfigValue().SetText(stores.ripplePairs == 2 ? "PAIR" : "SGL");
    ag.agProfile.ModeValue().SetText(DeliveryModeCaption(stores.deliveryMode));
    ag.agProfile.FuzingValue().SetText("INST");
    ag.agProfile.DispenseValue().SetText(stores.ripplePairs == 2 ? "PAIR" : "SGL");
    ag.agProfile.WeaponValue().SetText(FormatInt(static_cast<float>(CountReadyAirGroundStores(stores))));
    ag.agProfile.TimeValue().SetText(
        stores.releaseInProgress
            ? FormatFixed(stores.impactTimeRemainingSeconds, 1)
            : FormatInt(static_cast<float>(stores.releaseSeconds)));
    ag.agProfile.PairValue().SetText(stores.ripplePairs == 2 ? "PAIR" : "SGL");
    ag.agProfile.NoseTailValue().SetText("NOSE");
    ag.agProfile.RippleValue().SetText(stores.rippleQuantity > 1 ? "ON" : "OFF");
    ag.agProfile.IntervalValue().SetText(FormatFixed(stores.releaseIntervalSeconds, 2));
    ag.agProfile.ProfileValue().SetText("PROF " + FormatInt(static_cast<float>(stores.profile)));

    {
        char buffer[32] {};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "T%02d %03d/%02d",
            target.number,
            static_cast<int>(std::lround(target.bearingDeg)) % 360,
            static_cast<int>(std::lround(target.rangeNm)));
        ag.agProfile.TargetValue().SetText(buffer);
    }

    if (stores.releaseInProgress)
    {
        ag.agProfile.RangeState().SetText("REL");
        ag.SetStatusCaption("REL");
    }
    else if (selectedAgStore)
    {
        ag.agProfile.RangeState().SetText("IN RNG");
        ag.SetStatusCaption("RDY");
    }
    else
    {
        ag.agProfile.RangeState().SetText("NO REL");
        ag.SetStatusCaption("NO WPN");
    }

    for (int osb = 1; osb <= static_cast<int>(kOsbCount); ++osb)
    {
        SetOsbLegend(ag.osbLegends, osb, MfdController::OsbLegend(MfdPage::Ag, osb, input));
    }
}
} // namespace

void MfdController::Initialize(lhld_ui::LhldUi& ui)
{
    auto& radar = ui.Radar();
    rwsTracks_.fill(nullptr);
    twsTracks_.fill(nullptr);
    buggedTracks_.fill(nullptr);
    sttTracks_.fill(nullptr);

    radar.DynamicRadarRwsTrack().SetStrobeMagnetEnabled(false);
    radar.DynamicRadarTwsTrack().SetStrobeMagnetEnabled(false);
    radar.DynamicRadarBuggedTrack().SetStrobeMagnetEnabled(false);
    radar.DynamicRadarSttTrack().SetStrobeMagnetEnabled(false);
    radar.DynamicRadarRwsTrack().SetVisible(false);
    radar.DynamicRadarTwsTrack().SetVisible(false);
    radar.DynamicRadarBuggedTrack().SetVisible(false);
    radar.DynamicRadarSttTrack().SetVisible(false);
}

int MfdController::CapturedTrackIndex() const noexcept
{
    for (std::size_t index = 0; index < kMaxRadarTracks; ++index)
    {
        const bool rwsCaptured =
            rwsTracks_[index] != nullptr && rwsTracks_[index]->IsStrobeCaptured();
        const bool twsCaptured =
            twsTracks_[index] != nullptr && twsTracks_[index]->IsStrobeCaptured();
        if (rwsCaptured || twsCaptured)
        {
            return static_cast<int>(index);
        }
    }
    return -1;
}

void MfdController::Populate(lhld_ui::LhldUi& ui, const MfdInputSample& input)
{
    const MfdFrame frame = BuildMfdFrame(input);
    ApplyRadar(ui, input, frame.radar, rwsTracks_, twsTracks_, buggedTracks_, sttTracks_);
    ApplySms(ui, input);
    ApplyNav(ui, input, frame.nav);
    ApplyAg(ui, input);
}

std::string MfdController::OsbLegend(const MfdPage page, const int osbOneBased, const MfdInputSample& input)
{
    // The FCR authors its own Falcon-style bottom row. Other pages keep the
    // common page-select row provided by the shared OSB template.
    if (page != MfdPage::Radar)
    {
        switch (osbOneBased)
        {
        case 12: return "FCR";
        case 13: return "SMS";
        case 14: return "HSD";
        case 15: return "A-G";
        default: break;
        }
    }

    switch (page)
    {
    case MfdPage::Radar:
        switch (osbOneBased)
        {
        case 1:
            return "CRM";
        case 2:
            return RadarStatusCaption(input);
        case 3:
            return input.radar.fieldOfView == RadarFieldOfView::Expanded ? "EXP" : "NORM";
        case 4:
            return "OVRD";
        case 5:
            return "CNTL";
        default:
            return "";
        }
    case MfdPage::Sms:
        switch (osbOneBased)
        {
        case 1:
            return "A-A";
        case 2:
            return "A-G";
        case 3:
            return "NAV";
        case 6:
            return "STEP";
        case 7:
            return "PROF";
        default:
            return "";
        }
    case MfdPage::Nav:
        switch (osbOneBased)
        {
        case 1:
            return input.nav.centeredDisplay ? "CNTR" : "DCTR";
        case 2:
            return input.nav.declutterActive ? "DCLT*" : "DCLT";
        case 6:
            return FormatInt(input.nav.rangeScaleNm);
        case 7:
            return "STPT";
        case 8:
            return "ADD";
        default:
            return "";
        }
    case MfdPage::Ag:
        switch (osbOneBased)
        {
        case 1:
            return "CCIP";
        case 2:
            return "CCRP";
        case 6:
            return "STA";
        case 7:
            return "PROF";
        case 8:
            return input.stores.ripplePairs == 2 ? "PAIR" : "SGL";
        case 9:
            return "QTY";
        case 10:
            return "INT";
        case 11:
            return "REL";
        default:
            return "";
        }
    }

    return "";
}
} // namespace lhld
