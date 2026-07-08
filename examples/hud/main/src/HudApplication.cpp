/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the Dear ImGui HUD client application.
 */

#include "hud_main/HudApplication.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

#include <imgui.h>

namespace hud_main
{
// Generic HUD contract enums used across the panel code.
using hud::HudGunMode;
using hud::HudMasterMode;
using hud::HudWeaponMode;

namespace
{
constexpr float kModeButtonWidth = 102.0f;
constexpr float kModeButtonHeight = 50.0f;
constexpr float kToggleButtonWidth = 104.0f;
constexpr float kToggleButtonHeight = 40.0f;
constexpr float kActionButtonWidth = 120.0f;
constexpr float kActionButtonHeight = 40.0f;
constexpr float kStickPovRadius = 66.0f;
constexpr float kStickPovDeadZone = 0.10f;
constexpr ImGuiTreeNodeFlags kDefaultOpenPanel = ImGuiTreeNodeFlags_DefaultOpen;

/**
 * @brief Normalized stick command produced by a scripted maneuver.
 */
struct ScriptedStickCommand
{
    /** Normalized pitch-stick command in [-1, 1]. */
    float pitch = 0.0f;
    /** Normalized roll-stick command in [-1, 1]. */
    float roll = 0.0f;
};

constexpr float kLoopPitchCommand = 1.0f;
constexpr float kLoopRollCommand = 0.0f;
constexpr float kBarrelRollPitchCommand = 0.35f;
constexpr float kBarrelRollRollCommand = 1.0f;

/**
 * @brief Resolves the scripted stick command for one panel-selected maneuver.
 *
 * Kept as a small pure function so the maneuver constants stay out of ImGui
 * callbacks and can be replaced independently of the HUD publishing path.
 *
 * @param maneuver Active scripted maneuver.
 * @return Normalized pitch/roll stick command.
 */
ScriptedStickCommand ScriptedManeuverCommand(const HudManeuver maneuver) noexcept
{
    switch (maneuver)
    {
    case HudManeuver::Loop:
        return ScriptedStickCommand {kLoopPitchCommand, kLoopRollCommand};
    case HudManeuver::BarrelRoll:
        return ScriptedStickCommand {kBarrelRollPitchCommand, kBarrelRollRollCommand};
    case HudManeuver::Manual:
        return ScriptedStickCommand {0.0f, 0.0f};
    }

    return ScriptedStickCommand {0.0f, 0.0f};
}

// Keep mode labels centralized so the control panel and telemetry never drift.
const char* MasterModeLabel(const HudMasterMode mode) noexcept
{
    switch (mode)
    {
    case HudMasterMode::Nav:
        return "NAV";
    case HudMasterMode::AirToAir:
        return "A-A";
    case HudMasterMode::AirToGround:
        return "A-G";
    case HudMasterMode::Landing:
        return "LDG";
    }

    return "NAV";
}

// Small formatting helpers keep ImGui drawing code focused on layout and state.
std::string FormatFloatText(const char* label, const float value, const char* unit)
{
    char buffer[64] {};
    std::snprintf(buffer, sizeof(buffer), "%s %.1f %s", label, value, unit);
    return buffer;
}

std::string FormatIntegerText(const char* label, const float value, const char* unit)
{
    char buffer[64] {};
    std::snprintf(buffer, sizeof(buffer), "%s %d %s", label, static_cast<int>(std::lround(value)), unit);
    return buffer;
}

void PushPanelButtonStyle(const bool active)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
    if (active)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.58f, 0.38f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.68f, 0.46f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.47f, 0.31f, 1.0f));
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.20f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.29f, 0.34f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.11f, 0.35f, 0.30f, 1.0f));
    }
}

void PopPanelButtonStyle()
{
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

void ShowLastItemTooltip(const char* tooltip)
{
    if (tooltip != nullptr && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tooltip);
    }
}

bool DrawPanelButton(const char* label, const bool active, const ImVec2 size, const char* tooltip = nullptr)
{
    PushPanelButtonStyle(active);
    const bool pressed = ImGui::Button(label, size);
    PopPanelButtonStyle();
    ShowLastItemTooltip(tooltip);
    return pressed;
}

bool DrawToggleButton(const char* label, bool& value, const ImVec2 size, const char* tooltip = nullptr)
{
    if (!DrawPanelButton(label, value, size, tooltip))
    {
        return false;
    }

    value = !value;
    return true;
}

bool BeginControlPanel(const char* label)
{
    return ImGui::CollapsingHeader(label, kDefaultOpenPanel);
}

void ApplyStickPovInput(PilotControls& controls, HudManeuver& maneuver, const ImVec2 center, const float radius) noexcept
{
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float offsetX = mouse.x - center.x;
    const float offsetY = mouse.y - center.y;
    const float distance = std::sqrt(offsetX * offsetX + offsetY * offsetY);
    const float scale = distance > radius ? radius / std::max(distance, 0.001f) : 1.0f;
    float roll = (offsetX * scale) / radius;
    float pitch = (offsetY * scale) / radius;
    if (std::fabs(roll) < kStickPovDeadZone)
    {
        roll = 0.0f;
    }
    if (std::fabs(pitch) < kStickPovDeadZone)
    {
        pitch = 0.0f;
    }

    controls.rollCommand = std::clamp(roll, -1.0f, 1.0f);
    controls.pitchCommand = std::clamp(pitch, -1.0f, 1.0f);
    maneuver = HudManeuver::Manual;
}

void DrawStickPov(PilotControls& controls, HudManeuver& maneuver)
{
    const float diameter = kStickPovRadius * 2.0f;
    ImGui::InvisibleButton("##manual_stick_pov", ImVec2(diameter, diameter));
    ShowLastItemTooltip("Drag like an aircraft stick: pull down to pitch up, push up to pitch down.");
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 center {min.x + kStickPovRadius, min.y + kStickPovRadius};
    if (active)
    {
        ApplyStickPovInput(controls, maneuver, center, kStickPovRadius);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(
        hovered || active ? ImVec4(0.32f, 0.95f, 0.58f, 1.0f) : ImVec4(0.32f, 0.47f, 0.55f, 1.0f));
    const ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.05f, 0.08f, 0.10f, 1.0f));
    const ImU32 axisColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.18f, 0.36f, 0.36f, 1.0f));
    const ImU32 knobColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.28f, 0.95f, 0.58f, 1.0f));
    drawList->AddCircleFilled(center, kStickPovRadius, fillColor, 64);
    drawList->AddCircle(center, kStickPovRadius, borderColor, 64, 2.5f);
    drawList->AddCircle(center, kStickPovRadius * 0.50f, axisColor, 48, 1.0f);
    drawList->AddLine(
        ImVec2(center.x - kStickPovRadius, center.y),
        ImVec2(center.x + kStickPovRadius, center.y),
        axisColor,
        1.5f);
    drawList->AddLine(
        ImVec2(center.x, center.y - kStickPovRadius),
        ImVec2(center.x, center.y + kStickPovRadius),
        axisColor,
        1.5f);

    const ImVec2 knob {
        center.x + controls.rollCommand * kStickPovRadius * 0.74f,
        center.y + controls.pitchCommand * kStickPovRadius * 0.74f};
    drawList->AddCircleFilled(knob, 12.0f, knobColor, 32);
    drawList->AddCircle(knob, 12.0f, IM_COL32(8, 13, 18, 255), 32, 2.0f);
}
} // namespace

HudApplication::HudApplication()
{
    // Align the UI slider with the simulation default before the first frame.
    controls_.throttle = simulation_.Aircraft().throttle;
    SyncHudInputBufferFromSimulation();
}

bool HudApplication::Initialize(std::string& error)
{
    // Loading, validation and transport creation all belong to the reusable
    // HUD runtime client; the application only tracks the connection state.
    connected_ = hudRuntime_.Initialize(error);
    if (!connected_)
    {
        return false;
    }

    SetStatus("HUD client connected.", false);
    return true;
}

void HudApplication::DrawFrame(const float deltaSeconds)
{
    ApplyKeyboardControls();
    // Apply once before drawing so the panel displays the active scripted
    // command, then again after drawing to catch mode changes made this frame.
    ApplyManeuverControls();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin(
        "HUD controls",
        nullptr,
        ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
    DrawConnectionPanel();
    DrawFlightControls();
    DrawWeaponControls();
    DrawTelemetryPanel();
    ImGui::End();
    ImGui::PopStyleVar(2);

    ApplyManeuverControls();
    // The HUD buffer is refreshed after the panel has collected all operator
    // intent, which keeps rendering, input and publishing in one frame phase.
    UpdateHudInputBufferFromUi(deltaSeconds);

    PublishFrame();
}

void HudApplication::UpdateHudInputBufferFromUi(const float deltaSeconds)
{
    // This is the HUD-client adapter boundary. In a real integration, replace
    // this block with code that fills `hudInputs_` from the aircraft state:
    //
    //   hudInputs_.aircraft.altitudeMeters       = aircraft altitude MSL;
    //   hudInputs_.aircraft.radioAltitudeMeters  = radar-altimeter height AGL;
    //   hudInputs_.aircraft.yawRad/pitchRad/rollRad = aircraft attitude;
    //   hudInputs_.aircraft.headingRad           = navigation heading;
    //   hudInputs_.aircraft.north/east/downSpeedMps = NED velocity vector;
    //   hudInputs_.aircraft.mach                 = current Mach, or <= 0 to derive a fallback;
    //   hudInputs_.aircraft.specificEnergyRateMps = h_dot + V / g * V_dot;
    //   hudInputs_.target.*                      = resolved A-A target data;
    //   hudInputs_.weapon.*                      = resolved avionics/weapon state.
    //
    // Do not send raw ImGui/panel commands directly to generated HUD handles.
    // They must first become this semantic SI buffer, then the HUD runtime
    // client converts the buffer to HUD commands.
    simulation_.SetControls(controls_);
    if (running_)
    {
        simulation_.Step(deltaSeconds);
    }
    SyncHudInputBufferFromSimulation();
}

void HudApplication::Shutdown()
{
    // The runtime client emits the final shutdown caption and flushes the
    // realtime transport before releasing it.
    hudRuntime_.Shutdown();
    connected_ = false;
}

void HudApplication::ApplyKeyboardControls()
{
    // Keyboard arrows are intentionally sampled as digital stick commands. The
    // simulation applies smoothing, so this layer only collects pilot intent.
    int pitchCommand = 0;
    int rollCommand = 0;
    if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
    {
        ++pitchCommand;
    }
    if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
    {
        --pitchCommand;
    }
    if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
    {
        ++rollCommand;
    }
    if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
    {
        --rollCommand;
    }

    controls_.pitchCommand = static_cast<float>(std::clamp(pitchCommand, -1, 1));
    controls_.rollCommand = static_cast<float>(std::clamp(rollCommand, -1, 1));
    if (pitchCommand != 0 || rollCommand != 0)
    {
        // Any direct pilot input takes ownership back from scripted maneuvers.
        maneuver_ = HudManeuver::Manual;
    }
}

void HudApplication::SelectManeuver(const HudManeuver maneuver) noexcept
{
    maneuver_ = maneuver;
    if (maneuver_ == HudManeuver::Manual)
    {
        controls_.pitchCommand = 0.0f;
        controls_.rollCommand = 0.0f;
    }
}

void HudApplication::ApplyManeuverControls() noexcept
{
    // Scripted maneuvers write the same control fields as manual input. The
    // command constants live in `ScriptedManeuverCommand`, so this method only
    // forwards resolved panel intent.
    if (maneuver_ == HudManeuver::Manual)
    {
        return;
    }

    const ScriptedStickCommand command = ScriptedManeuverCommand(maneuver_);
    controls_.pitchCommand = command.pitch;
    controls_.rollCommand = command.roll;
}

void HudApplication::SyncHudInputBufferFromSimulation()
{
    // Keep the buffer copy explicit. This makes the sample boundary obvious and
    // leaves room for a future external-aircraft producer to replace it.
    hudInputs_ = simulation_.Inputs();
}

void HudApplication::DrawConnectionPanel()
{
    // This panel is intentionally read-heavy: it exposes transport health and
    // lifecycle actions without mutating the semantic HUD buffer directly.
    // The UDP endpoint itself is a runtime-client implementation detail read
    // from the HUD window JSON, so only the stream state is displayed here.
    if (ImGui::BeginTable("##connection_header", 3, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch, 0.34f);
        ImGui::TableSetupColumn("Transport", ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 0.36f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("HUD CONTROL PANEL");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("UDP HUD stream");
        ImGui::TextColored(
            connected_ ? ImVec4(0.35f, 0.95f, 0.58f, 1.0f) : ImVec4(1.0f, 0.55f, 0.42f, 1.0f),
            "%s",
            connected_ ? "streaming" : "disconnected");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextColored(
            statusIsError_ ? ImVec4(1.0f, 0.55f, 0.42f, 1.0f) : ImVec4(0.72f, 0.86f, 0.95f, 1.0f),
            "%s",
            status_.empty() ? "Ready." : status_.c_str());
        ImGui::EndTable();
    }

    if (DrawPanelButton(
            running_ ? "PAUSE" : "RUN",
            running_,
            ImVec2(kActionButtonWidth, kActionButtonHeight),
            "Start or pause the sample aircraft simulation."))
    {
        running_ = !running_;
    }

    ImGui::SameLine();
    if (DrawPanelButton(
            "RESET SCENE",
            false,
            ImVec2(kActionButtonWidth, kActionButtonHeight),
            "Reset aircraft, target, weapons and HUD UI state."))
    {
        ResetScene();
    }

    ImGui::SameLine();
    if (DrawPanelButton(
            "RECONNECT",
            connected_,
            ImVec2(kActionButtonWidth, kActionButtonHeight),
            "Reconnect the UDP publisher and resend the generated HUD startup state."))
    {
        std::string error;
        // Reconnect re-initializes the runtime client, which reloads the
        // validated asset configuration and resends the generated startup state.
        connected_ = hudRuntime_.Initialize(error);
        if (!connected_)
        {
            SetStatus(error, true);
        }
        else
        {
            SetStatus("HUD client reconnected.", false);
        }
    }
    ImGui::Separator();
}

void HudApplication::DrawFlightControls()
{
    if (BeginControlPanel("Aircraft"))
    {
        // The UI presents throttle as percent, while `PilotControls` stores the
        // normalized ratio expected by the simulation.
        float throttlePercent = controls_.throttle * 100.0f;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderFloat("Throttle", &throttlePercent, 0.0f, 100.0f, "%.0f %%"))
        {
            controls_.throttle = throttlePercent / 100.0f;
            maneuver_ = HudManeuver::Manual;
        }
        DrawToggleButton(
            "AFTERBURNER",
            controls_.afterburnerRequested,
            ImVec2(kActionButtonWidth, kActionButtonHeight),
            "Request afterburner in the sample aircraft model.");
    }

    if (BeginControlPanel("Maneuver"))
    {
        if (DrawPanelButton(
                "MANUAL",
                maneuver_ == HudManeuver::Manual,
                ImVec2(kActionButtonWidth, kActionButtonHeight),
                "Return stick control to keyboard or POV input."))
        {
            SelectManeuver(HudManeuver::Manual);
        }
        ImGui::SameLine();
        if (DrawPanelButton(
                "LOOP",
                maneuver_ == HudManeuver::Loop,
                ImVec2(kActionButtonWidth, kActionButtonHeight),
                "Command a scripted pitch-up loop."))
        {
            SelectManeuver(HudManeuver::Loop);
        }
        ImGui::SameLine();
        if (DrawPanelButton(
                "BARREL",
                maneuver_ == HudManeuver::BarrelRoll,
                ImVec2(kActionButtonWidth, kActionButtonHeight),
                "Command a scripted barrel-roll maneuver."))
        {
            SelectManeuver(HudManeuver::BarrelRoll);
        }
    }

    if (BeginControlPanel("Stick POV"))
    {
        DrawStickPov(controls_, maneuver_);

        ImGui::Text("Pitch cmd %+.0f", controls_.pitchCommand);
        ImGui::SameLine(180.0f);
        ImGui::Text("Roll cmd %+.0f", controls_.rollCommand);
    }
}

void HudApplication::DrawWeaponControls()
{
    if (BeginControlPanel("HUD Mode"))
    {
        if (DrawPanelButton(
                "NAV",
                controls_.masterMode == HudMasterMode::Nav,
                ImVec2(kModeButtonWidth, kModeButtonHeight),
                "Select navigation mode and clear weapon-delivery symbology."))
        {
            SelectHudMode(HudMasterMode::Nav, HudWeaponMode::None, HudGunMode::None);
        }
        ImGui::SameLine();
        if (DrawPanelButton(
                "A-A MSL",
                controls_.masterMode == HudMasterMode::AirToAir &&
                    controls_.weaponMode == HudWeaponMode::AirToAirMissile,
                ImVec2(kModeButtonWidth, kModeButtonHeight),
                "Select air-to-air missile mode and show missile cues when armed or SIM is active."))
        {
            SelectHudMode(HudMasterMode::AirToAir, HudWeaponMode::AirToAirMissile, HudGunMode::None);
        }
        ImGui::SameLine();
        if (DrawPanelButton(
                "EEGS",
                controls_.masterMode == HudMasterMode::AirToAir &&
                    controls_.weaponMode == HudWeaponMode::AirToAirGun,
                ImVec2(kModeButtonWidth, kModeButtonHeight),
                "Select air-to-air gun mode with EEGS funnel symbology."))
        {
            SelectHudMode(HudMasterMode::AirToAir, HudWeaponMode::AirToAirGun, HudGunMode::Eegs);
        }

        if (DrawPanelButton(
                "CCIP",
                controls_.masterMode == HudMasterMode::AirToGround &&
                    controls_.weaponMode == HudWeaponMode::AirToGroundCcip,
                ImVec2(kModeButtonWidth, kModeButtonHeight),
                "Select air-to-ground CCIP delivery symbology."))
        {
            SelectHudMode(HudMasterMode::AirToGround, HudWeaponMode::AirToGroundCcip, HudGunMode::None);
        }
        ImGui::SameLine();
        if (DrawPanelButton(
                "STRF",
                controls_.masterMode == HudMasterMode::AirToGround &&
                    controls_.weaponMode == HudWeaponMode::AirToGroundStrafe,
                ImVec2(kModeButtonWidth, kModeButtonHeight),
                "Select air-to-ground strafe gun symbology."))
        {
            SelectHudMode(HudMasterMode::AirToGround, HudWeaponMode::AirToGroundStrafe, HudGunMode::Strafe);
        }
        ImGui::SameLine();
        if (DrawPanelButton(
                "LANDING",
                controls_.masterMode == HudMasterMode::Landing,
                ImVec2(kModeButtonWidth, kModeButtonHeight),
                "Select landing mode and enable landing-specific HUD cues."))
        {
            SelectHudMode(HudMasterMode::Landing, HudWeaponMode::None, HudGunMode::None);
        }
    }

    if (BeginControlPanel("Master / Target"))
    {
        DrawToggleButton(
            "MASTER ARM",
            controls_.masterArm,
            ImVec2(kToggleButtonWidth, kToggleButtonHeight),
            "Enable live weapon-delivery symbology and launch gates.");
        ImGui::SameLine();
        DrawToggleButton(
            "SIM",
            controls_.simulateMode,
            ImVec2(kToggleButtonWidth, kToggleButtonHeight),
            "Enable simulated weapon symbology without arming live weapons.");
        ImGui::SameLine();
        DrawToggleButton(
            "TRIGGER",
            controls_.triggerHeld,
            ImVec2(kToggleButtonWidth, kToggleButtonHeight),
            "Hold the gun trigger for EEGS FEDS or locked-target BATR cues.");
        ImGui::SameLine();
        DrawToggleButton(
            "LOCK",
            controls_.targetLocked,
            ImVec2(kToggleButtonWidth, kToggleButtonHeight),
            "Toggle radar/target lock for air-to-air gun and missile presentation.");
    }

    if (BeginControlPanel("Missile"))
    {
        const bool aim120Selected = simulation_.SelectedMissile() == MissileType::Aim120C;
        if (DrawPanelButton(
                "AIM-120C",
                aim120Selected,
                ImVec2(kToggleButtonWidth, kToggleButtonHeight),
                "Select AIM-120C for missile inventory and launch-zone timing."))
        {
            SelectMissile(MissileType::Aim120C);
        }
        ImGui::SameLine();
        if (DrawPanelButton(
                "AIM-9M",
                !aim120Selected,
                ImVec2(kToggleButtonWidth, kToggleButtonHeight),
                "Select AIM-9M for missile inventory and timing display."))
        {
            SelectMissile(MissileType::Aim9M);
        }

        if (DrawPanelButton(
                "FIRE MISSILE",
                false,
                ImVec2(kActionButtonWidth, kActionButtonHeight),
                "Attempt to fire the selected missile through inventory and launch gates."))
        {
            FireSelectedMissile();
        }
        ImGui::SameLine();
        if (DrawPanelButton(
                "CYCLE MISSILE",
                false,
                ImVec2(kActionButtonWidth, kActionButtonHeight),
                "Cycle the selected missile type in the sample inventory."))
        {
            // Cycling is delegated to the simulation so inventory and selected type
            // remain consistent with the same state owner used by launches.
            simulation_.CycleSelectedMissile();
            SyncHudInputBufferFromSimulation();
        }

        ImGui::Text(
            "Inventory %s",
            FormatMissileInventory(simulation_.SelectedMissile(), simulation_.Inventory()).c_str());
        ImGui::SameLine(180.0f);
        ImGui::Text("%s", lastFireAccepted_ ? "Launch accepted" : "Launch gated");
    }

    if (BeginControlPanel("Landing / ILS"))
    {
        DrawToggleButton(
            "GEAR DOWN",
            controls_.landingGearDown,
            ImVec2(kToggleButtonWidth, kToggleButtonHeight),
            "Toggle landing gear state and landing air-data presentation.");
        ImGui::SameLine();
        bool ilsSelected = controls_.ilsSelected;
        if (DrawToggleButton(
                "ILS",
                ilsSelected,
                ImVec2(kToggleButtonWidth, kToggleButtonHeight),
                "Power and select the ILS receiver."))
        {
            SetIlsEnabled(ilsSelected);
        }
        ImGui::SameLine();
        DrawToggleButton(
            "ILS CMD",
            controls_.ilsCommandSteeringActive,
            ImVec2(kToggleButtonWidth, kToggleButtonHeight),
            "Toggle ILS command-steering cue display when ILS is valid.");
        ImGui::SameLine();
        DrawToggleButton(
            "DECLUTTER",
            controls_.landingDeclutterActive,
            ImVec2(kToggleButtonWidth, kToggleButtonHeight),
            "Hide selected landing and ILS clutter for the landing presentation.");
    }
}

void HudApplication::DrawTelemetryPanel()
{
    if (!BeginControlPanel("Telemetry"))
    {
        return;
    }

    // Telemetry uses the same conversion helpers as the HUD runtime. That
    // keeps the debug panel from inventing display units independently.
    const hud::AircraftState aircraft = hud::BuildAircraftStateForHud(hudInputs_.aircraft);
    const hud::TargetState target = hud::BuildTargetStateForHud(hudInputs_.target);
    ImGui::Text("%s", FormatIntegerText("Speed", aircraft.speedKts, "KT").c_str());
    ImGui::Text("%s", FormatFloatText("Mach", aircraft.mach, "").c_str());
    ImGui::Text("%s", FormatFloatText("Pitch", aircraft.pitchDegrees, "deg").c_str());
    ImGui::Text("%s", FormatFloatText("Roll", aircraft.rollDegrees, "deg").c_str());
    ImGui::Text("%s", FormatIntegerText("Altitude", aircraft.altitudeFeet, "ft").c_str());
    ImGui::Text("%s", FormatIntegerText("Radar altitude", aircraft.radarAltitudeFeet, "ft").c_str());
    ImGui::Text("%s", FormatFloatText("Energy", aircraft.energyRate, "").c_str());
    ImGui::Text("Mode %s", MasterModeLabel(hudInputs_.weapon.masterMode));
    ImGui::Text("Target %.1f NM / closure %.0f KT", target.rangeNm, target.closureKts);
    ImGui::Text("%s", hudInputs_.weapon.missileInFlight ? "Missile in flight" : "No missile in flight");
}

void HudApplication::PublishFrame()
{
    if (!connected_)
    {
        return;
    }

    // The runtime client owns the whole publishing pipeline: generated-UI
    // cycle, semantic conversion, batch submission and feedback liveness.
    std::string error;
    if (!hudRuntime_.Publish(hudInputs_, error))
    {
        // Treat publish errors as a lost connection so the panel stops
        // claiming that realtime updates are still flowing.
        SetStatus(error, true);
        connected_ = false;
    }
}

void HudApplication::ResetScene()
{
    // Reset the simulation and panel intent; the runtime client keeps its
    // generated baselines and simply publishes the fresh sample as a delta.
    simulation_.Reset();
    controls_ = {};
    controls_.throttle = simulation_.Aircraft().throttle;
    controls_.masterMode = HudMasterMode::Nav;
    SyncHudInputBufferFromSimulation();
    maneuver_ = HudManeuver::Manual;
    lastFireAccepted_ = false;
    SetStatus("Scene reset.", false);
}

void HudApplication::FireSelectedMissile()
{
    // Apply the latest panel controls before evaluating launch gates so mode,
    // throttle and selected weapon are coherent with the visible UI.
    simulation_.SetControls(controls_);
    lastFireAccepted_ = simulation_.FireSelectedMissile();
    SyncHudInputBufferFromSimulation();
    SetStatus(lastFireAccepted_ ? "Missile launched." : "Launch inhibited.", !lastFireAccepted_);
}

void HudApplication::SelectMissile(const MissileType missileType)
{
    // Selection is persisted in the simulation-owned weapon sample and then
    // copied back to the publish buffer.
    simulation_.SelectMissile(missileType);
    SyncHudInputBufferFromSimulation();
}

void HudApplication::SelectHudMode(const HudMasterMode masterMode,
                                       const HudWeaponMode weaponMode,
                                       const HudGunMode gunMode) noexcept
{
    controls_.masterMode = masterMode;
    controls_.weaponMode = weaponMode;
    controls_.gunMode = gunMode;
    controls_.landingModeActive = masterMode == HudMasterMode::Landing;
    if (masterMode != HudMasterMode::Landing)
    {
        controls_.landingDeclutterActive = false;
    }
    if (weaponMode != HudWeaponMode::AirToAirGun && weaponMode != HudWeaponMode::AirToGroundStrafe)
    {
        controls_.triggerHeld = false;
    }
}

void HudApplication::SetIlsEnabled(const bool enabled) noexcept
{
    controls_.ilsPowered = enabled;
    controls_.ilsSelected = enabled;
    controls_.ilsSignalValid = enabled;
    if (!enabled)
    {
        controls_.ilsCommandSteeringActive = false;
    }
}

void HudApplication::SetStatus(std::string status, const bool error)
{
    // Move the text into the application state; ImGui reads it on the next
    // frame without keeping references to a temporary string.
    status_ = std::move(status);
    statusIsError_ = error;
}
} // namespace hud_main
