/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Operator-oriented mock client used to exercise the public UDP command API against a running MFD window.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <imgui.h>

#include "CockpitDemoMockupUi.h"
#include "FullDemoMockupUi.h"
#include "MinimalRadarMockupUi.h"
#include "Win32Dx11ImGuiHost.h"
#include "mfd/client/ClientSdk.h"

#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
#endif

namespace
{
/** @brief Width of the navigation pane shown on the left of the mockup UI. */
constexpr float kSidebarWidth = 300.0f;
/** @brief Height reserved for the footer status area. */
constexpr float kStatusHeight = 72.0f;
/** @brief Page name targeted by the built-in radar stress simulator. */
constexpr std::string_view kRadarSimulationPageName = "Radar";
/** @brief Reticle template id used to instantiate simulated radar tracks. */
constexpr std::string_view kRadarSimulationTemplateId = "radar_track";
/** @brief Number of simulated radar tracks emitted by the mockup batch demo. */
constexpr int kRadarSimulationTrackCount = 100;
/** @brief Target update cadence of the radar stress simulator. */
constexpr float kRadarSimulationIntervalSeconds = 0.020f;
/** @brief Composite cockpit page controlled by the pilot-station demo. */
constexpr std::string_view kCockpitPageName = "Cockpit";
/** @brief Dynamic reticle template used by cockpit radar contacts. */
constexpr std::string_view kCockpitRadarTemplateId = "cockpit_radar_contact";
/** @brief Target update cadence of the cockpit simulation loop. */
constexpr float kCockpitSimulationIntervalSeconds = 0.020f;
constexpr float kDegreesToRadians = 0.01745329252f;
constexpr float kRadiansToDegrees = 57.2957795f;
constexpr float kKnotsPerMach = 661.0f;
constexpr float kCockpitAdiCenterX = -1.03f;
constexpr float kCockpitAdiCenterY = 0.00f;
constexpr float kCockpitHudCenterX = 0.00f;
constexpr float kCockpitHudCenterY = 0.00f;
constexpr float kCockpitRadarCenterX = 1.03f;
constexpr float kCockpitRadarCenterY = -0.02f;
constexpr std::string_view kFullDemoWindowFile = "assets/windows/demo_pages.json";
constexpr std::string_view kCockpitDemoWindowFile = "assets/windows/demo_pages_cockpit.json";
constexpr std::string_view kMinimalRadarWindowFile = "assets/windows/demo_pages_minimal.json";

enum class WindowPresetKind
{
    Unknown,
    FullDemo,
    CockpitDemo,
    MinimalRadar
};

/** @brief High-level object type currently selected in the navigation tree. */
enum class SelectionKind
{
    Page,
    Reticle,
    Strobe
};

/** @brief Current tree selection routed to the inspector panel. */
struct NavigationSelection
{
    SelectionKind kind = SelectionKind::Page;
    int pageIndex = 0;
    int reticleIndex = -1;
};

/** @brief Editable draft mirroring the runtime view and optional strobe of one page. */
struct PageDraft
{
    std::array<float, 2> center {0.0f, 0.0f};
    float zoom = 1.0f;
    bool hasStrobe = false;
    int selectedStrobeIndex = -1;
    bool strobeEnabled = false;
    std::array<float, 2> strobePosition {0.0f, 0.0f};
};

/** @brief Editable draft for whole-window display controls. */
struct WindowDisplayDraft
{
    bool invertColors = false;
    float brightness = 1.0f;
    bool disabled = false;
};

/** @brief Editable draft mirroring the public fields that can be patched on one static reticle. */
struct ReticleDraft
{
    bool visible = true;
    bool blinkEnabled = false;
    std::string blinkTypeName;
    std::array<float, 2> position {0.0f, 0.0f};
    float rotation = 0.0f;
    float thickness = 0.0042f;
    ImVec4 color {0.20f, 1.00f, 0.40f, 1.00f};
    bool hasText = false;
    float letterSpacing = mfd::kDefaultTextLetterSpacing;
    std::array<char, 128> text {};
};

/** @brief Editable draft used to create, update or remove one dynamic reticle instance. */
struct DynamicDraft
{
    std::array<char, 64> id {};
    bool visible = true;
    bool blinkEnabled = false;
    std::string blinkTypeName;
    std::array<float, 2> position {0.25f, 0.15f};
    float rotation = 0.0f;
    float thickness = 0.0042f;
    ImVec4 color {0.30f, 0.88f, 1.00f, 1.00f};
    float letterSpacing = mfd::kDefaultTextLetterSpacing;
    std::array<char, 64> text {};
};

/** @brief One convenience preset mapping a UI label to a root window JSON file. */
struct WindowFilePreset
{
    const char* label = "";
    std::filesystem::path path;
};

/** @brief Runtime counters and timing data of the 100-track radar batch simulator. */
struct RadarSimulationState
{
    bool enabled = false;
    bool clearRequested = false;
    float accumulatorSeconds = 0.0f;
    float elapsedSeconds = 0.0f;
    float lastObservedCycleMs = 0.0f;
    float lastSendDurationMs = 0.0f;
    int lastCommandCount = 0;
    std::uint32_t nextSequence = 1;
    double previousSendTimestamp = 0.0;
};

/** @brief Runtime state of the integrated cockpit demo driven from the mockup UI. */
struct CockpitSimulationState
{
    bool enabled = false;
    bool radarEnabled = true;
    float accumulatorSeconds = 0.0f;
    float elapsedSeconds = 0.0f;
    float pitchDegrees = 2.0f;
    float bankDegrees = 0.0f;
    float headingDegrees = 32.0f;
    float selectedHeadingDegrees = 32.0f;
    float flightPathAngleDegrees = 1.5f;
    float pitchControlInput = 0.0f;
    float rollControlInput = 0.0f;
    float throttle = 0.48f;
    float speedKts = 320.0f;
    float ownshipX = 0.0f;
    float ownshipY = 0.0f;
    float lastObservedCycleMs = 0.0f;
    float lastSendDurationMs = 0.0f;
    int lastCommandCount = 0;
    std::uint32_t nextSequence = 1;
    double previousSendTimestamp = 0.0;
};

/** @brief Authored seed used to synthesize one cockpit radar contact in the simple flight simulation. */
struct CockpitTargetSeed
{
    const char* id = "";
    const char* label = "";
    float baseX = 0.0f;
    float baseY = 0.0f;
    float orbitRadius = 0.0f;
    float orbitRate = 0.0f;
    float orbitPhase = 0.0f;
    mfd::ColorRgba color {};
    bool blink = false;
};

/** @brief Latest feedback snapshot received from a window strobe plus its reception timestamp. */
struct StrobeFeedbackEntry
{
    mfd::StrobeStatusFeedback feedback;
    double receivedTimestamp = 0.0;
};

/**
 * @brief Copies a string view into a fixed-size ImGui-friendly character buffer.
 * @tparam N Capacity of the destination array.
 * @param destination Fixed-size destination buffer.
 * @param value Source text to copy and truncate if needed.
 */
template <std::size_t N>
void CopyTextBuffer(std::array<char, N>& destination, const std::string_view value)
{
    std::snprintf(destination.data(), destination.size(), "%.*s", static_cast<int>(value.size()), value.data());
}

/** @brief Builds the public runtime id of one simulated radar track. */
std::string RadarTrackId(const int index)
{
    char buffer[32] {};
    std::snprintf(buffer, sizeof(buffer), "mock_radar_%03d", index + 1);
    return buffer;
}

/** @brief Builds the short label displayed by one simulated radar track. */
std::string RadarTrackLabel(const int index)
{
    char buffer[16] {};
    std::snprintf(buffer, sizeof(buffer), "R%03d", index + 1);
    return buffer;
}

/** @brief Wraps an angle in degrees into the `[0, 360[` range. */
float WrapDegrees(float value)
{
    value = std::fmod(value, 360.0f);
    if (value < 0.0f)
    {
        value += 360.0f;
    }

    return value;
}

/** @brief Moves one scalar toward a target without overshooting it. */
float Approach(const float current, const float target, const float maxDelta)
{
    if (current < target)
    {
        return std::min(current + maxDelta, target);
    }

    return std::max(current - maxDelta, target);
}

/** @brief Formats a heading as a three-digit cockpit-style value. */
std::string FormatHeadingValue(const float headingDegrees)
{
    char buffer[16] {};
    std::snprintf(buffer, sizeof(buffer), "%03d", static_cast<int>(std::lround(WrapDegrees(headingDegrees))) % 360);
    return buffer;
}

/** @brief Formats a scalar using a caller-provided `printf`-style format string. */
std::string FormatSignedFloat(const float value, const char* format)
{
    char buffer[32] {};
    std::snprintf(buffer, sizeof(buffer), format, value);
    return buffer;
}

/** @brief Formats an airspeed value in knots as a compact three-digit field. */
std::string FormatSpeedValue(const float speedKts)
{
    char buffer[16] {};
    std::snprintf(buffer, sizeof(buffer), "%03d", static_cast<int>(std::lround(std::max(speedKts, 0.0f))));
    return buffer;
}

/** @brief Formats a normalized factor as a percent string. */
std::string FormatPercentValue(const float factor)
{
    char buffer[16] {};
    std::snprintf(buffer, sizeof(buffer), "%02d%%", static_cast<int>(std::lround(std::clamp(factor, 0.0f, 1.0f) * 100.0f)));
    return buffer;
}

/** @brief Synthetic targets used by the cockpit radar showcase. */
const std::array<CockpitTargetSeed, 6> kCockpitTargets {{
    {"cockpit_contact_01", "B21", 1.8f, 9.4f, 0.5f, 0.30f, 0.2f, mfd::ColorRgba {94, 244, 162, 255}, false},
    {"cockpit_contact_02", "M42", -4.8f, 7.6f, 0.7f, -0.25f, 1.0f, mfd::ColorRgba {255, 198, 109, 255}, true},
    {"cockpit_contact_03", "A17", 6.1f, 5.7f, 0.6f, 0.42f, 2.2f, mfd::ColorRgba {88, 214, 255, 255}, false},
    {"cockpit_contact_04", "F03", -7.0f, 3.0f, 0.8f, 0.36f, 0.6f, mfd::ColorRgba {255, 132, 92, 255}, true},
    {"cockpit_contact_05", "L08", 2.5f, -3.4f, 0.5f, -0.44f, 2.8f, mfd::ColorRgba {166, 255, 206, 255}, false},
    {"cockpit_contact_06", "T55", -3.2f, -7.6f, 0.9f, 0.28f, 1.9f, mfd::ColorRgba {255, 176, 98, 255}, true},
}};

WindowPresetKind DetectWindowPresetKind(const std::filesystem::path& windowFile)
{
    const std::string normalized = windowFile.lexically_normal().generic_string();

    if (normalized == kFullDemoWindowFile)
    {
        return WindowPresetKind::FullDemo;
    }

    if (normalized == kCockpitDemoWindowFile)
    {
        return WindowPresetKind::CockpitDemo;
    }

    if (normalized == kMinimalRadarWindowFile)
    {
        return WindowPresetKind::MinimalRadar;
    }

    return WindowPresetKind::Unknown;
}

template <typename DynamicSet, typename DynamicReticle>
void EnsureGeneratedDynamicReticles(DynamicSet& set,
                                    std::vector<DynamicReticle*>& reticles,
                                    const std::size_t count)
{
    while (reticles.size() < count)
    {
        reticles.push_back(&set.Create());
    }
}

template <typename DynamicSet, typename DynamicReticle>
void RemoveGeneratedDynamicReticles(DynamicSet& set, std::vector<DynamicReticle*>& reticles)
{
    for (DynamicReticle* reticle : reticles)
    {
        if (reticle != nullptr)
        {
            set.Remove(*reticle);
        }
    }

    reticles.clear();
}

template <typename GeneratedUi>
std::unique_ptr<GeneratedUi> MakeInitializedGeneratedUi()
{
    std::unique_ptr<GeneratedUi> ui = std::make_unique<GeneratedUi>();
    ui->Initialize();
    return ui;
}

template <typename WindowUi, typename DynamicTrack>
void PopulateRadarSimulationUi(WindowUi& ui,
                               std::vector<DynamicTrack*>& generatedTracks,
                               const float elapsedSeconds)
{
    static constexpr std::array<mfd::ColorRgba, 4> kTrackPalette {{
        mfd::ColorRgba {0, 255, 0, 255},
        mfd::ColorRgba {72, 220, 255, 255},
        mfd::ColorRgba {255, 191, 0, 255},
        mfd::ColorRgba {255, 96, 96, 255},
    }};

    constexpr float kPi = 3.14159265359f;
    constexpr float kTwoPi = kPi * 2.0f;

    ui.Run();
    auto& radar = ui.Radar();
    auto& tracks = radar.DynamicRadarTrack();
    tracks.SetVisible(true);
    EnsureGeneratedDynamicReticles(tracks, generatedTracks, static_cast<std::size_t>(kRadarSimulationTrackCount));

    for (int index = 0; index < kRadarSimulationTrackCount; ++index)
    {
        const float normalizedIndex = static_cast<float>(index) / static_cast<float>(kRadarSimulationTrackCount);
        const float ringIndex = static_cast<float>(index % 10) / 9.0f;
        const float speed = 0.35f + normalizedIndex * 1.15f;
        const float baseAngle = normalizedIndex * kTwoPi;
        const float angle = baseAngle + elapsedSeconds * speed;
        const float radiusPulse = 0.035f * std::sin(elapsedSeconds * 0.85f + normalizedIndex * 11.0f);
        const float radius = std::clamp(0.18f + ringIndex * 0.62f + radiusPulse, 0.12f, 0.93f);
        const float x = std::cos(angle) * radius;
        const float y = std::sin(angle) * radius;
        const float headingDegrees = angle * kRadiansToDegrees + 90.0f;

        DynamicTrack& track = *generatedTracks[static_cast<std::size_t>(index)];
        track.SetVisible(true);
        track.SetPosition(mfd::Vec2 {x, y});
        track.SetRotationDegrees(headingDegrees);
        track.SetColor(kTrackPalette[static_cast<std::size_t>(index) % kTrackPalette.size()]);
        track.TrackLabel().SetText(RadarTrackLabel(index));
        track.TrackLabel().SetLetterSpacing(0.0080f);
        track.SetBlinkEnabled(true);

        if (index % 3 == 0)
        {
            track.Blink = radar.fast;
        }
        else
        {
            track.Blink = radar.caution;
        }
    }

}

template <typename WindowUi, typename DynamicContact>
void PopulateCockpitSimulationUi(WindowUi& ui,
                                 std::vector<DynamicContact*>& generatedContacts,
                                 const CockpitSimulationState& simulation)
{
    static constexpr mfd::ColorRgba kHudNominal {46, 255, 162, 255};
    static constexpr mfd::ColorRgba kHudWarning {255, 198, 109, 255};
    static constexpr mfd::ColorRgba kRadarSearch {86, 244, 162, 255};
    static constexpr mfd::ColorRgba kRadarStandby {255, 198, 109, 255};

    const bool overspeed = simulation.speedKts > 700.0f;
    const float adiPitchOffset = std::clamp(-simulation.pitchDegrees * 0.0063f, -0.60f, 0.60f);
    const float hudPitchOffset = std::clamp(-simulation.pitchDegrees * 0.0080f, -0.24f, 0.24f);
    const float hudFpmX = std::clamp(simulation.bankDegrees * 0.0032f, -0.14f, 0.14f);
    const float hudFpmY = std::clamp(
        (simulation.flightPathAngleDegrees - simulation.pitchDegrees) * 0.014f - 0.03f,
        -0.18f,
        0.18f);
    const float radarSweepDegrees = WrapDegrees(std::fmod(simulation.elapsedSeconds * 90.0f, 360.0f));
    const std::string headingText = FormatHeadingValue(simulation.headingDegrees);
    const std::string selectedHeadingText = FormatHeadingValue(simulation.selectedHeadingDegrees);
    const std::string speedText = FormatSpeedValue(simulation.speedKts);
    const std::string machText = FormatSignedFloat(simulation.speedKts / kKnotsPerMach, "%.2f");
    const std::string pitchText = FormatSignedFloat(simulation.pitchDegrees, "%+04.1f");
    const std::string rollText = FormatSignedFloat(simulation.bankDegrees, "%+03.0f");
    const std::string fpaText = FormatSignedFloat(simulation.flightPathAngleDegrees, "%+04.1f");
    const std::string throttleText = FormatPercentValue(simulation.throttle);
    const float headingBugRelativeDegrees =
        std::remainder(simulation.selectedHeadingDegrees - simulation.headingDegrees, 360.0f);

    ui.Run();
    auto& cockpit = ui.Cockpit();
    auto& contacts = cockpit.DynamicCockpitRadarContact();
    EnsureGeneratedDynamicReticles(contacts, generatedContacts, kCockpitTargets.size());

    const mfd::Vec2 adiBallPosition {kCockpitAdiCenterX, kCockpitAdiCenterY + adiPitchOffset};
    cockpit.adiBallSky.SetPosition(adiBallPosition);
    cockpit.adiBallSky.SetRotationDegrees(simulation.bankDegrees);
    cockpit.adiBallGround.SetPosition(adiBallPosition);
    cockpit.adiBallGround.SetRotationDegrees(simulation.bankDegrees);
    cockpit.adiBallHorizon.SetPosition(adiBallPosition);
    cockpit.adiBallHorizon.SetRotationDegrees(simulation.bankDegrees);
    cockpit.adiBallLadder.SetPosition(adiBallPosition);
    cockpit.adiBallLadder.SetRotationDegrees(simulation.bankDegrees);

    cockpit.adiHeadingBox.HeadingValue().SetText(headingText);
    cockpit.adiHeadingBox.CommandValue().SetText(selectedHeadingText);
    cockpit.adiHeadingCard.SetRotationDegrees(-simulation.headingDegrees);
    cockpit.adiHeadingCommandBug.SetRotationDegrees(headingBugRelativeDegrees);
    cockpit.adiPitchBox.SetValue(pitchText);
    cockpit.adiRollBox.SetValue(rollText);

    cockpit.hudPitchLadder.SetPosition(mfd::Vec2 {kCockpitHudCenterX, kCockpitHudCenterY + hudPitchOffset});
    cockpit.hudPitchLadder.SetRotationDegrees(simulation.bankDegrees * 0.88f);
    cockpit.hudVelocityVector.SetPosition(mfd::Vec2 {kCockpitHudCenterX + hudFpmX, kCockpitHudCenterY + hudFpmY});

    cockpit.hudSpeedBox.SetValue(speedText);
    cockpit.hudSpeedBox.SetColor(overspeed ? kHudWarning : kHudNominal);
    cockpit.hudMachBox.SetValue(machText);
    cockpit.hudMachBox.SetColor(overspeed ? kHudWarning : kHudNominal);

    if (overspeed)
    {
        cockpit.hudSpeedBox.Blink = cockpit.overspeed;
        cockpit.hudMachBox.Blink = cockpit.overspeed;
    }
    else
    {
        cockpit.hudSpeedBox.Blink = nullptr;
        cockpit.hudMachBox.Blink = nullptr;
    }

    cockpit.hudHeadingBox.SetValue(headingText);
    cockpit.hudFpaBox.SetValue(fpaText);
    cockpit.hudThrottleBox.SetValue(throttleText);
    cockpit.hudRadarBox.SetValue(simulation.radarEnabled ? std::string {"EMIT"} : std::string {"STBY"});
    cockpit.hudRadarBox.SetColor(simulation.radarEnabled ? kHudNominal : kHudWarning);

    cockpit.radarScope.SetVisible(simulation.radarEnabled);
    cockpit.radarSweep.SetVisible(simulation.radarEnabled);
    cockpit.radarSweep.SetPosition(mfd::Vec2 {kCockpitRadarCenterX, kCockpitRadarCenterY});
    cockpit.radarSweep.SetRotationDegrees(-radarSweepDegrees);
    cockpit.radarOwnship.SetVisible(simulation.radarEnabled);
    cockpit.radarHeadingBox.SetValue(headingText);
    cockpit.radarSpeedBox.SetValue(speedText);
    cockpit.radarStatusBox.SetValue(simulation.radarEnabled ? std::string {"SEARCH"} : std::string {"STANDBY"});
    cockpit.radarStatusBox.SetColor(simulation.radarEnabled ? kRadarSearch : kRadarStandby);
    cockpit.radarOffOverlay.SetVisible(!simulation.radarEnabled);

    for (std::size_t index = 0; index < generatedContacts.size(); ++index)
    {
        const float headingRadians = simulation.headingDegrees * kDegreesToRadians;
        const float cosine = std::cos(headingRadians);
        const float sine = std::sin(headingRadians);
        constexpr float kRadarRangeWorldUnits = 10.5f;
        constexpr float kRadarRadius = 0.33f;
        const CockpitTargetSeed& target = kCockpitTargets[index];
        DynamicContact& contact = *generatedContacts[index];

        const float orbitAngle = simulation.elapsedSeconds * target.orbitRate + target.orbitPhase;
        const float worldX = target.baseX + std::cos(orbitAngle) * target.orbitRadius;
        const float worldY = target.baseY + std::sin(orbitAngle) * target.orbitRadius;
        const float deltaX = worldX - simulation.ownshipX;
        const float deltaY = worldY - simulation.ownshipY;
        const float right = deltaX * cosine - deltaY * sine;
        const float forward = deltaX * sine + deltaY * cosine;
        const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        const bool visible = simulation.radarEnabled && distance <= kRadarRangeWorldUnits;
        const float normalizedX = std::clamp(right / kRadarRangeWorldUnits, -1.0f, 1.0f) * kRadarRadius;
        const float normalizedY = std::clamp(forward / kRadarRangeWorldUnits, -1.0f, 1.0f) * kRadarRadius;
        const float contactHeadingDegrees = std::atan2(right, forward) * kRadiansToDegrees;

        contact.SetVisible(visible);
        contact.SetPosition(mfd::Vec2 {kCockpitRadarCenterX + normalizedX, kCockpitRadarCenterY + normalizedY});
        contact.SetRotationDegrees(contactHeadingDegrees);
        contact.SetColor(target.color);
        contact.ContactLabel().SetText(std::string {target.label});

        if (target.blink && simulation.radarEnabled)
        {
            contact.Blink = cockpit.threat;
        }
        else
        {
            contact.Blink = nullptr;
        }
    }

}

/** @brief Converts a runtime RGBA color to an ImGui color vector. */
ImVec4 ToImGuiColor(const mfd::ColorRgba& color);
/** @brief Converts an ImGui color vector to a runtime RGBA color. */
mfd::ColorRgba ToColorRgba(const ImVec4& color);
/** @brief Returns the first text primitive of a reticle when one exists. */
const mfd::Primitive* FindFirstTextPrimitive(const mfd::ReticleGroup& reticle);
/** @brief Builds a human-readable label for the current navigation-tree selection. */
std::string SelectionLabel(const NavigationSelection& selection, const mfd::MfdDocument& document);
/** @brief Applies the custom visual style of the mockup operator UI. */
void ApplyMockupTheme();
/** @brief Draws one accent-colored button matching the mockup visual language. */
bool AccentButton(const char* label);

/**
 * @brief ImGui-based operator shell acting as a public UDP client for running MFD windows.
 *
 * @details `MockupApplication` is intentionally close to what a real external
 * client would do: it loads one authored window for discovery convenience,
 * builds a `CommandClient`, maintains editable drafts for pages and reticles,
 * sends typed commands and optionally receives strobe feedback from the target
 * window.
 */
class MockupApplication
{
public:
    /** @brief Builds the mockup shell and seeds the default dynamic-reticle draft. */
    MockupApplication();
    /** @brief Runs the full mockup UI until the operator closes the tool. */
    int Run();

private:
    /** @brief Reloads the selected target window file and refreshes all local drafts. */
    bool ReloadConfiguration();
    /** @brief Recreates the outbound UDP command client from the loaded window transport. */
    void RecreateClient();
    /** @brief Recreates the optional inbound strobe-feedback receiver. */
    void RecreateFeedbackReceiver();
    /** @brief Rebuilds the UDP transports after the window was detected closed. */
    void HandleWindowDisconnect();
    /** @brief Polls pending UDP feedback packets and updates the local cache. */
    void PollFeedback();
    /** @brief Returns a page by UI index. */
    const mfd::PageDefinition* PageByIndex(int pageIndex) const;
    /** @brief Returns a static reticle by page and UI index. */
    const mfd::ReticleGroup* ReticleByIndex(int pageIndex, int reticleIndex) const;
    /** @brief Returns the latest strobe feedback known for one page. */
    const StrobeFeedbackEntry* StrobeFeedbackForPage(int pageIndex) const;
    /** @brief Returns the editable whole-window display draft. */
    WindowDisplayDraft& EnsureWindowDisplayDraft();
    /** @brief Returns the editable draft associated with one page. */
    PageDraft& EnsurePageDraft(int pageIndex);
    /** @brief Loads one authored strobe into the editable page draft. */
    void LoadStrobeDraft(const mfd::PageDefinition& page, int strobeIndex, PageDraft& draft, bool preferLiveFeedback) const;
    /** @brief Returns the editable draft associated with one static reticle. */
    ReticleDraft& EnsureReticleDraft(int pageIndex, int reticleIndex);
    /** @brief Copies live strobe feedback into the related page draft. */
    void ApplyStrobeFeedbackToDraft(const mfd::StrobeStatusFeedback& feedback);
    /** @brief Restores the whole-window display draft from the loaded document defaults. */
    void ResetWindowDisplayDraft();
    /** @brief Restores the dynamic-reticle draft to a ready-to-send example state. */
    void ResetDynamicDraft();
    /** @brief Chooses a sane default page after reloading a target window. */
    void SelectDefaultPage();
    /** @brief Selects one page in the navigation tree. */
    void SelectPage(int pageIndex);
    /** @brief Selects one static reticle in the navigation tree. */
    void SelectReticle(int pageIndex, int reticleIndex);
    /** @brief Selects one authored strobe entry of one page in the navigation tree. */
    void SelectStrobe(int pageIndex, int strobeIndex);
    /** @brief Sends a page-activation command for the current selection. */
    bool SendActivatePage();
    /** @brief Sends the current page-view draft. */
    bool SendPageView();
    /** @brief Sends the current whole-window display draft. */
    bool SendWindowDisplayUpdate();
    /** @brief Sends a full runtime reset command to the connected target window. */
    bool SendResetWindow();
    /** @brief Sends the current static-reticle draft as a reticle patch. */
    bool SendReticleUpdate();
    /** @brief Sends the current strobe draft. */
    bool SendStrobeUpdate();
    /** @brief Creates or updates one dynamic reticle from the current draft. */
    bool SendDynamicUpsert();
    /** @brief Removes one dynamic reticle identified by the current draft id. */
    bool SendDynamicRemove();
    /** @brief Advances and publishes the 100-track radar simulation at its target cadence. */
    void UpdateRadarSimulation(float deltaSeconds);
    /** @brief Advances and publishes the cockpit demo simulation at its target cadence. */
    void UpdateCockpitSimulation(float deltaSeconds);
    /** @brief Integrates one step of the simple cockpit flight model. */
    void StepCockpitSimulation(float deltaSeconds);
    /** @brief Returns `true` when the selected target is the composite cockpit demo window. */
    bool IsCockpitDemoWindow() const;
    /** @brief Sends one radar-simulation batch or clears its generated tracks. */
    bool SendRadarSimulationBatch(bool clearOnly, bool quiet = false, bool initializeBeforeSend = false);
    /** @brief Sends one full cockpit frame batch built from the current simulation state. */
    bool SendCockpitSimulationBatch(bool quiet = false, bool initializeBeforeSend = false);
    /** @brief Activates the generated radar page used by the current radar simulator preset. */
    bool ActivateRadarSimulationPage();
    /** @brief Activates the generated cockpit page used by the cockpit simulator preset. */
    bool ActivateCockpitSimulationPage();
    /** @brief Restores the currently selected page draft from authored defaults. */
    void ResetSelectedPageDraft();
    /** @brief Restores the currently selected reticle draft from authored defaults. */
    void ResetSelectedReticleDraft();
    /** @brief Updates the footer status line. */
    void SetStatus(std::string message, bool isError);
    /** @brief Draws the full mockup shell. */
    void DrawUi();
    /** @brief Draws the top-level header bar and connection summary. */
    void DrawHeaderBar();
    /** @brief Draws the window/page/reticle navigation tree. */
    void DrawNavigationTree();
    /** @brief Draws the inspector matching the current selection. */
    void DrawInspector();
    /** @brief Draws whole-window display controls such as inversion and brightness. */
    void DrawWindowDisplayInspector();
    /** @brief Draws page-level controls such as view, quick strobe and blink declarations. */
    void DrawPageInspector();
    /** @brief Draws the reusable blink picker shared by static and dynamic reticle editors. */
    void DrawBlinkEditor(const mfd::PageDefinition& page,
                         const char* idSuffix,
                         bool& enabled,
                         std::string& blinkTypeName);
    /** @brief Draws the static-reticle inspector. */
    void DrawReticleInspector();
    /** @brief Draws the strobe inspector plus live feedback state. */
    void DrawStrobeInspector();
    /** @brief Draws the dynamic-reticle composer. */
    void DrawDynamicComposer();
    /** @brief Draws the radar stress-simulator controls and metrics. */
    void DrawRadarSimulationPanel();
    /** @brief Draws the simplified pilot-station banner used by the cockpit target preset. */
    void DrawCockpitPilotView();
    /** @brief Draws the cockpit simulation controls and live state. */
    void DrawCockpitSimulationPanel();
    /** @brief Draws the footer status bar. */
    void DrawStatusBar();
    /** @brief Returns the current shell uptime in seconds. */
    double TimeSeconds() const noexcept;

    /** @brief Built-in target presets exposed by the window-target combo box. */
    const std::array<WindowFilePreset, 3> knownWindowFiles_ {{
        {"Full Demo", std::filesystem::path {kFullDemoWindowFile}},
        {"Cockpit Demo", std::filesystem::path {kCockpitDemoWindowFile}},
        {"Minimal Radar", std::filesystem::path {kMinimalRadarWindowFile}},
    }};

    /** @brief Root window file currently targeted by the mockup. */
    std::filesystem::path windowFile_ = std::filesystem::path {kFullDemoWindowFile};
    /** @brief JSON loader used to inspect the target window description locally. */
    mfd::JsonLoader loader_ {};
    /** @brief Loaded authored configuration used to populate the operator UI. */
    mfd::LoadedWindowConfiguration loaded_ {};
    /** @brief Public outbound command client targeting the selected window. */
    std::unique_ptr<mfd::CommandClient> client_ {};
    /** @brief Dedicated realtime publisher used by the built-in simulators. */
    std::unique_ptr<mfd::client::LatestBatchPublisher> realtimePublisher_ {};
    /** @brief Persistent generated UI used by the full-demo radar simulator. */
    std::unique_ptr<full_demo_ui::FullDemoMockupUi> fullDemoGeneratedUi_ {};
    /** @brief Persistent generated UI used by the minimal-radar simulator. */
    std::unique_ptr<minimal_radar_ui::MinimalRadarMockupUi> minimalRadarGeneratedUi_ {};
    /** @brief Persistent generated UI used by the cockpit simulator. */
    std::unique_ptr<cockpit_demo_ui::CockpitDemoMockupUi> cockpitGeneratedUi_ {};
    /** @brief Optional inbound receiver for live strobe feedback. */
    std::unique_ptr<mfd::IExchangeChannel> feedbackReceiver_ {};
    /** @brief Detects window shutdown so the client can rebuild its transports. */
    mfd::client::WindowLivenessMonitor livenessMonitor_ {2.0};
    /** @brief Hidden generated handles used by the full-demo radar simulator. */
    std::vector<full_demo_ui::RadarTrackDynamicReticle*> fullDemoRadarTracks_ {};
    /** @brief Hidden generated handles used by the minimal-radar simulator. */
    std::vector<minimal_radar_ui::RadarTrackDynamicReticle*> minimalRadarRadarTracks_ {};
    /** @brief Hidden generated handles used by the cockpit simulator. */
    std::vector<cockpit_demo_ui::CockpitRadarContactDynamicReticle*> cockpitRadarContacts_ {};
    /** @brief Reticle template ids offered by the currently loaded library. */
    std::vector<std::string> templateIds_ {};
    /** @brief Per-page editable drafts keyed by normalized page name. */
    std::unordered_map<std::string, PageDraft> pageDrafts_ {};
    /** @brief Per-reticle editable drafts keyed by page-plus-reticle id. */
    std::unordered_map<std::string, ReticleDraft> reticleDrafts_ {};
    /** @brief Latest feedback snapshot known for each page exposing a strobe. */
    std::unordered_map<std::string, StrobeFeedbackEntry> strobeFeedbackByPage_ {};
    /** @brief Current navigation selection. */
    NavigationSelection selection_ {};
    /** @brief Editable whole-window display draft. */
    WindowDisplayDraft windowDisplayDraft_ {};
    /** @brief Editable dynamic-reticle draft. */
    DynamicDraft dynamicDraft_ {};
    /** @brief State of the built-in radar batch simulator. */
    RadarSimulationState radarSimulation_ {};
    /** @brief State of the built-in cockpit simulator. */
    CockpitSimulationState cockpitSimulation_ {};
    /** @brief Private Win32 + DX11 host shell used by the mockup UI. */
    Win32Dx11ImGuiHost host_ {};
    /** @brief Selected index inside the built-in window-target preset list. */
    int selectedWindowFileIndex_ = 0;
    /** @brief Selected index inside the dynamic-template combo box. */
    int selectedTemplateIndex_ = 0;
    /** @brief Human-readable status of the last action initiated by the operator. */
    std::string lastStatus_ {};
    /** @brief Human-readable status of the feedback receiver path. */
    std::string lastFeedbackStatus_ {};
    /** @brief Last runtime exception surfaced by the shell. */
    std::string lastRuntimeError_ {};
    /** @brief Indicates whether the main footer status is an error. */
    bool statusIsError_ = false;
};

MockupApplication::MockupApplication()
{
    ResetDynamicDraft();
}

} // namespace

int main()
{
#if defined(_WIN32)
    if (::IsDebuggerPresent() != 0)
    {
        MockupApplication application;
        return application.Run();
    }
#endif

    try
    {
        MockupApplication application;
        return application.Run();
    }
    catch (const std::exception&)
    {
        return 1;
    }
}

namespace
{
std::uint8_t ToColorChannelByte(const float value) noexcept
{
    return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

ImVec4 ToImGuiColor(const mfd::ColorRgba& color)
{
    return ImVec4(
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        static_cast<float>(color.a) / 255.0f);
}

mfd::ColorRgba ToColorRgba(const ImVec4& color)
{
    return mfd::ColorRgba {
        ToColorChannelByte(color.x),
        ToColorChannelByte(color.y),
        ToColorChannelByte(color.z),
        ToColorChannelByte(color.w)};
}

const mfd::Primitive* FindFirstTextPrimitive(const mfd::ReticleGroup& reticle)
{
    for (const auto& primitive : reticle.primitives)
    {
        if (primitive.type == mfd::PrimitiveType::Text)
        {
            return &primitive;
        }
    }

    return nullptr;
}

const mfd::PageStrobeDefinition* StrobeByIndex(const mfd::PageDefinition& page, int strobeIndex) noexcept;

std::string SelectionLabel(const NavigationSelection& selection, const mfd::MfdDocument& document)
{
    if (selection.pageIndex < 0 || selection.pageIndex >= static_cast<int>(document.pages.size()))
    {
        return "Nothing selected";
    }

    const auto& page = document.pages[static_cast<std::size_t>(selection.pageIndex)];

    switch (selection.kind)
    {
    case SelectionKind::Page:
        return page.title.empty() ? page.name : page.title;

    case SelectionKind::Strobe:
        if (const mfd::PageStrobeDefinition* strobe = StrobeByIndex(page, selection.reticleIndex); strobe != nullptr)
        {
            return page.name + " / " + strobe->name;
        }
        return page.name + " / Strobe";

    case SelectionKind::Reticle:
        if (selection.reticleIndex >= 0 &&
            selection.reticleIndex < static_cast<int>(page.staticReticles.size()))
        {
            const auto& reticle = page.staticReticles[static_cast<std::size_t>(selection.reticleIndex)];
            if (!reticle.id.empty())
            {
                return page.name + " / " + reticle.id;
            }

            return page.name + " / Reticle";
        }

        return page.name + " / Reticle";
    }

    return "Selection";
}

int ActiveStrobeIndex(const mfd::PageDefinition& page) noexcept
{
    if (page.strobes.empty())
    {
        return -1;
    }

    if (const mfd::PageStrobeDefinition* activeStrobe = mfd::FindActivePageStrobeDefinition(page);
        activeStrobe != nullptr)
    {
        for (std::size_t index = 0; index < page.strobes.size(); ++index)
        {
            if (&page.strobes[index] == activeStrobe)
            {
                return static_cast<int>(index);
            }
        }
    }

    return 0;
}

const mfd::PageStrobeDefinition* StrobeByIndex(const mfd::PageDefinition& page, const int strobeIndex) noexcept
{
    return strobeIndex >= 0 && strobeIndex < static_cast<int>(page.strobes.size())
               ? &page.strobes[static_cast<std::size_t>(strobeIndex)]
               : nullptr;
}

void ApplyMockupTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.WindowPadding = ImVec2(14.0f, 14.0f);
    style.FramePadding = ImVec2(10.0f, 8.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing = 18.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.53f, 0.62f, 0.69f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.08f, 0.11f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.11f, 0.15f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.16f, 0.24f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.25f, 0.31f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.19f, 0.31f, 0.38f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.09f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.09f, 0.12f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.07f, 0.11f, 0.15f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.10f, 0.13f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.18f, 0.29f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.23f, 0.38f, 0.45f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.27f, 0.47f, 0.56f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.86f, 0.78f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.72f, 0.83f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.33f, 0.86f, 0.78f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.12f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.33f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.40f, 0.48f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.11f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.31f, 0.37f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.39f, 0.46f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.18f, 0.28f, 0.34f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.19f, 0.42f, 0.49f, 0.35f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.24f, 0.62f, 0.68f, 0.78f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.29f, 0.76f, 0.80f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.18f, 0.23f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.17f, 0.31f, 0.38f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.15f, 0.27f, 0.34f, 1.00f);
}

bool AccentButton(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.54f, 0.61f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.66f, 0.73f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.44f, 0.52f, 1.00f));
    const bool pressed = ImGui::Button(label);
    ImGui::PopStyleColor(3);
    return pressed;
}

double MockupApplication::TimeSeconds() const noexcept
{
    return host_.TimeSeconds();
}

int MockupApplication::Run()
{
    if (!ReloadConfiguration())
    {
        throw std::runtime_error("Unable to load mockup window JSON: " + windowFile_.string());
    }

    std::string hostError;
    if (!host_.Initialize(
            Win32Dx11ImGuiHost::Config {1500, 920, 1180, 720, "Client Mockup"},
            hostError))
    {
        throw std::runtime_error("Unable to initialize the mockup shell: " + hostError);
    }

    ApplyMockupTheme();

    float deltaSeconds = 0.0f;
    while (host_.BeginFrame(deltaSeconds))
    {
        try
        {
            PollFeedback();
            UpdateRadarSimulation(deltaSeconds);
            UpdateCockpitSimulation(deltaSeconds);
        }
        catch (const std::exception& exception)
        {
            lastRuntimeError_ = exception.what();
        }
        catch (...)
        {
            lastRuntimeError_ = "Unknown exception while updating the mockup simulators";
        }

        host_.SetClearColor(8, 13, 18, 255);

        try
        {
            DrawUi();
        }
        catch (const std::exception& exception)
        {
            lastRuntimeError_ = exception.what();
        }
        catch (...)
        {
            lastRuntimeError_ = "Unknown exception while rendering the mockup";
        }

        host_.EndFrame();
    }

    return 0;
}

bool MockupApplication::ReloadConfiguration()
{
    try
    {
        loaded_ = loader_.LoadWindowConfiguration(windowFile_);

        templateIds_.clear();
        templateIds_.reserve(loaded_.document.reticleLibrary.size());
        for (const auto& entry : loaded_.document.reticleLibrary)
        {
            templateIds_.push_back(entry.first);
        }
        std::sort(templateIds_.begin(), templateIds_.end());

        if (selectedTemplateIndex_ >= static_cast<int>(templateIds_.size()))
        {
            selectedTemplateIndex_ = 0;
        }

        pageDrafts_.clear();
        reticleDrafts_.clear();
        strobeFeedbackByPage_.clear();
        ResetWindowDisplayDraft();
        ResetDynamicDraft();
        radarSimulation_ = {};
        cockpitSimulation_ = {};
        fullDemoGeneratedUi_ = MakeInitializedGeneratedUi<full_demo_ui::FullDemoMockupUi>();
        minimalRadarGeneratedUi_ = MakeInitializedGeneratedUi<minimal_radar_ui::MinimalRadarMockupUi>();
        cockpitGeneratedUi_ = MakeInitializedGeneratedUi<cockpit_demo_ui::CockpitDemoMockupUi>();
        fullDemoRadarTracks_.clear();
        minimalRadarRadarTracks_.clear();
        cockpitRadarContacts_.clear();
        SelectDefaultPage();
        RecreateClient();
        RecreateFeedbackReceiver();
        lastRuntimeError_.clear();

        if (client_ == nullptr || !client_->IsReady())
        {
            return true;
        }

        if (const auto* page = PageByIndex(selection_.pageIndex))
        {
            SetStatus(
                "Configuration loaded for '" + loaded_.window.title + "'. Active editor page: " + page->name,
                false);
        }
        else
        {
            SetStatus("Configuration loaded for '" + loaded_.window.title + "'.", false);
        }

        return true;
    }
    catch (const std::exception& exception)
    {
        loaded_ = {};
        templateIds_.clear();
        realtimePublisher_.reset();
        client_.reset();
        fullDemoGeneratedUi_.reset();
        minimalRadarGeneratedUi_.reset();
        cockpitGeneratedUi_.reset();
        fullDemoRadarTracks_.clear();
        minimalRadarRadarTracks_.clear();
        cockpitRadarContacts_.clear();
        SetStatus("Unable to load '" + windowFile_.string() + "': " + exception.what(), true);
        return false;
    }
}

void MockupApplication::RecreateClient()
{
    realtimePublisher_.reset();
    client_.reset();

    if (!loaded_.window.commandTransports.udp.has_value() ||
        !loaded_.window.commandTransports.udp->enabled)
    {
        SetStatus("No UDP command transport is configured in the selected window JSON.", true);
        return;
    }
    if (!loaded_.generatedTransportMap.has_value())
    {
        SetStatus("No generated transport map was found next to the selected window JSON.", true);
        return;
    }

    client_ = std::make_unique<mfd::CommandClient>(loaded_.window.commandTransports, loaded_.generatedTransportMap);

    if (client_ == nullptr || !client_->IsReady())
    {
        const std::string error =
            client_ == nullptr ? "Unable to create the command client" : client_->LastError();
        SetStatus("UDP command transport unavailable: " + error, true);
        return;
    }

    mfd::CommandClient* const realtimeClient = client_.get();
    realtimePublisher_ = std::make_unique<mfd::client::LatestBatchPublisher>(
        [realtimeClient](const mfd::CommandBatch& batch)
        {
            return realtimeClient != nullptr && realtimeClient->SendBatch(batch);
        },
        [realtimeClient]()
        {
            return realtimeClient == nullptr ? std::string {"Realtime command client is unavailable"}
                                             : realtimeClient->LastError();
        });

    SetStatus("Connected through UDP to '" + loaded_.window.title + "'.", false);
}

void MockupApplication::RecreateFeedbackReceiver()
{
    feedbackReceiver_.reset();
    lastFeedbackStatus_.clear();

    if (!loaded_.window.feedbackTransports.udp.has_value() ||
        !loaded_.window.feedbackTransports.udp->enabled)
    {
        lastFeedbackStatus_ = "No UDP strobe feedback transport is configured.";
        return;
    }

    feedbackReceiver_ = mfd::CreateFeedbackReceiverChannel(loaded_.window.feedbackTransports);
    if (feedbackReceiver_ == nullptr || !feedbackReceiver_->IsReady())
    {
        lastFeedbackStatus_ =
            feedbackReceiver_ == nullptr
                ? "Unable to create the feedback receiver"
                : feedbackReceiver_->LastError();
        return;
    }

    lastFeedbackStatus_ = "Listening for UDP strobe feedback.";
}

void MockupApplication::PollFeedback()
{
    if (feedbackReceiver_ == nullptr || !feedbackReceiver_->IsReady())
    {
        return;
    }

    constexpr int kMaxFeedbackPacketsPerFrame = 32;

    for (int packetIndex = 0; packetIndex < kMaxFeedbackPacketsPerFrame; ++packetIndex)
    {
        const auto payload = feedbackReceiver_->TryReceive();
        if (!payload.has_value())
        {
            break;
        }

        const auto* raw = reinterpret_cast<const char*>(payload->data());
        std::string error;
        const auto decoded =
            mfd::DeserializeFeedbackPayload(std::string_view(raw, payload->size()), &error);
        if (!decoded.has_value())
        {
            lastFeedbackStatus_ = error.empty() ? "Unable to decode strobe feedback" : std::move(error);
            continue;
        }

        // Any decoded packet proves the window is alive.
        livenessMonitor_.RecordActivity(TimeSeconds());

        if (const auto* lifecycle = std::get_if<mfd::WindowLifecycleFeedback>(&(*decoded)); lifecycle != nullptr)
        {
            if (lifecycle->state == mfd::WindowLifecycleState::Closing)
            {
                livenessMonitor_.RecordWindowClosing(TimeSeconds());
            }

            continue;
        }

        const auto* feedback = std::get_if<mfd::StrobeStatusFeedback>(&(*decoded));
        if (feedback == nullptr)
        {
            const auto* activePage = std::get_if<mfd::ActivePageFeedback>(&(*decoded));
            if (activePage != nullptr)
            {
                lastFeedbackStatus_ = "Active page feedback received for page '" + activePage->pageName + "'.";
            }

            continue;
        }

        const std::string normalizedPageName = mfd::NormalizePageName(feedback->pageName);
        strobeFeedbackByPage_[normalizedPageName] = StrobeFeedbackEntry {*feedback, TimeSeconds()};
        ApplyStrobeFeedbackToDraft(*feedback);
        lastFeedbackStatus_ = "Strobe feedback received for page '" + feedback->pageName + "'.";
    }

    if (!feedbackReceiver_->LastError().empty())
    {
        lastFeedbackStatus_ = feedbackReceiver_->LastError();
    }

    livenessMonitor_.Update(TimeSeconds());
    if (livenessMonitor_.ConsumeDisconnect())
    {
        HandleWindowDisconnect();
    }
}

void MockupApplication::HandleWindowDisconnect()
{
    strobeFeedbackByPage_.clear();
    RecreateClient();
    RecreateFeedbackReceiver();
    livenessMonitor_.Reset();
    SetStatus("Window connection closed or lost. Rebuilt the UDP transports; waiting for the window to return.",
              false);
}

const mfd::PageDefinition* MockupApplication::PageByIndex(const int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        return nullptr;
    }

    return &loaded_.document.pages[static_cast<std::size_t>(pageIndex)];
}

const mfd::ReticleGroup* MockupApplication::ReticleByIndex(const int pageIndex, const int reticleIndex) const
{
    const mfd::PageDefinition* page = PageByIndex(pageIndex);
    if (page == nullptr || reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
    {
        return nullptr;
    }

    return &page->staticReticles[static_cast<std::size_t>(reticleIndex)];
}

const StrobeFeedbackEntry* MockupApplication::StrobeFeedbackForPage(const int pageIndex) const
{
    const mfd::PageDefinition* page = PageByIndex(pageIndex);
    if (page == nullptr)
    {
        return nullptr;
    }

    const std::string normalizedPageName =
        page->normalizedName.empty() ? mfd::NormalizePageName(page->name) : page->normalizedName;
    const auto iterator = strobeFeedbackByPage_.find(normalizedPageName);
    return iterator == strobeFeedbackByPage_.end() ? nullptr : &iterator->second;
}

WindowDisplayDraft& MockupApplication::EnsureWindowDisplayDraft()
{
    return windowDisplayDraft_;
}

void MockupApplication::LoadStrobeDraft(const mfd::PageDefinition& page,
                                        const int strobeIndex,
                                        PageDraft& draft,
                                        const bool preferLiveFeedback) const
{
    const mfd::PageStrobeDefinition* strobe = StrobeByIndex(page, strobeIndex);
    if (strobe == nullptr)
    {
        draft.selectedStrobeIndex = -1;
        draft.hasStrobe = false;
        return;
    }

    draft.hasStrobe = true;
    draft.selectedStrobeIndex = strobeIndex;
    draft.strobeEnabled = strobe->reticle.visible;
    draft.strobePosition = {
        strobe->reticle.transform.position.x,
        strobe->reticle.transform.position.y};

    if (!preferLiveFeedback)
    {
        return;
    }

    const auto feedbackIterator = strobeFeedbackByPage_.find(
        page.normalizedName.empty() ? mfd::NormalizePageName(page.name) : page.normalizedName);
    if (feedbackIterator == strobeFeedbackByPage_.end())
    {
        return;
    }

    const int activeStrobeIndex = ActiveStrobeIndex(page);
    if (activeStrobeIndex != strobeIndex)
    {
        return;
    }

    draft.strobeEnabled = feedbackIterator->second.feedback.active;
    draft.strobePosition = {
        feedbackIterator->second.feedback.position.x,
        feedbackIterator->second.feedback.position.y};
}

PageDraft& MockupApplication::EnsurePageDraft(const int pageIndex)
{
    const mfd::PageDefinition* page = PageByIndex(pageIndex);
    if (page == nullptr)
    {
        static PageDraft fallback;
        return fallback;
    }

    const std::string key = page->normalizedName.empty() ? mfd::NormalizePageName(page->name) : page->normalizedName;
    auto [it, inserted] = pageDrafts_.try_emplace(key);

    if (inserted)
    {
        PageDraft draft;
        draft.center = {page->view.center.x, page->view.center.y};
        draft.zoom = page->view.zoom;
        draft.hasStrobe = !page->strobes.empty();

        const int preferredStrobeIndex =
            selection_.kind == SelectionKind::Strobe && selection_.pageIndex == pageIndex
                ? selection_.reticleIndex
                : ActiveStrobeIndex(*page);
        LoadStrobeDraft(*page, preferredStrobeIndex, draft, true);

        it->second = draft;
    }

    return it->second;
}

ReticleDraft& MockupApplication::EnsureReticleDraft(const int pageIndex, const int reticleIndex)
{
    const mfd::PageDefinition* page = PageByIndex(pageIndex);
    const mfd::ReticleGroup* reticle = ReticleByIndex(pageIndex, reticleIndex);
    if (page == nullptr || reticle == nullptr)
    {
        static ReticleDraft fallback;
        return fallback;
    }

    const std::string pageKey = page->normalizedName.empty() ? mfd::NormalizePageName(page->name) : page->normalizedName;
    const std::string reticleKey = pageKey + ":" + mfd::NormalizePageName(reticle->id);
    auto [it, inserted] = reticleDrafts_.try_emplace(reticleKey);

    if (inserted)
    {
        ReticleDraft draft;
        draft.visible = reticle->visible;
        draft.blinkEnabled = reticle->blink.enabled;
        draft.blinkTypeName = reticle->blink.typeName;
        draft.position = {reticle->transform.position.x, reticle->transform.position.y};
        draft.rotation = reticle->transform.rotationDegrees;

        if (reticle->overrides.thickness.has_value())
        {
            draft.thickness = *reticle->overrides.thickness;
        }
        else if (!reticle->primitives.empty())
        {
            draft.thickness = reticle->primitives.front().style.thickness;
        }

        if (reticle->overrides.color.has_value())
        {
            draft.color = ToImGuiColor(*reticle->overrides.color);
        }
        else if (!reticle->primitives.empty())
        {
            draft.color = ToImGuiColor(reticle->primitives.front().style.color);
        }

        if (const mfd::Primitive* textPrimitive = FindFirstTextPrimitive(*reticle))
        {
            if (const auto* geometry = std::get_if<mfd::TextGeometry>(&textPrimitive->geometry))
            {
                draft.hasText = true;
                draft.letterSpacing = geometry->letterSpacing;
                CopyTextBuffer(draft.text, geometry->text);
            }
        }

        it->second = draft;
    }

    return it->second;
}

void MockupApplication::ApplyStrobeFeedbackToDraft(const mfd::StrobeStatusFeedback& feedback)
{
    const std::string normalizedPageName = mfd::NormalizePageName(feedback.pageName);
    auto draftIterator = pageDrafts_.find(normalizedPageName);
    if (draftIterator == pageDrafts_.end())
    {
        return;
    }

    // Keep the live feedback separate from the editable draft so the operator
    // can manipulate the strobe without the 20 ms feedback loop overwriting
    // the values being edited.
    draftIterator->second.hasStrobe = true;
}

void MockupApplication::ResetWindowDisplayDraft()
{
    windowDisplayDraft_ = {};
}

void MockupApplication::ResetDynamicDraft()
{
    dynamicDraft_ = {};
    CopyTextBuffer(dynamicDraft_.id, "dynamic_track_01");
    dynamicDraft_.visible = true;
    dynamicDraft_.blinkEnabled = false;
    dynamicDraft_.blinkTypeName.clear();
    dynamicDraft_.position = {0.25f, 0.15f};
    dynamicDraft_.rotation = 0.0f;
    dynamicDraft_.thickness = 0.0042f;
    dynamicDraft_.color = ImVec4(0.30f, 0.88f, 1.00f, 1.00f);
    dynamicDraft_.letterSpacing = mfd::kDefaultTextLetterSpacing;
    CopyTextBuffer(dynamicDraft_.text, "MOCK");
}

bool MockupApplication::IsCockpitDemoWindow() const
{
    return std::any_of(
        loaded_.document.pages.begin(),
        loaded_.document.pages.end(),
        [](const mfd::PageDefinition& page)
        {
            return mfd::PageNamesEqual(page.name, kCockpitPageName);
        });
}

void MockupApplication::SelectDefaultPage()
{
    if (loaded_.document.pages.empty())
    {
        selection_ = {};
        return;
    }

    for (int index = 0; index < static_cast<int>(loaded_.document.pages.size()); ++index)
    {
        if (loaded_.document.pages[static_cast<std::size_t>(index)].defaultPage)
        {
            SelectPage(index);
            return;
        }
    }

    SelectPage(0);
}

void MockupApplication::SelectPage(const int pageIndex)
{
    selection_.kind = SelectionKind::Page;
    selection_.pageIndex = pageIndex;
    selection_.reticleIndex = -1;
    EnsurePageDraft(pageIndex);
}

void MockupApplication::SelectReticle(const int pageIndex, const int reticleIndex)
{
    selection_.kind = SelectionKind::Reticle;
    selection_.pageIndex = pageIndex;
    selection_.reticleIndex = reticleIndex;
    EnsureReticleDraft(pageIndex, reticleIndex);
}

void MockupApplication::SelectStrobe(const int pageIndex, const int strobeIndex)
{
    selection_.kind = SelectionKind::Strobe;
    selection_.pageIndex = pageIndex;
    selection_.reticleIndex = strobeIndex;
    PageDraft& draft = EnsurePageDraft(pageIndex);
    if (const mfd::PageDefinition* page = PageByIndex(pageIndex); page != nullptr)
    {
        LoadStrobeDraft(*page, strobeIndex, draft, true);
    }
}

bool MockupApplication::SendActivatePage()
{
    const mfd::PageDefinition* page = PageByIndex(selection_.pageIndex);
    if (page == nullptr)
    {
        SetStatus("No page selected.", true);
        return false;
    }

    if (client_ == nullptr || !client_->IsReady())
    {
        SetStatus("Command client is not ready.", true);
        return false;
    }

    if (!client_->ActivatePage(page->name))
    {
        SetStatus("Unable to activate page '" + page->name + "': " + client_->LastError(), true);
        return false;
    }

    SetStatus("Page '" + page->name + "' activated.", false);
    return true;
}

bool MockupApplication::SendPageView()
{
    const mfd::PageDefinition* page = PageByIndex(selection_.pageIndex);
    if (page == nullptr)
    {
        SetStatus("No page selected.", true);
        return false;
    }

    if (client_ == nullptr || !client_->IsReady())
    {
        SetStatus("Command client is not ready.", true);
        return false;
    }

    PageDraft& draft = EnsurePageDraft(selection_.pageIndex);
    draft.center[0] = std::clamp(draft.center[0], -1.0f, 1.0f);
    draft.center[1] = std::clamp(draft.center[1], -1.0f, 1.0f);
    draft.zoom = mfd::SanitizeZoom(draft.zoom);

    if (!client_->SetPageView(page->name, mfd::Vec2 {draft.center[0], draft.center[1]}, draft.zoom))
    {
        SetStatus("Unable to update page view for '" + page->name + "': " + client_->LastError(), true);
        return false;
    }

    SetStatus("View updated for page '" + page->name + "'.", false);
    return true;
}

bool MockupApplication::SendWindowDisplayUpdate()
{
    if (client_ == nullptr || !client_->IsReady())
    {
        SetStatus("Command client is not ready.", true);
        return false;
    }

    WindowDisplayDraft& draft = EnsureWindowDisplayDraft();
    draft.brightness = std::clamp(draft.brightness, 0.0f, 1.0f);

    mfd::WindowDisplayPatch patch;
    patch.invertColors = draft.invertColors;
    patch.brightness = draft.brightness;
    patch.disabled = draft.disabled;

    if (!client_->UpdateWindowDisplay(patch))
    {
        SetStatus("Unable to update whole-window display properties: " + client_->LastError(), true);
        return false;
    }

    SetStatus("Whole-window display updated on '" + loaded_.window.title + "'.", false);
    return true;
}

bool MockupApplication::SendResetWindow()
{
    if (client_ == nullptr || !client_->IsReady())
    {
        SetStatus("Command client is not ready.", true);
        return false;
    }

    if (!client_->ResetWindow())
    {
        SetStatus("Unable to reset target window: " + client_->LastError(), true);
        return false;
    }

    ResetWindowDisplayDraft();
    ResetDynamicDraft();

    switch (selection_.kind)
    {
    case SelectionKind::Page:
    case SelectionKind::Strobe:
        ResetSelectedPageDraft();
        break;
    case SelectionKind::Reticle:
        ResetSelectedReticleDraft();
        break;
    default:
        break;
    }

    SetStatus("Target window reset command sent.", false);
    return true;
}

bool MockupApplication::SendReticleUpdate()
{
    const mfd::PageDefinition* page = PageByIndex(selection_.pageIndex);
    const mfd::ReticleGroup* reticle = ReticleByIndex(selection_.pageIndex, selection_.reticleIndex);
    if (page == nullptr || reticle == nullptr)
    {
        SetStatus("No reticle selected.", true);
        return false;
    }

    if (reticle->id.empty())
    {
        SetStatus("The selected reticle has no public id and cannot be edited from the mockup.", true);
        return false;
    }

    if (client_ == nullptr || !client_->IsReady())
    {
        SetStatus("Command client is not ready.", true);
        return false;
    }

    ReticleDraft& draft = EnsureReticleDraft(selection_.pageIndex, selection_.reticleIndex);
    draft.position[0] = std::clamp(draft.position[0], -1.0f, 1.0f);
    draft.position[1] = std::clamp(draft.position[1], -1.0f, 1.0f);
    draft.thickness = std::max(0.0005f, draft.thickness);

    mfd::ReticlePatch patch;
    patch.visible = draft.visible;
    patch.blinkEnabled = draft.blinkEnabled;
    patch.blinkType = draft.blinkTypeName;
    patch.position = mfd::Vec2 {draft.position[0], draft.position[1]};
    patch.rotationDegrees = draft.rotation;
    patch.color = ToColorRgba(draft.color);
    patch.thickness = draft.thickness;

    if (draft.hasText)
    {
        patch.text = std::string(draft.text.data());
        patch.letterSpacing = draft.letterSpacing;
    }

    if (!client_->UpdateReticle(page->name, reticle->id, patch))
    {
        SetStatus(
            "Unable to update reticle '" + reticle->id + "' on page '" + page->name + "': " + client_->LastError(),
            true);
        return false;
    }

    SetStatus("Reticle '" + reticle->id + "' updated on page '" + page->name + "'.", false);
    return true;
}

bool MockupApplication::SendStrobeUpdate()
{
    const mfd::PageDefinition* page = PageByIndex(selection_.pageIndex);
    if (page == nullptr)
    {
        SetStatus("No page selected.", true);
        return false;
    }

    PageDraft& draft = EnsurePageDraft(selection_.pageIndex);
    if (!draft.hasStrobe)
    {
        SetStatus("The selected page has no strobe.", true);
        return false;
    }

    if (client_ == nullptr || !client_->IsReady())
    {
        SetStatus("Command client is not ready.", true);
        return false;
    }

    draft.strobePosition[0] = std::clamp(draft.strobePosition[0], -1.0f, 1.0f);
    draft.strobePosition[1] = std::clamp(draft.strobePosition[1], -1.0f, 1.0f);

    const mfd::PageStrobeDefinition* strobe = StrobeByIndex(*page, draft.selectedStrobeIndex);
    if (strobe == nullptr)
    {
        SetStatus("No authored strobe is currently selected on page '" + page->name + "'.", true);
        return false;
    }

    mfd::UpdateStrobeCommand command;
    command.page = page->name;
    command.strobe = strobe->name;
    command.active = draft.strobeEnabled;
    command.position = mfd::Vec2 {draft.strobePosition[0], draft.strobePosition[1]};
    if (!client_->Send(command))
    {
        SetStatus("Unable to update strobe on page '" + page->name + "': " + client_->LastError(), true);
        return false;
    }

    SetStatus("Strobe '" + strobe->name + "' updated on page '" + page->name + "'.", false);
    return true;
}

bool MockupApplication::SendDynamicUpsert()
{
    const mfd::PageDefinition* page = PageByIndex(selection_.pageIndex);
    if (page == nullptr)
    {
        SetStatus("Select a page before sending a dynamic reticle.", true);
        return false;
    }

    if (client_ == nullptr || !client_->IsReady())
    {
        SetStatus("Command client is not ready.", true);
        return false;
    }

    if (templateIds_.empty() || selectedTemplateIndex_ < 0 || selectedTemplateIndex_ >= static_cast<int>(templateIds_.size()))
    {
        SetStatus("No reticle template is available for dynamic creation.", true);
        return false;
    }

    const std::string reticleId = dynamicDraft_.id.data();
    if (reticleId.empty())
    {
        SetStatus("Dynamic reticle id cannot be empty.", true);
        return false;
    }

    dynamicDraft_.position[0] = std::clamp(dynamicDraft_.position[0], -1.0f, 1.0f);
    dynamicDraft_.position[1] = std::clamp(dynamicDraft_.position[1], -1.0f, 1.0f);
    dynamicDraft_.thickness = std::max(0.0005f, dynamicDraft_.thickness);

    mfd::ReticlePatch patch;
    patch.visible = dynamicDraft_.visible;
    patch.blinkEnabled = dynamicDraft_.blinkEnabled;
    patch.blinkType = dynamicDraft_.blinkTypeName;
    patch.position = mfd::Vec2 {dynamicDraft_.position[0], dynamicDraft_.position[1]};
    patch.rotationDegrees = dynamicDraft_.rotation;
    patch.color = ToColorRgba(dynamicDraft_.color);
    patch.thickness = dynamicDraft_.thickness;

    const std::string text = dynamicDraft_.text.data();
    if (!text.empty())
    {
        patch.text = text;
        patch.letterSpacing = dynamicDraft_.letterSpacing;
    }

    const std::string& templateId = templateIds_[static_cast<std::size_t>(selectedTemplateIndex_)];
    if (!client_->UpsertDynamicReticle(page->name, reticleId, templateId, patch))
    {
        SetStatus(
            "Unable to upsert dynamic reticle '" + reticleId + "' on page '" + page->name + "': " + client_->LastError(),
            true);
        return false;
    }

    SetStatus(
        "Dynamic reticle '" + reticleId + "' sent to page '" + page->name + "' using template '" + templateId + "'.",
        false);
    return true;
}

bool MockupApplication::SendDynamicRemove()
{
    const mfd::PageDefinition* page = PageByIndex(selection_.pageIndex);
    if (page == nullptr)
    {
        SetStatus("Select a page before removing a dynamic reticle.", true);
        return false;
    }

    if (client_ == nullptr || !client_->IsReady())
    {
        SetStatus("Command client is not ready.", true);
        return false;
    }

    const std::string reticleId = dynamicDraft_.id.data();
    if (reticleId.empty())
    {
        SetStatus("Dynamic reticle id cannot be empty.", true);
        return false;
    }

    if (!client_->RemoveDynamicReticle(page->name, reticleId))
    {
        SetStatus(
            "Unable to remove dynamic reticle '" + reticleId + "' from page '" + page->name + "': " + client_->LastError(),
            true);
        return false;
    }

    SetStatus("Dynamic reticle '" + reticleId + "' removed from page '" + page->name + "'.", false);
    return true;
}

void MockupApplication::UpdateRadarSimulation(const float deltaSeconds)
{
    if (radarSimulation_.clearRequested)
    {
        SendRadarSimulationBatch(true, true);
        radarSimulation_.clearRequested = false;
    }

    if (!radarSimulation_.enabled)
    {
        return;
    }

    radarSimulation_.accumulatorSeconds = std::min(
        radarSimulation_.accumulatorSeconds + std::max(deltaSeconds, 0.0f),
        kRadarSimulationIntervalSeconds * 5.0f);

    if (radarSimulation_.accumulatorSeconds < kRadarSimulationIntervalSeconds)
    {
        return;
    }

    const int simulatedSteps = std::max(
        1,
        static_cast<int>(radarSimulation_.accumulatorSeconds / kRadarSimulationIntervalSeconds));
    radarSimulation_.accumulatorSeconds =
        std::fmod(radarSimulation_.accumulatorSeconds, kRadarSimulationIntervalSeconds);
    radarSimulation_.elapsedSeconds +=
        static_cast<float>(simulatedSteps) * kRadarSimulationIntervalSeconds;

    if (!SendRadarSimulationBatch(false, true))
    {
        radarSimulation_.enabled = false;
        radarSimulation_.accumulatorSeconds = 0.0f;
        return;
    }
}

void MockupApplication::StepCockpitSimulation(const float deltaSeconds)
{
    CockpitSimulationState& simulation = cockpitSimulation_;

    const float pitchInput = std::clamp(simulation.pitchControlInput, -1.0f, 1.0f);
    const float rollInput = std::clamp(simulation.rollControlInput, -1.0f, 1.0f);

    simulation.pitchDegrees = std::clamp(
        simulation.pitchDegrees + pitchInput * 24.0f * deltaSeconds,
        -85.0f,
        85.0f);

    const float targetBankDegrees = rollInput * 58.0f;
    simulation.bankDegrees = Approach(
        simulation.bankDegrees,
        targetBankDegrees,
        125.0f * deltaSeconds);

    simulation.selectedHeadingDegrees = WrapDegrees(
        simulation.selectedHeadingDegrees + rollInput * 28.0f * deltaSeconds);

    const float bankRadians = simulation.bankDegrees * kDegreesToRadians;
    const float turnRateDegreesPerSecond =
        std::sin(bankRadians) * std::clamp(8.0f + simulation.speedKts * 0.028f, 8.0f, 30.0f);

    simulation.headingDegrees = WrapDegrees(
        simulation.headingDegrees +
        turnRateDegreesPerSecond * deltaSeconds);

    const float targetSpeedKts =
        std::clamp(190.0f + simulation.throttle * 690.0f -
                       std::max(simulation.pitchDegrees, 0.0f) * 1.3f -
                       std::abs(simulation.bankDegrees) * 0.18f,
                   160.0f,
                   860.0f);
    simulation.speedKts += (targetSpeedKts - simulation.speedKts) * std::min(deltaSeconds * 1.8f, 1.0f);

    const float targetFlightPathAngle =
        std::clamp(simulation.pitchDegrees * 0.70f + (simulation.throttle - 0.52f) * 5.8f -
                       std::abs(simulation.bankDegrees) * 0.04f,
                   -18.0f,
                   20.0f);
    simulation.flightPathAngleDegrees +=
        (targetFlightPathAngle - simulation.flightPathAngleDegrees) * std::min(deltaSeconds * 2.5f, 1.0f);

    const float headingRadians = simulation.headingDegrees * kDegreesToRadians;
    const float speedWorldUnitsPerSecond = simulation.speedKts * 0.00045f;
    simulation.ownshipX += std::sin(headingRadians) * speedWorldUnitsPerSecond * deltaSeconds;
    simulation.ownshipY += std::cos(headingRadians) * speedWorldUnitsPerSecond * deltaSeconds;
    simulation.elapsedSeconds += deltaSeconds;
}

void MockupApplication::UpdateCockpitSimulation(const float deltaSeconds)
{
    if (!cockpitSimulation_.enabled)
    {
        return;
    }

    cockpitSimulation_.accumulatorSeconds = std::min(
        cockpitSimulation_.accumulatorSeconds + std::max(deltaSeconds, 0.0f),
        kCockpitSimulationIntervalSeconds * 5.0f);

    if (cockpitSimulation_.accumulatorSeconds < kCockpitSimulationIntervalSeconds)
    {
        return;
    }

    const int simulatedSteps = std::max(
        1,
        static_cast<int>(cockpitSimulation_.accumulatorSeconds / kCockpitSimulationIntervalSeconds));
    cockpitSimulation_.accumulatorSeconds =
        std::fmod(cockpitSimulation_.accumulatorSeconds, kCockpitSimulationIntervalSeconds);

    for (int stepIndex = 0; stepIndex < simulatedSteps; ++stepIndex)
    {
        StepCockpitSimulation(kCockpitSimulationIntervalSeconds);
    }

    if (!SendCockpitSimulationBatch(true))
    {
        cockpitSimulation_.enabled = false;
        cockpitSimulation_.accumulatorSeconds = 0.0f;
    }
}

bool MockupApplication::SendRadarSimulationBatch(const bool clearOnly,
                                                 const bool quiet,
                                                 const bool initializeBeforeSend)
{
    if (realtimePublisher_ == nullptr || !realtimePublisher_->IsReady())
    {
        SetStatus("Realtime publisher is not ready for the radar simulator.", true);
        return false;
    }

    const bool hasRadarPage = std::any_of(
        loaded_.document.pages.begin(),
        loaded_.document.pages.end(),
        [](const mfd::PageDefinition& page)
        {
            return mfd::PageNamesEqual(page.name, kRadarSimulationPageName);
        });

    if (!hasRadarPage)
    {
        SetStatus("The selected window has no page named 'Radar'.", true);
        return false;
    }

    if (!clearOnly && loaded_.document.reticleLibrary.find(std::string(kRadarSimulationTemplateId)) == loaded_.document.reticleLibrary.end())
    {
        SetStatus("The reticle library does not expose the 'radar_track' template.", true);
        return false;
    }

    mfd::CommandBatch batch;

    switch (DetectWindowPresetKind(windowFile_))
    {
    case WindowPresetKind::FullDemo:
        if (fullDemoGeneratedUi_ == nullptr)
        {
            fullDemoGeneratedUi_ = MakeInitializedGeneratedUi<full_demo_ui::FullDemoMockupUi>();
        }
        if (initializeBeforeSend)
        {
            fullDemoGeneratedUi_->Initialize();
        }
        if (clearOnly)
        {
            RemoveGeneratedDynamicReticles(
                fullDemoGeneratedUi_->Radar().DynamicRadarTrack(),
                fullDemoRadarTracks_);
        }
        else
        {
            PopulateRadarSimulationUi(
                *fullDemoGeneratedUi_,
                fullDemoRadarTracks_,
                radarSimulation_.elapsedSeconds);
        }
        batch = fullDemoGeneratedUi_->BuildCommandBatch(radarSimulation_.nextSequence);
        break;
    case WindowPresetKind::MinimalRadar:
        if (minimalRadarGeneratedUi_ == nullptr)
        {
            minimalRadarGeneratedUi_ = MakeInitializedGeneratedUi<minimal_radar_ui::MinimalRadarMockupUi>();
        }
        if (initializeBeforeSend)
        {
            minimalRadarGeneratedUi_->Initialize();
        }
        if (clearOnly)
        {
            RemoveGeneratedDynamicReticles(
                minimalRadarGeneratedUi_->Radar().DynamicRadarTrack(),
                minimalRadarRadarTracks_);
        }
        else
        {
            PopulateRadarSimulationUi(
                *minimalRadarGeneratedUi_,
                minimalRadarRadarTracks_,
                radarSimulation_.elapsedSeconds);
        }
        batch = minimalRadarGeneratedUi_->BuildCommandBatch(radarSimulation_.nextSequence);
        break;
    default:
        SetStatus("No generated radar client API is available for the selected window preset.", true);
        return false;
    }

    const double sendStart = TimeSeconds();
    const int commandCount = static_cast<int>(batch.commands.size());
    const bool submitted = realtimePublisher_->SubmitLatest(std::move(batch));
    const double sendEnd = TimeSeconds();

    radarSimulation_.lastSendDurationMs = static_cast<float>((sendEnd - sendStart) * 1000.0);
    radarSimulation_.lastCommandCount = commandCount;

    if (radarSimulation_.previousSendTimestamp > 0.0)
    {
        radarSimulation_.lastObservedCycleMs =
            static_cast<float>((sendStart - radarSimulation_.previousSendTimestamp) * 1000.0);
    }

    radarSimulation_.previousSendTimestamp = sendStart;

    if (!submitted)
    {
        SetStatus(
            clearOnly
                ? "Unable to clear simulated radar tracks: " + realtimePublisher_->LastError()
                : "Unable to queue the radar batch: " + realtimePublisher_->LastError(),
            true);
        return false;
    }

    ++radarSimulation_.nextSequence;

    if (!quiet)
    {
        if (clearOnly)
        {
            SetStatus("Simulated radar tracks cleared from page 'Radar'.", false);
        }
        else if (initializeBeforeSend)
        {
            SetStatus("Generated radar UI initialized and batch sent to page 'Radar'.", false);
        }
        else
        {
            SetStatus(
                "Radar batch sent to page 'Radar': " + std::to_string(radarSimulation_.lastCommandCount) +
                    " track commands.",
                false);
        }
    }

    return true;
}

bool MockupApplication::SendCockpitSimulationBatch(const bool quiet, const bool initializeBeforeSend)
{
    if (realtimePublisher_ == nullptr || !realtimePublisher_->IsReady())
    {
        SetStatus("Realtime publisher is not ready for the cockpit simulator.", true);
        return false;
    }

    if (!IsCockpitDemoWindow())
    {
        SetStatus("The selected window does not expose the expected cockpit composite page.", true);
        return false;
    }

    if (loaded_.document.reticleLibrary.find(std::string(kCockpitRadarTemplateId)) == loaded_.document.reticleLibrary.end())
    {
        SetStatus("The reticle library does not expose the 'cockpit_radar_contact' template.", true);
        return false;
    }

    CockpitSimulationState& simulation = cockpitSimulation_;
    if (cockpitGeneratedUi_ == nullptr)
    {
        cockpitGeneratedUi_ = MakeInitializedGeneratedUi<cockpit_demo_ui::CockpitDemoMockupUi>();
    }
    if (initializeBeforeSend)
    {
        cockpitGeneratedUi_->Initialize();
    }
    PopulateCockpitSimulationUi(*cockpitGeneratedUi_, cockpitRadarContacts_, simulation);
    mfd::CommandBatch batch = cockpitGeneratedUi_->BuildCommandBatch(simulation.nextSequence);

    const double sendStart = TimeSeconds();
    const int commandCount = static_cast<int>(batch.commands.size());
    const bool submitted = realtimePublisher_->SubmitLatest(std::move(batch));
    const double sendEnd = TimeSeconds();

    simulation.lastSendDurationMs = static_cast<float>((sendEnd - sendStart) * 1000.0);
    simulation.lastCommandCount = commandCount;

    if (simulation.previousSendTimestamp > 0.0)
    {
        simulation.lastObservedCycleMs =
            static_cast<float>((sendStart - simulation.previousSendTimestamp) * 1000.0);
    }

    simulation.previousSendTimestamp = sendStart;

    if (!submitted)
    {
        SetStatus("Unable to queue the cockpit batch: " + realtimePublisher_->LastError(), true);
        return false;
    }

    ++simulation.nextSequence;

    if (!quiet)
    {
        if (initializeBeforeSend)
        {
            SetStatus(
                "Generated cockpit UI initialized and batch sent: " +
                    std::to_string(simulation.lastCommandCount) + " commands.",
                false);
        }
        else
        {
            SetStatus(
                "Cockpit composite batch sent: " + std::to_string(simulation.lastCommandCount) +
                    " commands.",
                false);
        }
    }

    return true;
}

bool MockupApplication::ActivateRadarSimulationPage()
{
    if (client_ == nullptr || !client_->IsReady())
    {
        SetStatus("UDP command client is not ready.", true);
        return false;
    }

    switch (DetectWindowPresetKind(windowFile_))
    {
    case WindowPresetKind::FullDemo:
        if (fullDemoGeneratedUi_ == nullptr)
        {
            fullDemoGeneratedUi_ = MakeInitializedGeneratedUi<full_demo_ui::FullDemoMockupUi>();
        }
        if (!client_->ActivatePage(fullDemoGeneratedUi_->Radar()))
        {
            SetStatus("Unable to activate generated radar page: " + client_->LastError(), true);
            return false;
        }
        return true;
    case WindowPresetKind::MinimalRadar:
        if (minimalRadarGeneratedUi_ == nullptr)
        {
            minimalRadarGeneratedUi_ = MakeInitializedGeneratedUi<minimal_radar_ui::MinimalRadarMockupUi>();
        }
        if (!client_->ActivatePage(minimalRadarGeneratedUi_->Radar()))
        {
            SetStatus("Unable to activate generated radar page: " + client_->LastError(), true);
            return false;
        }
        return true;
    default:
        SetStatus("No generated radar page is available for the selected window preset.", true);
        return false;
    }
}

bool MockupApplication::ActivateCockpitSimulationPage()
{
    if (client_ == nullptr || !client_->IsReady())
    {
        SetStatus("UDP command client is not ready.", true);
        return false;
    }

    if (cockpitGeneratedUi_ == nullptr)
    {
        cockpitGeneratedUi_ = MakeInitializedGeneratedUi<cockpit_demo_ui::CockpitDemoMockupUi>();
    }

    if (!client_->ActivatePage(cockpitGeneratedUi_->Cockpit()))
    {
        SetStatus("Unable to activate generated cockpit page: " + client_->LastError(), true);
        return false;
    }

    return true;
}

void MockupApplication::ResetSelectedPageDraft()
{
    const mfd::PageDefinition* page = PageByIndex(selection_.pageIndex);
    if (page == nullptr)
    {
        return;
    }

    const std::string key = page->normalizedName.empty() ? mfd::NormalizePageName(page->name) : page->normalizedName;
    pageDrafts_.erase(key);
    EnsurePageDraft(selection_.pageIndex);
}

void MockupApplication::ResetSelectedReticleDraft()
{
    const mfd::PageDefinition* page = PageByIndex(selection_.pageIndex);
    const mfd::ReticleGroup* reticle = ReticleByIndex(selection_.pageIndex, selection_.reticleIndex);
    if (page == nullptr || reticle == nullptr)
    {
        return;
    }

    const std::string pageKey = page->normalizedName.empty() ? mfd::NormalizePageName(page->name) : page->normalizedName;
    const std::string reticleKey = pageKey + ":" + mfd::NormalizePageName(reticle->id);
    reticleDrafts_.erase(reticleKey);
    EnsureReticleDraft(selection_.pageIndex, selection_.reticleIndex);
}

void MockupApplication::SetStatus(std::string message, const bool isError)
{
    lastStatus_ = std::move(message);
    statusIsError_ = isError;
}

void MockupApplication::DrawUi()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin(
        "Client Mockup Root",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(2);

    DrawHeaderBar();

    ImGui::BeginChild("BodyRegion", ImVec2(0.0f, -kStatusHeight), false);
    if (IsCockpitDemoWindow())
    {
        ImGui::BeginChild("CockpitPilotPane", ImVec2(0.0f, 0.0f), true);
        DrawCockpitPilotView();
        ImGui::EndChild();
    }
    else
    {
        ImGui::BeginChild("NavigationPane", ImVec2(kSidebarWidth, 0.0f), true);
        DrawNavigationTree();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("InspectorPane", ImVec2(0.0f, 0.0f), true);
        DrawInspector();
        ImGui::EndChild();
    }
    ImGui::EndChild();

    DrawStatusBar();

    ImGui::End();
}

void MockupApplication::DrawHeaderBar()
{
    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.00f), "Client Mockup Control");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextUnformatted(
        IsCockpitDemoWindow()
            ? "Cockpit pilot station over the live command API"
            : "Window, pages, reticles and strobe commands");

    ImGui::Spacing();

    ImGui::PushItemWidth(240.0f);
    if (ImGui::BeginCombo("Window target", knownWindowFiles_[static_cast<std::size_t>(selectedWindowFileIndex_)].label))
    {
        for (int index = 0; index < static_cast<int>(knownWindowFiles_.size()); ++index)
        {
            const bool selected = selectedWindowFileIndex_ == index;
            if (ImGui::Selectable(knownWindowFiles_[static_cast<std::size_t>(index)].label, selected))
            {
                selectedWindowFileIndex_ = index;
                windowFile_ = knownWindowFiles_[static_cast<std::size_t>(index)].path;
                ReloadConfiguration();
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (AccentButton("Reload"))
    {
        ReloadConfiguration();
    }

    if (!IsCockpitDemoWindow())
    {
        ImGui::SameLine();
        if (AccentButton("Activate selected page"))
        {
            SendActivatePage();
        }
    }

    ImGui::Spacing();

    if (!loaded_.window.title.empty())
    {
        ImGui::Text("Window title: %s", loaded_.window.title.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("| %dx%d px", loaded_.window.width, loaded_.window.height);
    }

    const bool hasClient = client_ != nullptr && client_->IsReady();
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextColored(
        hasClient ? ImVec4(0.33f, 0.86f, 0.78f, 1.00f) : ImVec4(1.00f, 0.45f, 0.45f, 1.00f),
        hasClient ? "Connected" : "UDP unavailable");

    if (loaded_.window.commandTransports.udp.has_value())
    {
        const auto& udp = *loaded_.window.commandTransports.udp;
        ImGui::SameLine();
        ImGui::TextDisabled("| UDP %s:%u", udp.address.c_str(), static_cast<unsigned>(udp.port));
    }

    if (loaded_.window.feedbackTransports.udp.has_value())
    {
        const auto& udp = *loaded_.window.feedbackTransports.udp;
        ImGui::SameLine();
        ImGui::TextDisabled("| Feedback %s:%u", udp.address.c_str(), static_cast<unsigned>(udp.port));
    }

    ImGui::SameLine();
    ImGui::TextDisabled("| FPS %.1f", ImGui::GetIO().Framerate);

    if (ImGui::GetIO().Framerate > 0.0f)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("| Frame %.2f ms", 1000.0f / ImGui::GetIO().Framerate);
    }

    ImGui::SameLine();
    ImGui::TextDisabled(
        "| Cockpit cycle %.1f / %.1f ms",
        cockpitSimulation_.lastObservedCycleMs > 0.0f
            ? cockpitSimulation_.lastObservedCycleMs
            : kCockpitSimulationIntervalSeconds * 1000.0f,
        kCockpitSimulationIntervalSeconds * 1000.0f);

    if (!IsCockpitDemoWindow())
    {
        ImGui::SameLine();
        ImGui::TextDisabled(
            "| Radar cycle %.1f / %.1f ms",
            radarSimulation_.lastObservedCycleMs > 0.0f
                ? radarSimulation_.lastObservedCycleMs
                : kRadarSimulationIntervalSeconds * 1000.0f,
            kRadarSimulationIntervalSeconds * 1000.0f);
    }

    ImGui::Separator();
}

void MockupApplication::DrawNavigationTree()
{
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Window structure");
    ImGui::TextDisabled("Select a page, a strobe or a reticle to edit it.");
    ImGui::Spacing();

    if (loaded_.document.pages.empty())
    {
        ImGui::TextDisabled("No page is loaded.");
        return;
    }

    for (int pageIndex = 0; pageIndex < static_cast<int>(loaded_.document.pages.size()); ++pageIndex)
    {
        const auto& page = loaded_.document.pages[static_cast<std::size_t>(pageIndex)];
        const bool pageSelected = selection_.kind == SelectionKind::Page && selection_.pageIndex == pageIndex;

        ImGuiTreeNodeFlags pageFlags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (pageSelected)
        {
            pageFlags |= ImGuiTreeNodeFlags_Selected;
        }

        const std::string pageLabel =
            (page.title.empty() ? page.name : page.title) + "##page_" + std::to_string(pageIndex);
        const bool pageOpened = ImGui::TreeNodeEx(pageLabel.c_str(), pageFlags);

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            SelectPage(pageIndex);
        }

        if (pageOpened)
        {
            ImGui::TextDisabled("name: %s", page.name.c_str());

            for (int strobeIndex = 0; strobeIndex < static_cast<int>(page.strobes.size()); ++strobeIndex)
            {
                const auto& strobe = page.strobes[static_cast<std::size_t>(strobeIndex)];
                const bool strobeSelected =
                    selection_.kind == SelectionKind::Strobe &&
                    selection_.pageIndex == pageIndex &&
                    selection_.reticleIndex == strobeIndex;
                ImGuiTreeNodeFlags strobeFlags =
                    ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
                if (strobeSelected)
                {
                    strobeFlags |= ImGuiTreeNodeFlags_Selected;
                }

                const bool active = strobe.normalizedName == page.normalizedActiveStrobeName ||
                                    (page.normalizedActiveStrobeName.empty() && strobeIndex == 0);
                const std::string strobeLabel =
                    std::string(active ? "* " : "") + strobe.name +
                    "##strobe_" + std::to_string(pageIndex) + "_" + std::to_string(strobeIndex);
                ImGui::TreeNodeEx(strobeLabel.c_str(), strobeFlags);
                if (ImGui::IsItemClicked())
                {
                    SelectStrobe(pageIndex, strobeIndex);
                }
            }

            for (int reticleIndex = 0; reticleIndex < static_cast<int>(page.staticReticles.size()); ++reticleIndex)
            {
                const auto& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
                const bool reticleSelected =
                    selection_.kind == SelectionKind::Reticle &&
                    selection_.pageIndex == pageIndex &&
                    selection_.reticleIndex == reticleIndex;

                ImGuiTreeNodeFlags reticleFlags =
                    ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
                if (reticleSelected)
                {
                    reticleFlags |= ImGuiTreeNodeFlags_Selected;
                }

                const std::string baseLabel = reticle.id.empty() ? "reticle_" + std::to_string(reticleIndex) : reticle.id;
                const std::string label = baseLabel + "##reticle_" + std::to_string(pageIndex) + "_" + std::to_string(reticleIndex);
                ImGui::TreeNodeEx(label.c_str(), reticleFlags);

                if (ImGui::IsItemClicked())
                {
                    SelectReticle(pageIndex, reticleIndex);
                }
            }

            ImGui::TreePop();
        }
    }
}

void MockupApplication::DrawInspector()
{
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Inspector");
    ImGui::TextDisabled("%s", SelectionLabel(selection_, loaded_.document).c_str());
    ImGui::Spacing();

    if (!lastRuntimeError_.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Runtime warning: %s", lastRuntimeError_.c_str());
        ImGui::Spacing();
    }

    DrawWindowDisplayInspector();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    switch (selection_.kind)
    {
    case SelectionKind::Page:
        DrawPageInspector();
        break;

    case SelectionKind::Reticle:
        DrawReticleInspector();
        break;

    case SelectionKind::Strobe:
        DrawStrobeInspector();
        break;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    DrawDynamicComposer();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    DrawRadarSimulationPanel();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    DrawCockpitSimulationPanel();
}

void MockupApplication::DrawCockpitPilotView()
{
    ImGui::TextColored(ImVec4(0.78f, 0.92f, 0.83f, 1.0f), "Cockpit Pilot Station");
    ImGui::TextDisabled(
        "This connection exposes one composite page with the ADI on the left, the HUD in the center and the radar on the right.");
    ImGui::Spacing();

    if (!lastRuntimeError_.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Runtime warning: %s", lastRuntimeError_.c_str());
        ImGui::Spacing();
    }

    DrawCockpitSimulationPanel();
}

void MockupApplication::DrawWindowDisplayInspector()
{
    WindowDisplayDraft& draft = EnsureWindowDisplayDraft();
    const bool clientReady = client_ != nullptr && client_->IsReady();

    ImGui::TextColored(ImVec4(0.96f, 0.81f, 0.52f, 1.0f), "Window display");
    ImGui::TextDisabled("Whole-window controls applied after the page is rendered.");
    ImGui::TextDisabled("Disable forces a black screen without clearing the underlying scene state.");

    if (!clientReady)
    {
        ImGui::BeginDisabled();
    }

    ImGui::Checkbox("Invert colors", &draft.invertColors);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::SliderFloat("Brightness", &draft.brightness, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Disable output", &draft.disabled);

    if (AccentButton("Send window display"))
    {
        SendWindowDisplayUpdate();
    }

    ImGui::SameLine();
    if (AccentButton("Reset window"))
    {
        SendResetWindow();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset window display"))
    {
        ResetWindowDisplayDraft();
        SetStatus("Whole-window display draft reset.", false);
    }

    if (!clientReady)
    {
        ImGui::EndDisabled();
        ImGui::TextDisabled("Connect to a target window to send the display patch.");
    }
}

void MockupApplication::DrawPageInspector()
{
    const mfd::PageDefinition* page = PageByIndex(selection_.pageIndex);
    if (page == nullptr)
    {
        ImGui::TextDisabled("Select a page to edit it.");
        return;
    }

    PageDraft& draft = EnsurePageDraft(selection_.pageIndex);

    ImGui::Text("Page name: %s", page->name.c_str());
    ImGui::TextDisabled("Normalized key: %s", page->normalizedName.c_str());
    ImGui::TextDisabled("Static reticles: %d", static_cast<int>(page->staticReticles.size()));
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Page view");
    ImGui::SetNextItemWidth(260.0f);
    ImGui::DragFloat2("Center", draft.center.data(), 0.01f, -1.0f, 1.0f, "%.3f");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::DragFloat("Zoom", &draft.zoom, 0.02f, 0.1f, 20.0f, "%.3f");

    if (AccentButton("Send page view"))
    {
        SendPageView();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset page view"))
    {
        ResetSelectedPageDraft();
        SetStatus("Page view draft reset from JSON.", false);
    }

    ImGui::SameLine();
    if (ImGui::Button("Activate page now"))
    {
        SendActivatePage();
    }

    if (draft.hasStrobe)
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Quick strobe");
        const mfd::PageStrobeDefinition* selectedStrobe = StrobeByIndex(*page, draft.selectedStrobeIndex);
        const char* selectedStrobeLabel = selectedStrobe == nullptr ? "<none>" : selectedStrobe->name.c_str();
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::BeginCombo("Active strobe##quickStrobe", selectedStrobeLabel))
        {
            for (int strobeIndex = 0; strobeIndex < static_cast<int>(page->strobes.size()); ++strobeIndex)
            {
                const auto& strobe = page->strobes[static_cast<std::size_t>(strobeIndex)];
                const bool selected = draft.selectedStrobeIndex == strobeIndex;
                if (ImGui::Selectable(strobe.name.c_str(), selected))
                {
                    LoadStrobeDraft(*page, strobeIndex, draft, true);
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::Checkbox("Enabled##quickStrobe", &draft.strobeEnabled);
        ImGui::SetNextItemWidth(260.0f);
        ImGui::DragFloat2("Position##quickStrobe", draft.strobePosition.data(), 0.01f, -1.0f, 1.0f, "%.3f");

        if (ImGui::Button("Send strobe"))
        {
            SendStrobeUpdate();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Full strobe editor is also available from the tree.");

        if (const StrobeFeedbackEntry* feedback = StrobeFeedbackForPage(selection_.pageIndex); feedback != nullptr)
        {
            ImGui::TextDisabled("Live return: %.3f / %.3f",
                                feedback->feedback.position.x,
                                feedback->feedback.position.y);
            ImGui::SameLine();
            if (ImGui::SmallButton("Use live##quickStrobe"))
            {
                draft.strobeEnabled = feedback->feedback.active;
                draft.strobePosition = {feedback->feedback.position.x, feedback->feedback.position.y};
                SetStatus("Strobe draft synchronized from live return.", false);
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.96f, 0.81f, 0.52f, 1.0f), "Page blink types");

    if (page->blinkTypes.empty())
    {
        ImGui::TextDisabled("This page declares no blink types.");
    }
    else
    {
        const std::string defaultBlinkName =
            page->defaultBlinkTypeName.empty() ? page->blinkTypes.front().name : page->defaultBlinkTypeName;
        ImGui::TextDisabled("Default blink: %s", defaultBlinkName.c_str());

        for (const auto& blinkType : page->blinkTypes)
        {
            ImGui::BulletText("%s - %u ms", blinkType.name.c_str(), blinkType.durationMs);
        }
    }
}

void MockupApplication::DrawBlinkEditor(const mfd::PageDefinition& page,
                                        const char* idSuffix,
                                        bool& enabled,
                                        std::string& blinkTypeName)
{
    const std::string defaultBlinkName =
        page.defaultBlinkTypeName.empty()
            ? (page.blinkTypes.empty() ? std::string {"<none>"} : page.blinkTypes.front().name)
            : page.defaultBlinkTypeName;

    if (page.blinkTypes.empty())
    {
        ImGui::BeginDisabled();
        ImGui::Checkbox(std::string("Blink enabled##").append(idSuffix).c_str(), &enabled);
        ImGui::EndDisabled();
        ImGui::TextDisabled("This page does not define any blink type.");
        enabled = false;
        blinkTypeName.clear();
        return;
    }

    ImGui::Checkbox(std::string("Blink enabled##").append(idSuffix).c_str(), &enabled);

    const std::string preview =
        blinkTypeName.empty()
            ? "<page default: " + defaultBlinkName + ">"
            : blinkTypeName;
    const std::string comboLabel = std::string("Blink type##").append(idSuffix);
    if (ImGui::BeginCombo(comboLabel.c_str(), preview.c_str()))
    {
        const std::string defaultItem = "<page default> - " + defaultBlinkName;
        const bool defaultSelected = blinkTypeName.empty();
        if (ImGui::Selectable(defaultItem.c_str(), defaultSelected))
        {
            blinkTypeName.clear();
        }

        if (defaultSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        for (const auto& blinkType : page.blinkTypes)
        {
            const bool selected = blinkTypeName == blinkType.name;
            const std::string label =
                blinkType.name + " - " + std::to_string(blinkType.durationMs) + " ms";
            if (ImGui::Selectable(label.c_str(), selected))
            {
                blinkTypeName = blinkType.name;
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    ImGui::TextDisabled("Same effective duration means same blink phase inside this page.");
}

void MockupApplication::DrawReticleInspector()
{
    const mfd::PageDefinition* page = PageByIndex(selection_.pageIndex);
    const mfd::ReticleGroup* reticle = ReticleByIndex(selection_.pageIndex, selection_.reticleIndex);
    if (page == nullptr || reticle == nullptr)
    {
        ImGui::TextDisabled("Select a reticle to edit it.");
        return;
    }

    ReticleDraft& draft = EnsureReticleDraft(selection_.pageIndex, selection_.reticleIndex);

    ImGui::Text("Page: %s", page->name.c_str());
    ImGui::Text("Reticle id: %s", reticle->id.empty() ? "<missing>" : reticle->id.c_str());
    if (!reticle->sourceTemplateId.empty())
    {
        ImGui::TextDisabled("Template: %s", reticle->sourceTemplateId.c_str());
    }
    ImGui::TextDisabled("Primitive count: %d", static_cast<int>(reticle->primitives.size()));
    ImGui::Spacing();

    if (reticle->id.empty())
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.65f, 0.30f, 1.0f),
            "This reticle has no public id, so commands cannot target it.");
        ImGui::Spacing();
    }

    ImGui::Checkbox("Visible", &draft.visible);
    ImGui::SetNextItemWidth(260.0f);
    ImGui::DragFloat2("Position", draft.position.data(), 0.01f, -1.0f, 1.0f, "%.3f");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::DragFloat("Rotation deg", &draft.rotation, 0.25f, -360.0f, 360.0f, "%.2f");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::DragFloat("Thickness", &draft.thickness, 0.0002f, 0.0005f, 0.05f, "%.4f");
    ImGui::ColorEdit4("Color", &draft.color.x, ImGuiColorEditFlags_DisplayRGB);
    DrawBlinkEditor(*page, "reticle", draft.blinkEnabled, draft.blinkTypeName);

    if (draft.hasText)
    {
        ImGui::InputText("Text", draft.text.data(), draft.text.size());
        ImGui::SetNextItemWidth(220.0f);
        ImGui::DragFloat("Letter spacing", &draft.letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f");
    }
    else
    {
        ImGui::TextDisabled("No text primitive detected in this reticle.");
    }

    const bool disableActions = reticle->id.empty();
    if (disableActions)
    {
        ImGui::BeginDisabled();
    }

    if (AccentButton("Send reticle update"))
    {
        SendReticleUpdate();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset reticle"))
    {
        ResetSelectedReticleDraft();
        SetStatus("Reticle draft reset from JSON.", false);
    }

    if (disableActions)
    {
        ImGui::EndDisabled();
    }
}

void MockupApplication::DrawStrobeInspector()
{
    const mfd::PageDefinition* page = PageByIndex(selection_.pageIndex);
    if (page == nullptr)
    {
        ImGui::TextDisabled("Select a strobe to edit it.");
        return;
    }

    PageDraft& draft = EnsurePageDraft(selection_.pageIndex);
    if (!draft.hasStrobe)
    {
        ImGui::TextDisabled("The selected page has no strobe.");
        return;
    }

    ImGui::Text("Page: %s", page->name.c_str());
    const mfd::PageStrobeDefinition* selectedStrobe = StrobeByIndex(*page, draft.selectedStrobeIndex);
    ImGui::TextDisabled("Strobe commands are optional and page specific.");
    ImGui::Spacing();

    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::BeginCombo("Authored strobe", selectedStrobe == nullptr ? "<none>" : selectedStrobe->name.c_str()))
    {
        for (int strobeIndex = 0; strobeIndex < static_cast<int>(page->strobes.size()); ++strobeIndex)
        {
            const auto& strobe = page->strobes[static_cast<std::size_t>(strobeIndex)];
            const bool selected = draft.selectedStrobeIndex == strobeIndex;
            if (ImGui::Selectable(strobe.name.c_str(), selected))
            {
                LoadStrobeDraft(*page, strobeIndex, draft, true);
                selection_.reticleIndex = strobeIndex;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Checkbox("Strobe active", &draft.strobeEnabled);
    ImGui::SetNextItemWidth(260.0f);
    ImGui::DragFloat2("Strobe position", draft.strobePosition.data(), 0.01f, -1.0f, 1.0f, "%.3f");

    if (AccentButton("Send strobe update"))
    {
        SendStrobeUpdate();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset strobe"))
    {
        ResetSelectedPageDraft();
        SetStatus("Strobe draft reset from JSON.", false);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Live return from window");

    if (const StrobeFeedbackEntry* feedbackEntry = StrobeFeedbackForPage(selection_.pageIndex);
        feedbackEntry != nullptr)
    {
        if (ImGui::Button("Copy live return into draft"))
        {
            draft.strobeEnabled = feedbackEntry->feedback.active;
            draft.strobePosition = {feedbackEntry->feedback.position.x, feedbackEntry->feedback.position.y};
            SetStatus("Strobe draft synchronized from live return.", false);
        }

        ImGui::Spacing();

        const mfd::StrobeStatusFeedback& feedback = feedbackEntry->feedback;
        ImGui::Text("Sequence: %u", feedback.sequence);
        ImGui::Text("Reported strobe id: %s", feedback.strobeId.empty() ? "<unknown>" : feedback.strobeId.c_str());
        ImGui::Text("Active: %s", feedback.active ? "true" : "false");
        ImGui::Text("Actual position: %.3f / %.3f", feedback.position.x, feedback.position.y);
        ImGui::Text("Capture area: %s",
                    feedback.capture.shape == mfd::StrobeCaptureShape::Circle ? "circle" : "rectangle");
        if (feedback.capture.shape == mfd::StrobeCaptureShape::Circle)
        {
            ImGui::Text("Capture radius: %.3f", feedback.capture.radius);
        }
        else
        {
            ImGui::Text("Capture size: %.3f / %.3f", feedback.capture.size.x, feedback.capture.size.y);
        }

        ImGui::Spacing();
        ImGui::Text("Magnet enabled: %s", feedback.magnet.enabled ? "true" : "false");
        if (feedback.magnet.enabled)
        {
            ImGui::Text("Magnet radius: %.3f", feedback.magnet.radius);
            ImGui::Text("Magnet strength: %.3f", feedback.magnet.strength);
            ImGui::Text("Magnetized: %s", feedback.magnet.magnetized ? "true" : "false");

            if (feedback.magnet.magnetized)
            {
                ImGui::Text("Target: %s", feedback.magnet.reticleId.c_str());
                ImGui::Text("Target position: %.3f / %.3f",
                            feedback.magnet.targetPosition.x,
                            feedback.magnet.targetPosition.y);
                ImGui::Text("Magnet distance: %.3f", feedback.magnet.distance);
            }
        }

        ImGui::Spacing();
        if (feedback.captureResult.has_value())
        {
            ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Captured reticle");
            ImGui::Text("Reticle: %s", feedback.captureResult->reticleId.c_str());
            if (!feedback.captureResult->label.empty())
            {
                ImGui::Text("Label: %s", feedback.captureResult->label.c_str());
            }
            if (!feedback.captureResult->category.empty())
            {
                ImGui::Text("Category: %s", feedback.captureResult->category.c_str());
            }
            ImGui::Text("Position: %.3f / %.3f",
                        feedback.captureResult->position.x,
                        feedback.captureResult->position.y);
            ImGui::Text("Distance: %.3f", feedback.captureResult->distance);
        }
        else
        {
            ImGui::TextDisabled("No dynamic reticle currently captured by the strobe.");
        }

        ImGui::TextDisabled("Received %.2f s ago", TimeSeconds() - feedbackEntry->receivedTimestamp);
    }
    else
    {
        ImGui::TextDisabled("No strobe feedback received yet for this page.");
    }
}

void MockupApplication::DrawDynamicComposer()
{
    const mfd::PageDefinition* page = PageByIndex(selection_.pageIndex);

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Dynamic reticle");
    if (page != nullptr)
    {
        ImGui::TextDisabled("Target page: %s", page->name.c_str());
    }
    else
    {
        ImGui::TextDisabled("Select a page to target dynamic commands.");
    }

    if (templateIds_.empty())
    {
        ImGui::TextDisabled("No template is available in the reticle library.");
        return;
    }

    if (selectedTemplateIndex_ < 0 || selectedTemplateIndex_ >= static_cast<int>(templateIds_.size()))
    {
        selectedTemplateIndex_ = 0;
    }

    ImGui::PushItemWidth(280.0f);
    if (ImGui::BeginCombo("Template", templateIds_[static_cast<std::size_t>(selectedTemplateIndex_)].c_str()))
    {
        for (int index = 0; index < static_cast<int>(templateIds_.size()); ++index)
        {
            const bool selected = selectedTemplateIndex_ == index;
            const std::string& templateId = templateIds_[static_cast<std::size_t>(index)];
            if (ImGui::Selectable(templateId.c_str(), selected))
            {
                selectedTemplateIndex_ = index;
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::InputText("Dynamic id", dynamicDraft_.id.data(), dynamicDraft_.id.size());
    ImGui::Checkbox("Visible##dynamic", &dynamicDraft_.visible);
    ImGui::SetNextItemWidth(260.0f);
    ImGui::DragFloat2("Position##dynamic", dynamicDraft_.position.data(), 0.01f, -1.0f, 1.0f, "%.3f");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::DragFloat("Rotation##dynamic", &dynamicDraft_.rotation, 0.25f, -360.0f, 360.0f, "%.2f");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::DragFloat("Thickness##dynamic", &dynamicDraft_.thickness, 0.0002f, 0.0005f, 0.05f, "%.4f");
    ImGui::ColorEdit4("Color##dynamic", &dynamicDraft_.color.x, ImGuiColorEditFlags_DisplayRGB);
    if (page != nullptr)
    {
        DrawBlinkEditor(*page, "dynamic", dynamicDraft_.blinkEnabled, dynamicDraft_.blinkTypeName);
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Checkbox("Blink enabled##dynamic_missing_page", &dynamicDraft_.blinkEnabled);
        ImGui::EndDisabled();
        dynamicDraft_.blinkEnabled = false;
        dynamicDraft_.blinkTypeName.clear();
        ImGui::TextDisabled("Select a page to access its blink types.");
    }
    ImGui::InputText("Text##dynamic", dynamicDraft_.text.data(), dynamicDraft_.text.size());
    ImGui::SetNextItemWidth(220.0f);
    ImGui::DragFloat("Letter spacing##dynamic", &dynamicDraft_.letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f");

    const bool disableDynamic = page == nullptr;
    if (disableDynamic)
    {
        ImGui::BeginDisabled();
    }

    if (AccentButton("Upsert dynamic reticle"))
    {
        SendDynamicUpsert();
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove dynamic reticle"))
    {
        SendDynamicRemove();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset dynamic draft"))
    {
        ResetDynamicDraft();
        SetStatus("Dynamic draft reset.", false);
    }

    if (disableDynamic)
    {
        ImGui::EndDisabled();
    }
}

void MockupApplication::DrawRadarSimulationPanel()
{
    const bool hasRadarPage = std::any_of(
        loaded_.document.pages.begin(),
        loaded_.document.pages.end(),
        [](const mfd::PageDefinition& page)
        {
            return mfd::PageNamesEqual(page.name, kRadarSimulationPageName);
        });
    const bool hasRadarTemplate = loaded_.document.reticleLibrary.find(std::string(kRadarSimulationTemplateId)) != loaded_.document.reticleLibrary.end();
    const bool clientReady = client_ != nullptr && client_->IsReady();

    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Radar batch simulator");
    ImGui::TextDisabled(
        "Sends 100 simulated detections to page 'Radar' every 20 ms with the UDP batch API and blink patches.");
    ImGui::Spacing();

    ImGui::Text("Target page: %s", std::string(kRadarSimulationPageName).c_str());
    ImGui::Text("Template: %s", std::string(kRadarSimulationTemplateId).c_str());
    ImGui::Text("Track count: %d", kRadarSimulationTrackCount);
    ImGui::Text("Target cycle: %.1f ms", kRadarSimulationIntervalSeconds * 1000.0f);
    ImGui::Text(
        "Observed cycle: %s",
        radarSimulation_.lastObservedCycleMs > 0.0f
            ? (std::to_string(radarSimulation_.lastObservedCycleMs) + " ms").c_str()
            : "n/a");
    ImGui::Text(
        "Last send time: %s",
        radarSimulation_.lastSendDurationMs > 0.0f
            ? (std::to_string(radarSimulation_.lastSendDurationMs) + " ms").c_str()
            : "n/a");
    ImGui::Text("Last command count: %d", radarSimulation_.lastCommandCount);
    ImGui::Text("UI FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Spacing();

    if (!hasRadarPage)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.30f, 1.0f), "This window JSON does not expose a page named 'Radar'.");
    }

    if (!hasRadarTemplate)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.65f, 0.30f, 1.0f),
            "The reticle library does not contain the 'radar_track' template.");
    }

    if (!clientReady)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "UDP command client is not ready.");
    }

    const bool canRun = hasRadarPage && hasRadarTemplate && clientReady;
    if (!canRun)
    {
        ImGui::BeginDisabled();
    }

    bool enabled = radarSimulation_.enabled;
    if (ImGui::Checkbox("Enable 100-track radar simulation", &enabled))
    {
        radarSimulation_.enabled = enabled;
        radarSimulation_.accumulatorSeconds = 0.0f;

        if (enabled)
        {
            radarSimulation_.elapsedSeconds = 0.0f;
            radarSimulation_.lastObservedCycleMs = 0.0f;
            radarSimulation_.lastSendDurationMs = 0.0f;
            radarSimulation_.lastCommandCount = 0;
            radarSimulation_.previousSendTimestamp = 0.0;
            radarSimulation_.clearRequested = false;
            if (ActivateRadarSimulationPage())
            {
                SetStatus("Radar simulator enabled. Sending 100 tracks every 20 ms.", false);
            }
            else
            {
                radarSimulation_.enabled = false;
            }
        }
        else
        {
            radarSimulation_.clearRequested = true;
            SetStatus("Radar simulator stopped. Simulated tracks will be cleared once.", false);
        }
    }

    if (AccentButton("Send one radar batch"))
    {
        SendRadarSimulationBatch(false, false);
    }

    ImGui::SameLine();
    if (ImGui::Button("Initialize generated radar UI"))
    {
        SendRadarSimulationBatch(false, false, true);
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear simulated tracks"))
    {
        radarSimulation_.clearRequested = false;
        SendRadarSimulationBatch(true, false);
    }

    if (!canRun)
    {
        ImGui::EndDisabled();
    }
}

void MockupApplication::DrawCockpitSimulationPanel()
{
    const bool hasCockpitPage = IsCockpitDemoWindow();
    const bool hasRadarTemplate = loaded_.document.reticleLibrary.find(std::string(kCockpitRadarTemplateId)) != loaded_.document.reticleLibrary.end();
    const bool clientReady = client_ != nullptr && client_->IsReady();
    int pitchCommand =
        (ImGui::IsKeyDown(ImGuiKey_UpArrow) ? 1 : 0) - (ImGui::IsKeyDown(ImGuiKey_DownArrow) ? 1 : 0);
    int rollCommand =
        (ImGui::IsKeyDown(ImGuiKey_RightArrow) ? 1 : 0) - (ImGui::IsKeyDown(ImGuiKey_LeftArrow) ? 1 : 0);

    ImGui::TextColored(ImVec4(0.78f, 0.92f, 0.83f, 1.0f), "Pilot Controls");
    ImGui::TextDisabled(
        "One live cockpit page: ADI on the left, HUD in the center, radar on the right.");
    ImGui::Spacing();

    ImGui::Text("Target page: %s", std::string(kCockpitPageName).c_str());
    ImGui::Text("Dynamic template: %s", std::string(kCockpitRadarTemplateId).c_str());
    ImGui::Text("Target cycle: %.1f ms", kCockpitSimulationIntervalSeconds * 1000.0f);
    ImGui::Text(
        "Observed cycle: %s",
        cockpitSimulation_.lastObservedCycleMs > 0.0f
            ? (std::to_string(cockpitSimulation_.lastObservedCycleMs) + " ms").c_str()
            : "n/a");
    ImGui::Text(
        "Last send time: %s",
        cockpitSimulation_.lastSendDurationMs > 0.0f
            ? (std::to_string(cockpitSimulation_.lastSendDurationMs) + " ms").c_str()
            : "n/a");
    ImGui::Text("Last command count: %d", cockpitSimulation_.lastCommandCount);
    ImGui::Spacing();

    if (!hasCockpitPage)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.65f, 0.30f, 1.0f),
            "This window JSON must expose the composite page 'Cockpit'.");
    }

    if (!hasRadarTemplate)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.65f, 0.30f, 1.0f),
            "The reticle library does not contain the 'cockpit_radar_contact' template.");
    }

    if (!clientReady)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "UDP command client is not ready.");
    }

    const bool canRun = hasCockpitPage && hasRadarTemplate && clientReady;
    if (!canRun)
    {
        ImGui::BeginDisabled();
    }

    bool enabled = cockpitSimulation_.enabled;
    if (ImGui::Checkbox("Enable cockpit simulation", &enabled))
    {
        if (enabled)
        {
            const float preservedThrottle = cockpitSimulation_.throttle;
            const bool preservedRadarEnabled = cockpitSimulation_.radarEnabled;
            cockpitSimulation_ = {};
            cockpitSimulation_.enabled = true;
            cockpitSimulation_.throttle = preservedThrottle;
            cockpitSimulation_.radarEnabled = preservedRadarEnabled;
            if (ActivateCockpitSimulationPage())
            {
                SendCockpitSimulationBatch(true);
                SetStatus("Cockpit simulator enabled. The composite cockpit page now updates every 20 ms.", false);
            }
            else
            {
                cockpitSimulation_.enabled = false;
            }
        }
        else
        {
            cockpitSimulation_.enabled = false;
            cockpitSimulation_.accumulatorSeconds = 0.0f;
            SetStatus("Cockpit simulator paused. The last cockpit frame stays visible.", false);
        }
    }

    ImGui::SameLine();
    if (AccentButton("Send one cockpit frame"))
    {
        SendCockpitSimulationBatch(false);
    }

    ImGui::SameLine();
    if (ImGui::Button("Initialize generated cockpit UI"))
    {
        SendCockpitSimulationBatch(false, true);
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset cockpit state"))
    {
        const bool wasEnabled = cockpitSimulation_.enabled;
        const float preservedThrottle = cockpitSimulation_.throttle;
        const bool preservedRadarEnabled = cockpitSimulation_.radarEnabled;
        cockpitSimulation_ = {};
        cockpitSimulation_.enabled = wasEnabled;
        cockpitSimulation_.throttle = preservedThrottle;
        cockpitSimulation_.radarEnabled = preservedRadarEnabled;
        if (wasEnabled)
        {
            SendCockpitSimulationBatch(true);
        }
        SetStatus("Cockpit simulator state reset.", false);
    }

    ImGui::Spacing();
    float throttlePercent = cockpitSimulation_.throttle * 100.0f;
    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::SliderFloat("Throttle", &throttlePercent, 0.0f, 100.0f, "%.0f %%"))
    {
        cockpitSimulation_.throttle = throttlePercent / 100.0f;
    }

    ImGui::Checkbox("Radar enabled", &cockpitSimulation_.radarEnabled);
    ImGui::TextDisabled("Arrow keys: Up/Down pitch, Left/Right roll. Heading then evolves from roll.");
    ImGui::Spacing();

    bool pitchUpHeld = false;
    bool pitchDownHeld = false;
    bool rollLeftHeld = false;
    bool rollRightHeld = false;

    ImGui::PushButtonRepeat(true);
    ImGui::ArrowButton("##cockpit_pitch_up", ImGuiDir_Up);
    pitchUpHeld = ImGui::IsItemActive();

    ImGui::ArrowButton("##cockpit_roll_left", ImGuiDir_Left);
    rollLeftHeld = ImGui::IsItemActive();
    ImGui::SameLine();
    ImGui::ArrowButton("##cockpit_pitch_down", ImGuiDir_Down);
    pitchDownHeld = ImGui::IsItemActive();
    ImGui::SameLine();
    ImGui::ArrowButton("##cockpit_roll_right", ImGuiDir_Right);
    rollRightHeld = ImGui::IsItemActive();
    ImGui::PopButtonRepeat();

    if (pitchUpHeld)
    {
        ++pitchCommand;
    }
    if (pitchDownHeld)
    {
        --pitchCommand;
    }
    if (rollRightHeld)
    {
        ++rollCommand;
    }
    if (rollLeftHeld)
    {
        --rollCommand;
    }

    cockpitSimulation_.pitchControlInput =
        static_cast<float>(std::clamp(pitchCommand, -1, 1));
    cockpitSimulation_.rollControlInput =
        static_cast<float>(std::clamp(rollCommand, -1, 1));

    ImGui::Text(
        "Pitch command: %s",
        cockpitSimulation_.pitchControlInput > 0.0f
            ? "UP"
            : (cockpitSimulation_.pitchControlInput < 0.0f ? "DOWN" : "NEUTRAL"));
    ImGui::SameLine();
    ImGui::Text(
        "Roll command: %s",
        cockpitSimulation_.rollControlInput > 0.0f
            ? "RIGHT"
            : (cockpitSimulation_.rollControlInput < 0.0f ? "LEFT" : "NEUTRAL"));

    ImGui::Spacing();
    ImGui::Text("Pitch %.1f deg", cockpitSimulation_.pitchDegrees);
    ImGui::Text("Bank %.1f deg", cockpitSimulation_.bankDegrees);
    ImGui::Text("Heading %s deg", FormatHeadingValue(cockpitSimulation_.headingDegrees).c_str());
    ImGui::Text("Flight path %.1f deg", cockpitSimulation_.flightPathAngleDegrees);
    ImGui::Text(
        "Speed %.0f kts | Mach %.2f",
        cockpitSimulation_.speedKts,
        cockpitSimulation_.speedKts / kKnotsPerMach);
    ImGui::Text("Radar: %s", cockpitSimulation_.radarEnabled ? "EMIT" : "STBY");
    ImGui::Spacing();

    if (AccentButton("Show cockpit page"))
    {
        ActivateCockpitSimulationPage();
    }

    ImGui::SameLine();
    if (ImGui::Button("Neutralize bank"))
    {
        cockpitSimulation_.bankDegrees = 0.0f;
        cockpitSimulation_.rollControlInput = 0.0f;
    }

    ImGui::SameLine();
    if (ImGui::Button("Level pitch"))
    {
        cockpitSimulation_.pitchDegrees = 0.0f;
        cockpitSimulation_.pitchControlInput = 0.0f;
    }

    if (!canRun)
    {
        ImGui::EndDisabled();
    }
}

void MockupApplication::DrawStatusBar()
{
    ImGui::BeginChild("StatusBar", ImVec2(0.0f, kStatusHeight), true);
    ImGui::TextColored(
        statusIsError_ ? ImVec4(1.0f, 0.45f, 0.45f, 1.0f) : ImVec4(0.33f, 0.86f, 0.78f, 1.0f),
        "%s",
        lastStatus_.empty() ? "Ready." : lastStatus_.c_str());

    if (client_ != nullptr && !client_->LastError().empty())
    {
        ImGui::TextDisabled("UDP detail: %s", client_->LastError().c_str());
    }

    if (realtimePublisher_ != nullptr && !realtimePublisher_->LastError().empty())
    {
        ImGui::TextDisabled("Realtime detail: %s", realtimePublisher_->LastError().c_str());
    }

    if (!lastFeedbackStatus_.empty())
    {
        ImGui::TextDisabled("Feedback detail: %s", lastFeedbackStatus_.c_str());
    }

    if (!lastRuntimeError_.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.45f, 1.0f), "Runtime detail: %s", lastRuntimeError_.c_str());
    }
    ImGui::EndChild();
}
} // namespace
