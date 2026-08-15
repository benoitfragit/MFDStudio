/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the Dear ImGui LHLD demonstration application.
 */

#include "MfdApplication.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <utility>

#include <imgui.h>

#include "CockpitUi.h"
#include "MfdRadarSimulation.h"
#include "mfd/control/CommandClient.h"
#include "mfd/control/FeedbackTransport.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/PageName.h"

namespace lhld
{
namespace
{
constexpr float kBezelInset = 48.0f;
constexpr float kOsbButtonWidth = 46.0f;
constexpr float kOsbButtonHeight = 26.0f;
constexpr float kOsbGap = 5.0f;
constexpr int kMinRenderSize = 256;
constexpr int kMaxRenderSize = 1024;
constexpr ImU32 kPageSelectButtonTextColor = IM_COL32(245, 232, 72, 255);
constexpr ImU32 kPageSelectActiveTextColor = IM_COL32(8, 14, 8, 255);
constexpr ImU32 kBezelBodyColor = IM_COL32(16, 18, 19, 255);
constexpr ImU32 kBezelEdgeLightColor = IM_COL32(48, 52, 53, 255);
constexpr ImU32 kBezelEdgeShadowColor = IM_COL32(5, 6, 7, 255);
constexpr ImU32 kButtonShadowColor = IM_COL32(2, 3, 4, 170);

// Normalized MFD-space anchors for the 20 option-select buttons, matching the
// on-glass legend layout authored in osb_legends_tpl.json.
struct OsbAnchor
{
    float nx;
    float ny;
};

constexpr std::array<OsbAnchor, kOsbCount> kOsbAnchors = {{
    {-0.6f, 1.0f}, {-0.3f, 1.0f}, {0.0f, 1.0f}, {0.3f, 1.0f}, {0.6f, 1.0f},
    {1.0f, 0.6f}, {1.0f, 0.3f}, {1.0f, 0.0f}, {1.0f, -0.3f}, {1.0f, -0.6f},
    {0.6f, -1.0f}, {0.3f, -1.0f}, {0.0f, -1.0f}, {-0.3f, -1.0f}, {-0.6f, -1.0f},
    {-1.0f, -0.6f}, {-1.0f, -0.3f}, {-1.0f, 0.0f}, {-1.0f, 0.3f}, {-1.0f, 0.6f}}};

const char* PageName(const MfdPage page) noexcept
{
    switch (page)
    {
    case MfdPage::Radar:
        return "Radar";
    case MfdPage::Sms:
        return "Sms";
    case MfdPage::Nav:
        return "Nav";
    case MfdPage::Ag:
        return "Ag";
    }
    return "Radar";
}

// The ImGui-documented conversion from a backend texture handle to ImTextureID.
// Kept in one helper so the boundary cast stays localized.
ImTextureID ToImTextureId(void* handle) noexcept
{
    return (ImTextureID)(std::intptr_t)handle;
}

void ShowLastItemTooltip(const char* tooltip)
{
    if (tooltip != nullptr && tooltip[0] != '\0' && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tooltip);
    }
}

bool ModeButton(const char* label, const bool active, const ImVec2 size, const char* tooltip = nullptr)
{
    return cockpit::ModeButton(label, active, size, tooltip);
}

bool PanelButton(const char* label, const ImVec2 size, const char* tooltip)
{
    return cockpit::ActionButton(label, size, tooltip);
}

float TwoColumnButtonWidth() noexcept
{
    return std::max(72.0f, (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f);
}

float ThreeColumnButtonWidth() noexcept
{
    return std::max(64.0f, (ImGui::GetContentRegionAvail().x - 2.0f * ImGui::GetStyle().ItemSpacing.x) / 3.0f);
}

bool IsPageSelectOsb(const int osbOneBased) noexcept
{
    return osbOneBased == 12 || osbOneBased == 13 || osbOneBased == 14 || osbOneBased == 15;
}

bool IsActivePageSelectOsb(const int osbOneBased, const MfdPage activePage) noexcept
{
    return (osbOneBased == 12 && activePage == MfdPage::Radar) ||
        (osbOneBased == 13 && activePage == MfdPage::Sms) ||
        (osbOneBased == 14 && activePage == MfdPage::Nav) ||
        (osbOneBased == 15 && activePage == MfdPage::Ag);
}

const char* BezelButtonCaption(const int osbOneBased) noexcept
{
    switch (osbOneBased)
    {
    case 12:
        return "FCR";
    case 13:
        return "SMS";
    case 14:
        return "HSD";
    case 15:
        return "A-G";
    default:
        return "";
    }
}

const char* OsbActionTooltip(const MfdPage page, const int osbOneBased) noexcept
{
    switch (osbOneBased)
    {
    case 12:
        return "Select the FCR radar page.";
    case 13:
        return "Select the stores-management page.";
    case 14:
        return "Select the HSD navigation page.";
    case 15:
        return "Select the air-to-ground profile page.";
    default:
        break;
    }

    switch (page)
    {
    case MfdPage::Radar:
        switch (osbOneBased)
        {
        case 2:
            return "Toggle range-while-search and track-while-scan radar modes.";
        case 3:
            return "Toggle the Falcon 4:1 EXP display about the acquisition cursor.";
        case 17:
            return "Cycle the antenna bar scan; TWS selects a valid coupled scan pattern.";
        case 18:
            return "Cycle the mode-supported antenna azimuth scan width.";
        case 19:
            return "Decrease the FCR range scale.";
        case 20:
            return "Increase the FCR range scale.";
        default:
            return "No action assigned to this radar OSB.";
        }
    case MfdPage::Sms:
        switch (osbOneBased)
        {
        case 1:
            return "Select air-to-air master mode.";
        case 2:
            return "Select air-to-ground master mode.";
        case 3:
            return "Select navigation master mode.";
        case 6:
            return "Step the selected stores station.";
        case 7:
            return "Cycle the active stores profile.";
        default:
            return "No action assigned to this stores OSB.";
        }
    case MfdPage::Nav:
        switch (osbOneBased)
        {
        case 1:
            return "Toggle centered/decentered HSD presentation.";
        case 2:
            return "Toggle HSD declutter for route legs and bullseye.";
        case 6:
            return "Cycle HSD range scale.";
        case 7:
            return "Step the selected steerpoint.";
        case 8:
            return "Insert the draft waypoint after the selected steerpoint.";
        default:
            return "No action assigned to this HSD OSB.";
        }
    case MfdPage::Ag:
        switch (osbOneBased)
        {
        case 1:
            return "Select CCIP delivery presentation.";
        case 2:
            return "Select CCRP delivery presentation.";
        case 6:
            return "Step to the next loaded A-G release station.";
        case 7:
            return "Cycle the A-G delivery profile.";
        case 8:
            return "Toggle single/pair release.";
        case 9:
            return "Cycle release quantity.";
        case 10:
            return "Cycle release interval.";
        case 11:
            return "Run a simulated release, or release one store when MASTER ARM is set.";
        default:
            return "No action assigned to this A-G OSB.";
        }
    }

    return "No action assigned to this OSB.";
}

const char* DeliveryModeLabel(const AirGroundDeliveryMode mode) noexcept
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

float ClampFinite(const float value, const float fallback, const float low, const float high) noexcept
{
    const float finiteValue = std::isfinite(value) ? value : fallback;
    return std::clamp(finiteValue, low, high);
}

int ClampWaypointCount(const int count) noexcept
{
    return std::clamp(count, 1, static_cast<int>(kSteerpointCount));
}

int ClampCurrentSteerpoint(const NavState& nav) noexcept
{
    return std::clamp(nav.currentSteerpoint, 1, ClampWaypointCount(nav.waypointCount));
}

void RenumberWaypoints(NavState& nav) noexcept
{
    for (std::size_t index = 0; index < kSteerpointCount; ++index)
    {
        nav.steerpoints[index].number = static_cast<int>(index) + 1;
    }
}

void WriteWaypointFromDraft(NavState& nav,
                            const int waypointOneBased,
                            const float bearingDeg,
                            const float rangeNm) noexcept
{
    const int clampedWaypoint = std::clamp(waypointOneBased, 1, static_cast<int>(kSteerpointCount));
    nav.waypointCount = std::max(ClampWaypointCount(nav.waypointCount), clampedWaypoint);
    Steerpoint& steerpoint = nav.steerpoints[static_cast<std::size_t>(clampedWaypoint - 1)];
    steerpoint.bearingDeg = ClampFinite(bearingDeg, 0.0f, 0.0f, 359.0f);
    steerpoint.rangeNm = ClampFinite(rangeNm, 10.0f, 1.0f, 160.0f);
    nav.currentSteerpoint = clampedWaypoint;
    RenumberWaypoints(nav);
}

void InsertWaypointFromDraft(NavState& nav, const float bearingDeg, const float rangeNm) noexcept
{
    const int activeCount = ClampWaypointCount(nav.waypointCount);
    const int current = std::clamp(nav.currentSteerpoint, 1, activeCount);
    if (current >= static_cast<int>(kSteerpointCount))
    {
        nav.currentSteerpoint = current;
        nav.waypointCount = activeCount;
        return;
    }

    const int insertWaypoint = current + 1;
    const int newCount = std::min(activeCount + 1, static_cast<int>(kSteerpointCount));
    const std::size_t insertIndex = static_cast<std::size_t>(insertWaypoint - 1);
    for (std::size_t index = static_cast<std::size_t>(newCount - 1); index > insertIndex; --index)
    {
        nav.steerpoints[index] = nav.steerpoints[index - 1U];
    }

    nav.waypointCount = newCount;
    WriteWaypointFromDraft(nav, insertWaypoint, bearingDeg, rangeNm);
}

int NextLoadedAgStation(const StoresState& stores, const int currentStation) noexcept
{
    for (int offset = 1; offset <= static_cast<int>(kStationCount); ++offset)
    {
        const int station = ((std::max(currentStation, 1) - 1 + offset) % static_cast<int>(kStationCount)) + 1;
        if (IsAirGroundStoreStation(station) && IsStationLoaded(stores, station))
        {
            return station;
        }
    }
    return 0;
}

void DrawCenteredButtonCaption(const char* caption, const bool active)
{
    if (caption[0] == '\0')
    {
        return;
    }

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImVec2 textSize = ImGui::CalcTextSize(caption);
    const ImVec2 textPosition {
        min.x + (max.x - min.x - textSize.x) * 0.5f,
        min.y + (max.y - min.y - textSize.y) * 0.5f};
    ImGui::GetWindowDrawList()->AddText(
        textPosition,
        active ? kPageSelectActiveTextColor : kPageSelectButtonTextColor,
        caption);
}

void DrawBezelScrew(ImDrawList& drawList, const ImVec2 center, const float radius)
{
    drawList.AddCircleFilled(center, radius, IM_COL32(8, 9, 10, 255), 24);
    drawList.AddCircle(center, radius, IM_COL32(58, 62, 62, 255), 24, 1.0f);
    drawList.AddLine(
        ImVec2(center.x - radius * 0.55f, center.y),
        ImVec2(center.x + radius * 0.55f, center.y),
        IM_COL32(38, 42, 42, 255),
        1.5f);
}

void DrawBezelHardware(ImDrawList& drawList,
                       const ImVec2 origin,
                       const float unitSide,
                       const ImVec2 imageTopLeft,
                       const float imageSide)
{
    const ImVec2 bezelMax {origin.x + unitSide, origin.y + unitSide};
    drawList.AddRectFilled(origin, bezelMax, kBezelBodyColor, 8.0f);
    drawList.AddRect(origin, bezelMax, kBezelEdgeLightColor, 8.0f, 0, 2.0f);
    drawList.AddRect(
        ImVec2(origin.x + 3.0f, origin.y + 3.0f),
        ImVec2(bezelMax.x - 3.0f, bezelMax.y - 3.0f),
        kBezelEdgeShadowColor,
        6.0f,
        0,
        2.0f);

    const ImVec2 imageMax {imageTopLeft.x + imageSide, imageTopLeft.y + imageSide};
    drawList.AddRectFilled(
        ImVec2(imageTopLeft.x - 8.0f, imageTopLeft.y - 8.0f),
        ImVec2(imageMax.x + 8.0f, imageMax.y + 8.0f),
        IM_COL32(4, 5, 5, 255),
        18.0f);
    drawList.AddRect(
        ImVec2(imageTopLeft.x - 6.0f, imageTopLeft.y - 6.0f),
        ImVec2(imageMax.x + 6.0f, imageMax.y + 6.0f),
        IM_COL32(44, 48, 48, 255),
        16.0f,
        0,
        2.0f);

    const float screwInset = 25.0f;
    const float screwRadius = std::max(4.5f, unitSide * 0.014f);
    DrawBezelScrew(drawList, ImVec2(origin.x + screwInset, origin.y + screwInset), screwRadius);
    DrawBezelScrew(drawList, ImVec2(bezelMax.x - screwInset, origin.y + screwInset), screwRadius);
    DrawBezelScrew(drawList, ImVec2(origin.x + screwInset, bezelMax.y - screwInset), screwRadius);
    DrawBezelScrew(drawList, ImVec2(bezelMax.x - screwInset, bezelMax.y - screwInset), screwRadius);

    drawList.AddCircleFilled(ImVec2(origin.x + unitSide * 0.20f, origin.y + 24.0f), 8.0f, IM_COL32(24, 83, 19, 255), 20);
    drawList.AddCircleFilled(ImVec2(origin.x + unitSide * 0.80f, origin.y + 24.0f), 8.0f, IM_COL32(24, 83, 19, 255), 20);
}

std::unique_ptr<MfdInputSource> CreateDefaultInputSource()
{
    return std::make_unique<MfdRadarSimulation>();
}
} // namespace

MfdApplication::MfdApplication()
    : MfdApplication(CreateDefaultInputSource())
{
}

MfdApplication::MfdApplication(std::unique_ptr<MfdInputSource> inputSource)
    : inputSource_(inputSource == nullptr ? CreateDefaultInputSource() : std::move(inputSource))
{
    inputs_ = inputSource_->Inputs();
    radarControls_ = inputs_.radar;
    masterMode_ = inputs_.masterMode;
    stores_ = inputs_.stores;
    navControls_ = inputs_.nav;
}

bool MfdApplication::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, std::string& error)
{
    MfdConfig loadedConfig;
    if (!LoadConfig(loadedConfig, error))
    {
        return false;
    }

    config_ = loadedConfig;

    // The offscreen runtime must load and bind its UDP command receiver before
    // Connect() publishes the startup batch. Otherwise SendStartup() would reach
    // an unbound loopback port and the initial scene state would be dropped.
    if (!offscreenView_.Initialize(config_.windowFile, error))
    {
        return false;
    }
    uploader_.Initialize(device, context);
    offscreenView_.SetActivePage(PageName(activePage_));

    if (!Connect(config_, error))
    {
        return false;
    }

    SetStatus("LHLD demo connected.", false);
    return true;
}

bool MfdApplication::LoadConfig(MfdConfig& config, std::string& error) const
{
    try
    {
        mfd::JsonLoader loader;
        const std::string windowFile {lhld_ui::LhldUi::WindowFile()};
        const mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(windowFile);

        const mfd::PageDefinition* radarPage =
            mfd::FindPageDefinition(loaded.document, lhld_ui::RadarMockupPage::Name());
        if (radarPage == nullptr)
        {
            error = "The LHLD asset does not expose a page named 'Radar'.";
            return false;
        }

        if (!loaded.window.commandTransports.udp.has_value() || !loaded.window.commandTransports.udp->enabled)
        {
            error = "The LHLD window JSON has no enabled UDP command transport.";
            return false;
        }

        if (!loaded.generatedTransportMap.has_value())
        {
            error = "The LHLD generated transport map is missing.";
            return false;
        }

        config.commandTransport = *loaded.window.commandTransports.udp;
        config.feedbackTransport = loaded.window.feedbackTransports;
        config.generatedTransportMap = *loaded.generatedTransportMap;
        config.pageView = radarPage->view;
        config.windowFile = windowFile;
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

bool MfdApplication::Connect(const MfdConfig& config, std::string& error)
{
    startupClient_ = std::make_unique<mfd::CommandClient>(config.commandTransport, config.generatedTransportMap);
    if (startupClient_ == nullptr || !startupClient_->IsReady())
    {
        error = startupClient_ == nullptr ? "Unable to create the MFD startup client." : startupClient_->LastError();
        connected_ = false;
        return false;
    }

    publisher_ = std::make_unique<mfd::client::LatestBatchPublisher>(config.commandTransport);
    if (publisher_ == nullptr || !publisher_->IsReady())
    {
        error = publisher_ == nullptr ? "Unable to create the MFD realtime publisher." : publisher_->LastError();
        connected_ = false;
        return false;
    }

    feedbackReceiver_ = mfd::CreateFeedbackReceiverChannel(config.feedbackTransport);
    livenessMonitor_.Reset();
    ui_.Initialize();
    controller_.Initialize(ui_);
    if (!ui_.SendStartup(*startupClient_, config.pageView, "LHLD DEMO | READY"))
    {
        error = startupClient_->LastError();
        connected_ = false;
        return false;
    }

    connected_ = true;
    return true;
}

void MfdApplication::ApplyControlsToInputSource() noexcept
{
    inputSource_->ApplyRadarControls(radarControls_);
    inputSource_->SetMasterMode(masterMode_);
    inputSource_->SetStores(stores_);
    inputSource_->SetNavState(navControls_);
}

void MfdApplication::SelectRadarSearchSubmode(const RadarSubmode submode) noexcept
{
    if (submode != RadarSubmode::Rws && submode != RadarSubmode::Tws)
    {
        return;
    }
    radarReturnSubmode_ = submode;
    radarControls_.submode = submode;
}

void MfdApplication::CycleRadarAzimuthScan() noexcept
{
    radarControls_.selectedAzimuthScan = NextRadarAzimuthScan(radarControls_.selectedAzimuthScan);
}

void MfdApplication::CycleRadarBarScan() noexcept
{
    if (radarControls_.submode == RadarSubmode::Tws)
    {
        CycleRadarAzimuthScan();
        return;
    }
    radarControls_.selectedScanBars = NextRadarBarCount(radarControls_.selectedScanBars);
}

void MfdApplication::StepRadarRange(const RadarRangeStep direction) noexcept
{
    radarControls_.rangeScaleNm = StepRadarRangeScale(radarControls_.rangeScaleNm, direction);
}

void MfdApplication::ToggleRadarFieldOfView()
{
    if (radarControls_.submode == RadarSubmode::Stt)
    {
        radarControls_.fieldOfView = RadarFieldOfView::Normal;
        SetStatus("EXP is unavailable in STT; the FCR remains in NORM.", true);
        return;
    }

    radarControls_.fieldOfView = radarControls_.fieldOfView == RadarFieldOfView::Normal
        ? RadarFieldOfView::Expanded
        : RadarFieldOfView::Normal;
    SetStatus(
        radarControls_.fieldOfView == RadarFieldOfView::Expanded
            ? "FCR 4:1 expanded display selected."
            : "FCR normal display selected.",
        false);
}

void MfdApplication::BugCapturedRadarTrack()
{
    if (radarControls_.operatingState != RadarOperatingState::Operating)
    {
        SetStatus("The FCR must be operating before a track can be bugged.", true);
        return;
    }

    const int capturedTrack = controller_.CapturedTrackIndex();
    if (capturedTrack < 0)
    {
        SetStatus("No radar track is captured by the acquisition cursor.", true);
        return;
    }

    if (radarControls_.submode == RadarSubmode::Stt)
    {
        radarControls_.submode = radarReturnSubmode_;
    }
    radarControls_.buggedTrack = capturedTrack;
    SetStatus("Radar track bugged from runtime strobe capture.", false);
}

void MfdApplication::EnterSingleTargetTrack()
{
    if (radarControls_.buggedTrack < 0)
    {
        SetStatus("Bug a captured radar track before selecting STT.", true);
        return;
    }
    if (radarControls_.operatingState != RadarOperatingState::Operating)
    {
        SetStatus("The FCR must be operating before entering STT.", true);
        return;
    }

    if (radarControls_.submode != RadarSubmode::Stt)
    {
        radarReturnSubmode_ = radarControls_.submode;
    }
    radarControls_.submode = RadarSubmode::Stt;
    radarControls_.fieldOfView = RadarFieldOfView::Normal;
    SetStatus("Single-target track selected.", false);
}

void MfdApplication::ClearRadarBug() noexcept
{
    radarControls_.buggedTrack = -1;
    radarControls_.submode = radarReturnSubmode_;
}

void MfdApplication::SetRadarPowerEnabled(const bool enabled) noexcept
{
    if (!enabled)
    {
        radarControls_.operatingState = RadarOperatingState::Off;
        return;
    }

    if (radarControls_.operatingState == RadarOperatingState::Off)
    {
        radarControls_.operatingState = RadarOperatingState::Silent;
    }
}

void MfdApplication::SetRadarRfEnabled(const bool enabled) noexcept
{
    if (radarControls_.operatingState == RadarOperatingState::Off)
    {
        return;
    }
    radarControls_.operatingState = enabled ? RadarOperatingState::Operating : RadarOperatingState::Silent;
}

void MfdApplication::DrawFrame(const float deltaSeconds)
{
    elapsedSeconds_ += std::max(deltaSeconds, 0.0f);

    if (running_)
    {
        UpdateAgReleaseTimer(deltaSeconds);
    }
    ApplyControlsToInputSource();
    if (running_)
    {
        inputSource_->Step(deltaSeconds);
    }
    inputs_ = inputSource_->Inputs();
    radarControls_ = inputs_.radar;
    inputs_.activePage = activePage_;

    HandleKeyboardShortcuts();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    cockpit::PushTheme();
    ImGui::Begin(
        "LHLD",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    cockpit::DrawBackplate();
    ImGui::SetWindowFontScale(1.08f);
    ImGui::BeginChild(
        "##annunciator_rail",
        ImVec2(0.0f, 52.0f),
        ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    DrawPageSelectorBar();
    ImGui::EndChild();

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const cockpit::ConsoleLayout layout = cockpit::ResolveConsoleLayout(available);
    if (layout.sideBySide)
    {
        ImGui::BeginChild(
            "##mfd_bay",
            ImVec2(layout.mfdWidth, 0.0f),
            ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawMfdUnit(deltaSeconds);
        ImGui::EndChild();
        ImGui::SameLine(0.0f, layout.gap);
        DrawControlPanel();
    }
    else
    {
        const float reservedControlHeight = std::clamp(available.y * 0.40f, 250.0f, 350.0f);
        const float mfdRegionHeight = std::max(320.0f, available.y - reservedControlHeight - 8.0f);
        ImGui::BeginChild(
            "##mfd_bay",
            ImVec2(0.0f, mfdRegionHeight),
            ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawMfdUnit(deltaSeconds);
        ImGui::EndChild();
        DrawControlPanel();
    }

    ImGui::End();
    cockpit::PopTheme();

    ApplyControlsToInputSource();
    inputs_ = inputSource_->Inputs();
    radarControls_ = inputs_.radar;
    inputs_.activePage = activePage_;
    PublishFrame();

    PollFeedback();
}

void MfdApplication::DrawMfdUnit(const float deltaSeconds)
{
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float unitSide = std::clamp(std::min(available.x, available.y), 320.0f, 1100.0f);
    const float horizontalOffset = std::max(0.0f, (available.x - unitSide) * 0.5f);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + horizontalOffset);
    ImGui::BeginChild("##mfd_unit", ImVec2(unitSide, unitSide), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float imageSide = std::max(64.0f, unitSide - 2.0f * kBezelInset);
    const int renderSize = std::clamp(static_cast<int>(imageSide), kMinRenderSize, kMaxRenderSize);

    // Advance and render the hosted runtime, then upload its frame to DX11.
    offscreenView_.Update(deltaSeconds, renderSize, renderSize);
    const OffscreenPixels frame = offscreenView_.Pixels();
    if (frame.ready && !uploader_.Upload(frame.pixels, frame.width, frame.height, frame.rowStrideBytes))
    {
        // Surface the failure instead of silently keeping a stale/black texture.
        SetStatus("MFD offscreen texture upload failed.", true);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 imageTopLeft {origin.x + kBezelInset, origin.y + kBezelInset};
    DrawBezelHardware(*drawList, origin, unitSide, imageTopLeft, imageSide);
    ImGui::SetCursorScreenPos(imageTopLeft);
    if (uploader_.TextureId() != nullptr)
    {
        ImGui::Image(ToImTextureId(uploader_.TextureId()), ImVec2(imageSide, imageSide));

        // Acquisition-cursor slew: clicking on the FCR screen moves the cursor.
        if (activePage_ == MfdPage::Radar && ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const ImVec2 mouse = ImGui::GetMousePos();
            const float fracX = std::clamp((mouse.x - imageTopLeft.x) / imageSide, 0.0f, 1.0f);
            const float fracY = std::clamp((mouse.y - imageTopLeft.y) / imageSide, 0.0f, 1.0f);
            radarControls_.cursorPosition.x = 2.0f * fracX - 1.0f;
            radarControls_.cursorPosition.y = 1.0f - 2.0f * fracY;
        }
    }
    else
    {
        drawList->AddRectFilled(
            imageTopLeft,
            ImVec2(imageTopLeft.x + imageSide, imageTopLeft.y + imageSide),
            IM_COL32(4, 8, 6, 255));
    }

    DrawBezelButtons(origin.x, origin.y, unitSide, kBezelInset);
    ImGui::EndChild();
}

void MfdApplication::DrawBezelButtons(const float unitOriginX,
                                      const float unitOriginY,
                                      const float unitSide,
                                      const float imageInset)
{
    const float imageSide = std::max(64.0f, unitSide - 2.0f * imageInset);
    const float centerX = unitOriginX + imageInset + imageSide * 0.5f;
    const float centerY = unitOriginY + imageInset + imageSide * 0.5f;
    const float half = imageSide * 0.5f;

    for (std::size_t index = 0; index < kOsbCount; ++index)
    {
        const OsbAnchor anchor = kOsbAnchors[index];
        float screenX = centerX + anchor.nx * half;
        float screenY = centerY - anchor.ny * half;
        if (std::fabs(anchor.ny) >= std::fabs(anchor.nx))
        {
            screenY += (anchor.ny > 0.0f ? -1.0f : 1.0f) * (kOsbButtonHeight * 0.5f + kOsbGap);
        }
        else
        {
            screenX += (anchor.nx > 0.0f ? 1.0f : -1.0f) * (kOsbButtonWidth * 0.5f + kOsbGap);
        }

        const ImVec2 buttonMin {screenX - kOsbButtonWidth * 0.5f, screenY - kOsbButtonHeight * 0.5f};
        const ImVec2 buttonMax {buttonMin.x + kOsbButtonWidth, buttonMin.y + kOsbButtonHeight};
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            ImVec2(buttonMin.x + 2.0f, buttonMin.y + 3.0f),
            ImVec2(buttonMax.x + 2.0f, buttonMax.y + 3.0f),
            kButtonShadowColor,
            4.0f);

        ImGui::SetCursorScreenPos(buttonMin);
        char id[16] {};
        std::snprintf(id, sizeof(id), "##osb%02zu", index + 1);
        const int osbOneBased = static_cast<int>(index) + 1;
        const bool pageSelectButton = IsPageSelectOsb(osbOneBased);
        const bool activePageButton = IsActivePageSelectOsb(osbOneBased, activePage_);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        if (pageSelectButton)
        {
            ImGui::PushStyleColor(
                ImGuiCol_Button,
                activePageButton ? ImVec4(0.85f, 0.78f, 0.18f, 1.0f) : ImVec4(0.18f, 0.20f, 0.14f, 1.0f));
            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered,
                activePageButton ? ImVec4(0.96f, 0.86f, 0.24f, 1.0f) : ImVec4(0.30f, 0.31f, 0.18f, 1.0f));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.30f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.38f, 0.40f, 0.39f, 1.0f));
        }
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.19f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.58f, 0.61f, 0.59f, 1.0f));
        if (ImGui::Button(id, ImVec2(kOsbButtonWidth, kOsbButtonHeight)))
        {
            HandleOsbPress(osbOneBased);
        }
        DrawCenteredButtonCaption(BezelButtonCaption(osbOneBased), activePageButton);
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
        if (ImGui::IsItemHovered())
        {
            const std::string legend = MfdController::OsbLegend(activePage_, osbOneBased, inputs_);
            ImGui::SetTooltip(
                "OSB %zu  %s\n%s",
                index + 1,
                legend.empty() ? "-" : legend.c_str(),
                OsbActionTooltip(activePage_, osbOneBased));
        }
    }
}

void MfdApplication::HandleOsbPress(const int osbOneBased)
{
    switch (osbOneBased)
    {
    case 12:
        SelectPage(MfdPage::Radar);
        return;
    case 13:
        SelectPage(MfdPage::Sms);
        return;
    case 14:
        SelectPage(MfdPage::Nav);
        return;
    case 15:
        SelectPage(MfdPage::Ag);
        return;
    default:
        break;
    }

    switch (activePage_)
    {
    case MfdPage::Radar:
        switch (osbOneBased)
        {
        case 2:
            SelectRadarSearchSubmode(
                radarReturnSubmode_ == RadarSubmode::Rws ? RadarSubmode::Tws : RadarSubmode::Rws);
            break;
        case 3:
            ToggleRadarFieldOfView();
            break;
        case 17:
            CycleRadarBarScan();
            break;
        case 18:
            CycleRadarAzimuthScan();
            break;
        case 19:
            StepRadarRange(RadarRangeStep::Decrease);
            break;
        case 20:
            StepRadarRange(RadarRangeStep::Increase);
            break;
        default:
            break;
        }
        break;
    case MfdPage::Sms:
        switch (osbOneBased)
        {
        case 1:
            masterMode_ = MasterMode::AirToAir;
            break;
        case 2:
            masterMode_ = MasterMode::AirToGround;
            break;
        case 3:
            masterMode_ = MasterMode::Nav;
            break;
        case 6:
            StepSmsStation();
            break;
        case 7:
            stores_.profile = stores_.profile >= 3 ? 1 : stores_.profile + 1;
            break;
        default:
            break;
        }
        break;
    case MfdPage::Nav:
        switch (osbOneBased)
        {
        case 1:
            ToggleHsdCenteredDisplay();
            break;
        case 2:
            ToggleHsdDeclutter();
            break;
        case 6:
            CycleNavRange();
            break;
        case 7:
            StepNavSteerpoint();
            break;
        case 8:
            AddWaypointFromDraft();
            break;
        default:
            break;
        }
        break;
    case MfdPage::Ag:
        switch (osbOneBased)
        {
        case 1:
            SelectAgDeliveryMode(AirGroundDeliveryMode::Ccip);
            break;
        case 2:
            SelectAgDeliveryMode(AirGroundDeliveryMode::Ccrp);
            break;
        case 6:
            StepAgStation();
            break;
        case 7:
            CycleAgProfile();
            break;
        case 8:
            ToggleAgPairs();
            break;
        case 9:
            CycleAgQuantity();
            break;
        case 10:
            CycleReleaseInterval();
            break;
        case 11:
            ReleaseSelectedAgStore();
            break;
        default:
            break;
        }
        break;
    }
}

void MfdApplication::SelectPage(const MfdPage page)
{
    activePage_ = page;
    inputs_.activePage = page;
    offscreenView_.SetActivePage(PageName(page));
}

void MfdApplication::StepSmsStation() noexcept
{
    if (stores_.selectedStation < 1 || stores_.selectedStation >= static_cast<int>(kStationCount))
    {
        stores_.selectedStation = 1;
        return;
    }
    ++stores_.selectedStation;
}

void MfdApplication::StepAgStation() noexcept
{
    const int nextStation = NextLoadedAgStation(stores_, stores_.selectedStation);
    stores_.selectedStation = nextStation;
    stores_.readyCount = CountReadyAirGroundStores(stores_);
}

void MfdApplication::StepNavSteerpoint() noexcept
{
    const int waypointCount = ClampWaypointCount(navControls_.waypointCount);
    const int current = ClampCurrentSteerpoint(navControls_);
    navControls_.currentSteerpoint = current >= waypointCount ? 1 : current + 1;
}

void MfdApplication::CycleNavRange() noexcept
{
    navControls_.rangeScaleNm = navControls_.rangeScaleNm >= 160.0f ? 40.0f : navControls_.rangeScaleNm * 2.0f;
}

void MfdApplication::ToggleHsdCenteredDisplay() noexcept
{
    navControls_.centeredDisplay = !navControls_.centeredDisplay;
}

void MfdApplication::ToggleHsdDeclutter() noexcept
{
    navControls_.declutterActive = !navControls_.declutterActive;
}

void MfdApplication::AddWaypointFromDraft() noexcept
{
    InsertWaypointFromDraft(navControls_, waypointDraftBearingDeg_, waypointDraftRangeNm_);
}

void MfdApplication::OverwriteCurrentWaypointFromDraft() noexcept
{
    WriteWaypointFromDraft(
        navControls_,
        navControls_.currentSteerpoint,
        waypointDraftBearingDeg_,
        waypointDraftRangeNm_);
}

void MfdApplication::CycleAgProfile() noexcept
{
    stores_.profile = stores_.profile >= 4 ? 1 : stores_.profile + 1;
}

void MfdApplication::CycleAgQuantity() noexcept
{
    stores_.rippleQuantity = stores_.rippleQuantity >= 4 ? 1 : stores_.rippleQuantity + 1;
    stores_.readyCount = CountReadyAirGroundStores(stores_);
}

void MfdApplication::CycleReleaseInterval() noexcept
{
    if (stores_.releaseIntervalSeconds < 0.50f)
    {
        stores_.releaseIntervalSeconds = 0.50f;
    }
    else if (stores_.releaseIntervalSeconds < 0.75f)
    {
        stores_.releaseIntervalSeconds = 0.75f;
    }
    else if (stores_.releaseIntervalSeconds < 1.00f)
    {
        stores_.releaseIntervalSeconds = 1.00f;
    }
    else
    {
        stores_.releaseIntervalSeconds = 0.25f;
    }
}

void MfdApplication::ToggleAgPairs() noexcept
{
    stores_.ripplePairs = stores_.ripplePairs == 1 ? 2 : 1;
}

void MfdApplication::SelectAgDeliveryMode(const AirGroundDeliveryMode mode) noexcept
{
    stores_.deliveryMode = mode;
    masterMode_ = MasterMode::AirToGround;
}

void MfdApplication::ReleaseSelectedAgStore()
{
    const int station = stores_.selectedStation;
    if (!stores_.masterArm && !stores_.simulateMode)
    {
        SetStatus("A-G release inhibited: ARM or SIM required.", true);
        return;
    }

    if (!IsAirGroundStoreStation(station) || !IsStationLoaded(stores_, station))
    {
        SetStatus("A-G release inhibited: no bomb on selected station.", true);
        return;
    }

    stores_.releaseInProgress = true;
    stores_.releasedStation = station;
    stores_.impactTimeRemainingSeconds = static_cast<float>(std::max(stores_.releaseSeconds, 1));
    if (stores_.masterArm)
    {
        stores_.stationLoaded[static_cast<std::size_t>(station - 1)] = false;
        stores_.selectedStation = NextLoadedAgStation(stores_, station);
    }
    stores_.readyCount = CountReadyAirGroundStores(stores_);
    SetStatus(stores_.simulateMode && !stores_.masterArm ? "A-G SIM release accepted." : "A-G release accepted.", false);
}

void MfdApplication::UpdateAgReleaseTimer(const float deltaSeconds) noexcept
{
    if (!stores_.releaseInProgress || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
    {
        return;
    }

    stores_.impactTimeRemainingSeconds =
        std::max(0.0f, stores_.impactTimeRemainingSeconds - std::min(deltaSeconds, 0.25f));
    if (stores_.impactTimeRemainingSeconds <= 0.0f)
    {
        stores_.releaseInProgress = false;
        stores_.releasedStation = 0;
    }
}

void MfdApplication::JettisonStores()
{
    if (CountLoadedStores(stores_) == 0)
    {
        SetStatus("SMS stores already empty.", false);
        return;
    }

    JettisonAllStores(stores_);
    SetStatus("SMS jettison complete.", false);
}

void MfdApplication::HandleKeyboardShortcuts()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
    {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_1))
    {
        SelectPage(MfdPage::Radar);
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_2))
    {
        SelectPage(MfdPage::Sms);
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_3))
    {
        SelectPage(MfdPage::Nav);
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_4))
    {
        SelectPage(MfdPage::Ag);
    }
}

void MfdApplication::DrawPageSelectorBar()
{
    if (ModeButton("RADAR", activePage_ == MfdPage::Radar, ImVec2(90.0f, 30.0f), "Show the FCR radar page."))
    {
        SelectPage(MfdPage::Radar);
    }
    ImGui::SameLine();
    if (ModeButton("SMS", activePage_ == MfdPage::Sms, ImVec2(90.0f, 30.0f), "Show the stores-management page."))
    {
        SelectPage(MfdPage::Sms);
    }
    ImGui::SameLine();
    if (ModeButton("NAV", activePage_ == MfdPage::Nav, ImVec2(90.0f, 30.0f), "Show the HSD navigation page."))
    {
        SelectPage(MfdPage::Nav);
    }
    ImGui::SameLine();
    if (ModeButton("A-G", activePage_ == MfdPage::Ag, ImVec2(90.0f, 30.0f), "Show the air-to-ground profile page."))
    {
        SelectPage(MfdPage::Ag);
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "%s", status_.empty() ? "Ready." : status_.c_str());
}

void MfdApplication::DrawControlPanel()
{
    ImGui::BeginChild(
        "##control_panel",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_Borders,
        ImGuiWindowFlags_AlwaysVerticalScrollbar);
    cockpit::DrawPanelHeader("AVIONICS CONTROL", offscreenView_.ActivePageName().c_str());

    cockpit::DrawStatusLamp("DATA", connected_);
    ImGui::SameLine();
    cockpit::DrawStatusLamp("SIM RUN", running_);
    ImGui::SameLine();
    cockpit::DrawStatusLamp("FAULT", statusIsError_, true);
    ImGui::TextWrapped("%s", status_.empty() ? "Ready." : status_.c_str());

    const float actionWidth = TwoColumnButtonWidth();
    if (PanelButton(running_ ? "PAUSE" : "RUN", ImVec2(actionWidth, 32.0f), "Pause or resume the mini-simulation."))
    {
        running_ = !running_;
    }
    ImGui::SameLine();
    if (PanelButton("RESET", ImVec2(actionWidth, 32.0f), "Reset radar, stores, navigation and generated UI state."))
    {
        ResetScene();
    }

    DrawMasterModeControls();
    switch (activePage_)
    {
    case MfdPage::Radar:
        DrawRadarControls();
        break;
    case MfdPage::Sms:
        DrawStoresControls();
        break;
    case MfdPage::Nav:
        DrawNavigationControls();
        break;
    case MfdPage::Ag:
        DrawAirGroundControls();
        break;
    }
    ImGui::EndChild();
}

void MfdApplication::DrawMasterModeControls()
{
    cockpit::DrawSectionHeader("MASTER MODE", "ICP");
    const float buttonWidth = TwoColumnButtonWidth();
    if (ModeButton("NAV", masterMode_ == MasterMode::Nav, ImVec2(buttonWidth, 32.0f), "Select navigation master mode."))
    {
        masterMode_ = MasterMode::Nav;
    }
    ImGui::SameLine();
    if (ModeButton("A-A", masterMode_ == MasterMode::AirToAir, ImVec2(buttonWidth, 32.0f), "Select air-to-air master mode."))
    {
        masterMode_ = MasterMode::AirToAir;
    }
    if (ModeButton("A-G", masterMode_ == MasterMode::AirToGround, ImVec2(buttonWidth, 32.0f), "Select air-to-ground master mode."))
    {
        masterMode_ = MasterMode::AirToGround;
    }
    ImGui::SameLine();
    if (ModeButton("DGFT", masterMode_ == MasterMode::Dogfight, ImVec2(buttonWidth, 32.0f), "Select dogfight override mode."))
    {
        masterMode_ = MasterMode::Dogfight;
    }
}

void MfdApplication::DrawRadarControls()
{
    cockpit::DrawSectionHeader("FIRE CONTROL RADAR", "AN/APG-68");
    const float buttonWidth = ThreeColumnButtonWidth();
    if (ModeButton("RWS", radarControls_.submode == RadarSubmode::Rws, ImVec2(buttonWidth, 30.0f), "Select range-while-search radar mode."))
    {
        SelectRadarSearchSubmode(RadarSubmode::Rws);
    }
    ImGui::SameLine();
    if (ModeButton("TWS", radarControls_.submode == RadarSubmode::Tws, ImVec2(buttonWidth, 30.0f), "Select track-while-scan radar mode."))
    {
        SelectRadarSearchSubmode(RadarSubmode::Tws);
    }
    ImGui::SameLine();
    if (ModeButton(
            "EXP",
            radarControls_.fieldOfView == RadarFieldOfView::Expanded,
            ImVec2(buttonWidth, 30.0f),
            "Toggle the documented 4:1 display expansion about the acquisition cursor."))
    {
        ToggleRadarFieldOfView();
    }

    if (ModeButton("BUG", radarControls_.buggedTrack >= 0, ImVec2(buttonWidth, 30.0f), "Bug the track captured by the acquisition cursor."))
    {
        BugCapturedRadarTrack();
    }
    ImGui::SameLine();
    if (ModeButton("STT", radarControls_.submode == RadarSubmode::Stt, ImVec2(buttonWidth, 30.0f), "Enter single-target track on the bugged track."))
    {
        EnterSingleTargetTrack();
    }
    ImGui::SameLine();
    if (PanelButton("UNBUG", ImVec2(buttonWidth, 30.0f), "Clear the bugged track and return to search."))
    {
        ClearRadarBug();
    }

    const bool fcrPowerEnabled = radarControls_.operatingState != RadarOperatingState::Off;
    const bool rfEnabled = radarControls_.operatingState == RadarOperatingState::Operating;
    if (cockpit::TwoPositionSwitch(
            "fcr_power",
            "FCR PWR",
            fcrPowerEnabled,
            true,
            "FCR power. OFF displays FCR OFF; ON powers the radar without forcing RF emission."))
    {
        SetRadarPowerEnabled(!fcrPowerEnabled);
    }
    ImGui::SameLine();
    if (cockpit::TwoPositionSwitch(
            "rf_emission",
            "RF",
            rfEnabled,
            fcrPowerEnabled,
            "RF emission. OFF while powered displays NO RAD; ON commands normal radar operation."))
    {
        SetRadarRfEnabled(!rfEnabled);
    }

    cockpit::VerticalThumbwheel(
        "antenna_elevation",
        "ANT ELEV",
        radarControls_.antennaElevationDeg,
        -60.0f,
        60.0f,
        0.35f,
        "%+.0f DEG",
        "Drag the wheel vertically, or use the mouse wheel, to move the radar elevation volume.");
    ImGui::SameLine();
    cockpit::SlewControl(
        "acquisition_cursor",
        "CURSOR SLEW",
        radarControls_.cursorPosition.x,
        radarControls_.cursorPosition.y,
        "Drag to slew the acquisition cursor. Double-click to recenter it.");
    ImGui::Text("RNG %.0f NM  |  A%s / %dB  |  IFF %s",
                radarControls_.rangeScaleNm,
                RadarAzimuthMnemonic(radarControls_.azimuthScanWidthDeg),
                radarControls_.scanBars,
                RadarIffMnemonic(radarControls_.iffMode));
}

void MfdApplication::DrawStoresControls()
{
    cockpit::DrawSectionHeader("STORES MANAGEMENT", "SMS");
    ImGui::Text("STA %d  |  STORES %d/%zu  |  A-G READY %d",
                stores_.selectedStation,
                CountLoadedStores(stores_),
                kStationCount,
                CountReadyAirGroundStores(stores_));
    const float buttonWidth = TwoColumnButtonWidth();
    if (PanelButton("STEP STA", ImVec2(buttonWidth, 30.0f), "Step through every stores station."))
    {
        StepSmsStation();
    }
    ImGui::SameLine();
    if (PanelButton("PROFILE", ImVec2(buttonWidth, 30.0f), "Cycle the SMS delivery profile."))
    {
        stores_.profile = stores_.profile >= 3 ? 1 : stores_.profile + 1;
    }
    if (cockpit::ActionButton(
            "EMERGENCY JETTISON",
            ImVec2(-1.0f, 32.0f),
            "Clear all stores from the demo inventory.",
            cockpit::ActionTone::Caution))
    {
        JettisonStores();
    }

    if (cockpit::TwoPositionSwitch(
            "master_arm_sms",
            "MASTER ARM",
            stores_.masterArm,
            true,
            "Allow live demo release to remove a store from inventory."))
    {
        stores_.masterArm = !stores_.masterArm;
    }
    ImGui::SameLine();
    if (cockpit::TwoPositionSwitch(
            "simulation_sms",
            "SIM",
            stores_.simulateMode,
            true,
            "Allow simulated release cues without removing stores."))
    {
        stores_.simulateMode = !stores_.simulateMode;
    }
}

void MfdApplication::DrawAirGroundControls()
{
    cockpit::DrawSectionHeader("AIR-TO-GROUND DELIVERY", "SMS");
    const float modeWidth = TwoColumnButtonWidth();
    if (ModeButton("CCIP", stores_.deliveryMode == AirGroundDeliveryMode::Ccip, ImVec2(modeWidth, 30.0f), "Select CCIP-style A-G delivery presentation."))
    {
        SelectAgDeliveryMode(AirGroundDeliveryMode::Ccip);
    }
    ImGui::SameLine();
    if (ModeButton("CCRP", stores_.deliveryMode == AirGroundDeliveryMode::Ccrp, ImVec2(modeWidth, 30.0f), "Select CCRP-style A-G delivery presentation."))
    {
        SelectAgDeliveryMode(AirGroundDeliveryMode::Ccrp);
    }

    ImGui::Text("%s  |  PROF %d  |  STA %d",
                DeliveryModeLabel(stores_.deliveryMode),
                stores_.profile,
                stores_.selectedStation);
    const float actionWidth = ThreeColumnButtonWidth();
    if (PanelButton("STA", ImVec2(actionWidth, 30.0f), "Step to the next loaded A-G release station."))
    {
        StepAgStation();
    }
    ImGui::SameLine();
    if (PanelButton("QTY", ImVec2(actionWidth, 30.0f), "Cycle release quantity."))
    {
        CycleAgQuantity();
    }
    ImGui::SameLine();
    if (PanelButton("PAIR", ImVec2(actionWidth, 30.0f), "Toggle single or pair release."))
    {
        ToggleAgPairs();
    }
    if (PanelButton("INTERVAL", ImVec2(modeWidth, 30.0f), "Cycle ripple interval in seconds."))
    {
        CycleReleaseInterval();
    }
    ImGui::SameLine();
    if (cockpit::ActionButton(
            "PICKLE",
            ImVec2(modeWidth, 30.0f),
            "Run the A-G release action for the selected A-G station.",
            cockpit::ActionTone::Caution))
    {
        ReleaseSelectedAgStore();
    }

    if (cockpit::TwoPositionSwitch(
            "master_arm_ag",
            "MASTER ARM",
            stores_.masterArm,
            true,
            "Allow live demo release to remove a store from inventory."))
    {
        stores_.masterArm = !stores_.masterArm;
    }
    ImGui::SameLine();
    if (cockpit::TwoPositionSwitch(
            "simulation_ag",
            "SIM",
            stores_.simulateMode,
            true,
            "Allow simulated release cues without removing stores."))
    {
        stores_.simulateMode = !stores_.simulateMode;
    }

    ImGui::Text("QTY %d  |  %s  |  INTVL %.2f  |  TT %d",
                stores_.rippleQuantity,
                stores_.ripplePairs == 2 ? "PAIR" : "SGL",
                stores_.releaseIntervalSeconds,
                stores_.releaseSeconds);
    if (stores_.releaseInProgress)
    {
        ImGui::Text("RELEASE STA %d  |  IMPACT %.1f", stores_.releasedStation, stores_.impactTimeRemainingSeconds);
    }
}

void MfdApplication::DrawNavigationControls()
{
    cockpit::DrawSectionHeader("NAVIGATION DISPLAY", "HSD");
    ImGui::Text("STPT %d/%d  |  RNG %.0f NM",
                navControls_.currentSteerpoint,
                ClampWaypointCount(navControls_.waypointCount),
                navControls_.rangeScaleNm);
    const float buttonWidth = TwoColumnButtonWidth();
    if (ModeButton(
            navControls_.centeredDisplay ? "CENTERED" : "DECENTERED",
            navControls_.centeredDisplay,
            ImVec2(buttonWidth, 30.0f),
            "Toggle centered or decentered HSD presentation."))
    {
        ToggleHsdCenteredDisplay();
    }
    ImGui::SameLine();
    if (ModeButton(
            "DECLUTTER",
            navControls_.declutterActive,
            ImVec2(buttonWidth, 30.0f),
            "Toggle HSD declutter for route legs and bullseye."))
    {
        ToggleHsdDeclutter();
    }
    if (PanelButton("STEP STPT", ImVec2(buttonWidth, 30.0f), "Step the selected HSD steerpoint."))
    {
        StepNavSteerpoint();
    }
    ImGui::SameLine();
    if (PanelButton("RANGE", ImVec2(buttonWidth, 30.0f), "Cycle the HSD range scale."))
    {
        CycleNavRange();
    }

    cockpit::DrawSectionHeader("WAYPOINT DATA", "EDIT");
    ImGui::TextDisabled("BEARING");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##waypoint_bearing", &waypointDraftBearingDeg_, 0.0f, 359.0f, "%.0f deg");
    ShowLastItemTooltip("Bearing used by ADD WPT and OVERWRITE.");
    ImGui::TextDisabled("RANGE");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##waypoint_range", &waypointDraftRangeNm_, 1.0f, 160.0f, "%.1f NM");
    ShowLastItemTooltip("Range used by ADD WPT and OVERWRITE.");
    if (PanelButton("ADD WPT", ImVec2(buttonWidth, 30.0f), "Insert the draft waypoint after the selected steerpoint."))
    {
        AddWaypointFromDraft();
    }
    ImGui::SameLine();
    if (PanelButton("OVERWRITE", ImVec2(buttonWidth, 30.0f), "Replace the selected steerpoint with the draft waypoint."))
    {
        OverwriteCurrentWaypointFromDraft();
    }
}

void MfdApplication::PublishFrame()
{
    if (!connected_ || publisher_ == nullptr || !publisher_->IsReady())
    {
        return;
    }

    ui_.Run();
    controller_.Populate(ui_, inputs_);
    if (!ui_.SubmitLatest(*publisher_, sequence_++))
    {
        SetStatus(publisher_->LastError(), true);
        connected_ = false;
        return;
    }

    const std::string publisherError = publisher_->LastError();
    if (!publisherError.empty())
    {
        SetStatus(publisherError, true);
        connected_ = false;
    }
}

void MfdApplication::PollFeedback()
{
    if (feedbackReceiver_ == nullptr)
    {
        return;
    }

    std::string feedbackError;
    ui_.PollFeedback(*feedbackReceiver_, 16, &feedbackError);
    livenessMonitor_.Observe(ui_.TotalDecodedFeedbackPackets(), ui_.WindowReportedClosing(), elapsedSeconds_);
    if (livenessMonitor_.ConsumeDisconnect())
    {
        connected_ = false;
        SetStatus("MFD runtime feedback lost or closed.", true);
    }
}

void MfdApplication::ResetScene()
{
    inputSource_->Reset();
    radarControls_ = inputSource_->Inputs().radar;
    masterMode_ = MasterMode::AirToAir;
    stores_ = StoresState {};
    navControls_ = inputSource_->Inputs().nav;
    waypointDraftBearingDeg_ = 135.0f;
    waypointDraftRangeNm_ = 36.0f;
    activePage_ = MfdPage::Radar;
    inputs_ = inputSource_->Inputs();
    ui_.Initialize();
    controller_.Initialize(ui_);
    radarReturnSubmode_ = RadarSubmode::Rws;
    offscreenView_.SetActivePage(PageName(activePage_));
    SetStatus("Scene reset.", false);
}

void MfdApplication::Shutdown()
{
    if (publisher_ != nullptr && publisher_->IsReady())
    {
        ui_.SubmitShutdown(*publisher_, sequence_++, "LHLD DEMO | CLIENT STOPPED");
        publisher_->Flush();
    }
    offscreenView_.Shutdown();
}

void MfdApplication::SetStatus(std::string status, const bool error)
{
    status_ = std::move(status);
    statusIsError_ = error;
}
} // namespace lhld
