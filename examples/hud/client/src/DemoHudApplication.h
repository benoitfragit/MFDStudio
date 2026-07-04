/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Dear ImGui front-end for the Demo HUD demonstration client.
 */

#include <cstdint>
#include <memory>
#include <string>

#include "DemoHudController.h"
#include "DemoHudSimulation.h"
#include "DemoHudUi.h"
#include "mfd/client/ClientSdk.h"

namespace demo_hud
{
/**
 * @brief Scripted maneuver used to demonstrate HUD behavior through all attitudes.
 */
enum class DemoManeuver
{
    Manual,
    Loop,
    BarrelRoll
};

/**
 * @brief Interactive Demo HUD demo application.
 */
class DemoHudApplication
{
public:
    DemoHudApplication();

    /**
     * @brief Loads the HUD window configuration and prepares UDP publishers.
     * @param error Human-readable failure reason populated on initialization failure.
     * @return `true` when the demo can publish HUD frames.
     */
    bool Initialize(std::string& error);

    /**
     * @brief Draws one ImGui frame, advances simulation and publishes HUD commands.
     * @param deltaSeconds Host frame delta in seconds.
     */
    void DrawFrame(float deltaSeconds);

    /**
     * @brief Flushes pending HUD commands and emits a shutdown caption.
     */
    void Shutdown();

private:
    /**
     * @brief Runtime configuration extracted from the generated HUD window JSON.
     *
     * The application keeps the parsed transports and page view together so
     * reconnects can reuse the same validated asset configuration.
     */
    struct DemoConfig
    {
        mfd::WindowUdpCommandTransport commandTransport {};
        mfd::WindowFeedbackTransportConfig feedbackTransport {};
        mfd::GeneratedTransportMap generatedTransportMap {};
        mfd::PageViewState pageView {};
    };

    /**
     * @brief Reads the generated HUD window JSON and validates required runtime transports.
     */
    bool LoadConfig(DemoConfig& config, std::string& error) const;

    /**
     * @brief Creates the command clients, feedback receiver and generated UI startup state.
     */
    bool Connect(const DemoConfig& config, std::string& error);

    /**
     * @brief Samples keyboard state and converts it to normalized pilot controls.
     */
    void ApplyKeyboardControls();

    /**
     * @brief Selects the scripted maneuver used to fill pilot controls.
     */
    void SelectManeuver(DemoManeuver maneuver) noexcept;

    /**
     * @brief Writes scripted maneuver commands into the current pilot-control buffer.
     */
    void ApplyManeuverControls() noexcept;

    /**
     * @brief Advances the mini-simulation and refreshes the semantic HUD input sample.
     */
    void UpdateHudInputBufferFromUi(float deltaSeconds) noexcept;

    /**
     * @brief Copies the simulation-produced SI sample into the publishing buffer.
     */
    void SyncHudInputBufferFromSimulation() noexcept;

    /**
     * @brief Draws connection state, liveness state and lifecycle buttons.
     */
    void DrawConnectionPanel();

    /**
     * @brief Draws throttle, maneuver and manual-stick controls.
     */
    void DrawFlightControls();

    /**
     * @brief Draws master-mode, missile selection and launch controls.
     */
    void DrawWeaponControls();

    /**
     * @brief Draws read-only aircraft, target and missile telemetry.
     */
    void DrawTelemetryPanel();

    /**
     * @brief Converts the current HUD input sample to generated commands and sends one batch.
     */
    void PublishFrame();

    /**
     * @brief Consumes feedback packets and updates the window liveness monitor.
     */
    void PollFeedback();

    /**
     * @brief Restores the simulation, UI and pilot controls to their initial demo state.
     */
    void ResetScene();

    /**
     * @brief Attempts a missile launch and reports whether launch gating accepted it.
     */
    void FireSelectedMissile();

    /**
     * @brief Selects a missile type in the simulation and refreshes the HUD buffer.
     */
    void SelectMissile(MissileType missileType) noexcept;

    /**
     * @brief Selects the resolved demo HUD mode without touching generated HUD handles.
     */
    void SelectHudMode(HudMasterMode masterMode, HudWeaponMode weaponMode, HudGunMode gunMode) noexcept;

    /**
     * @brief Toggles the demo ILS avionics state used to fill `HudInputSample`.
     */
    void SetIlsDemoEnabled(bool enabled) noexcept;

    /**
     * @brief Stores the operator-facing status line shown by the control panel.
     */
    void SetStatus(std::string status, bool error);

    /** Generated UI command wrapper for `examples/hud/assets`. */
    demo_hud_ui::DemoHudUi ui_ {};
    /** Deterministic SI-unit state producer used by the demo controls. */
    DemoHudSimulation simulation_ {};
    /** Stateless adapter from semantic HUD inputs to generated UI handles. */
    DemoHudController controller_ {};
    /** Latest semantic aircraft/target/weapon sample published to the HUD. */
    HudInputSample hudInputs_ {};
    /** Pilot intent collected from ImGui controls or scripted maneuvers. */
    PilotControls controls_ {};
    /** Current source of pilot commands. */
    DemoManeuver maneuver_ = DemoManeuver::Manual;
    /** Parsed asset and transport configuration reused across reconnects. */
    DemoConfig config_ {};
    /** Reliable startup client used for initial scene submission. */
    std::unique_ptr<mfd::CommandClient> startupClient_ {};
    /** Latest-batch UDP publisher used for realtime HUD updates. */
    std::unique_ptr<mfd::client::LatestBatchPublisher> publisher_ {};
    /** Optional feedback receiver for window close and packet-liveness signals. */
    std::unique_ptr<mfd::IExchangeChannel> feedbackReceiver_ {};
    /** Detects stale or closed runtime windows from decoded feedback packets. */
    mfd::client::WindowLivenessMonitor livenessMonitor_ {2.0};
    /** Monotonic command sequence number sent with generated command batches. */
    std::uint32_t sequence_ = 1;
    /** Application uptime used as the liveness-monitor clock. */
    double elapsedSeconds_ = 0.0;
    /** True while command publishing is available. */
    bool connected_ = false;
    /** True while the mini-simulation should advance each frame. */
    bool running_ = true;
    /** Result of the most recent missile-launch request. */
    bool lastFireAccepted_ = false;
    /** Last operator-facing status message. */
    std::string status_ {};
    /** True when `status_` should be rendered as an error. */
    bool statusIsError_ = false;
};
} // namespace demo_hud
