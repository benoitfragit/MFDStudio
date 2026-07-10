/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Dear ImGui front-end for the HUD client.
 */

#include <string>

#include "hud/HudRuntimeClient.h"
#include "hud_main/HudSimulation.h"

namespace hud_main
{
/**
 * @brief Scripted maneuver used to exercise HUD behavior through all attitudes.
 */
enum class HudManeuver
{
    Manual,
    Loop,
    BarrelRoll
};

/**
 * @brief Interactive HUD client application.
 *
 * The application is a consumer of the reusable `hud_runtime` library: it owns
 * the ImGui control panel and the mini-simulation, fills one
 * `hud::HudInputSample` per frame and publishes it through
 * `hud::HudRuntimeClient`. All transport and generated-UI details live inside
 * the runtime library.
 */
class HudApplication
{
public:
    HudApplication();

    /**
     * @brief Initializes the HUD runtime client from the staged asset layout.
     * @param error Human-readable failure reason populated on initialization failure.
     * @return `true` when the client can publish HUD frames.
     */
    bool Initialize(std::string& error);

    /**
     * @brief Draws one ImGui frame, advances simulation and publishes HUD commands.
     * @param deltaSeconds Host frame delta in seconds.
     */
    void DrawFrame(float deltaSeconds);

    /**
     * @brief Shuts the HUD runtime client down and emits a shutdown caption.
     */
    void Shutdown();

private:
    /**
     * @brief Samples keyboard state and converts it to normalized pilot controls.
     */
    void ApplyKeyboardControls();

    /**
     * @brief Selects the scripted maneuver used to fill pilot controls.
     */
    void SelectManeuver(HudManeuver maneuver) noexcept;

    /**
     * @brief Writes scripted maneuver commands into the current pilot-control buffer.
     */
    void ApplyManeuverControls() noexcept;

    /**
     * @brief Advances the mini-simulation and refreshes the semantic HUD input sample.
     */
    void UpdateHudInputBufferFromUi(float deltaSeconds);

    /**
     * @brief Copies the simulation-produced SI sample into the publishing buffer.
     */
    void SyncHudInputBufferFromSimulation();

    /**
     * @brief Draws connection state, liveness state and lifecycle buttons.
     */
    void DrawConnectionPanel();

    /**
     * @brief Draws throttle, maneuver and manual-stick controls.
     */
    void DrawFlightControls();

    /**
     * @brief Draws wind, turbulence, terrain and atmosphere controls.
     *
     * This panel edits `SimulationControls::environment` only: it is a control
     * tool for the bundled mini-simulation, not a HUD symbology input.
     */
    void DrawEnvironmentControls();

    /**
     * @brief Draws master-mode, missile selection and launch controls.
     */
    void DrawWeaponControls();

    /**
     * @brief Draws read-only aircraft, target and missile telemetry.
     */
    void DrawTelemetryPanel();

    /**
     * @brief Publishes the current semantic HUD sample through the runtime client.
     */
    void PublishFrame();

    /**
     * @brief Restores the simulation and pilot controls to their initial sample state.
     */
    void ResetScene();

    /**
     * @brief Attempts a missile launch and reports whether launch gating accepted it.
     */
    void FireSelectedMissile();

    /**
     * @brief Selects a missile type in the simulation and refreshes the HUD buffer.
     */
    void SelectMissile(MissileType missileType);

    /**
     * @brief Selects the resolved HUD mode without touching generated HUD handles.
     */
    void SelectHudMode(hud::HudMasterMode masterMode,
                       hud::HudWeaponMode weaponMode,
                       hud::HudGunMode gunMode) noexcept;

    /**
     * @brief Toggles the panel ILS avionics state used to fill `hud::HudInputSample`.
     */
    void SetIlsEnabled(bool enabled) noexcept;

    /**
     * @brief Stores the operator-facing status line shown by the control panel.
     */
    void SetStatus(std::string status, bool error);

    /** Reusable HUD publishing client owning transports and generated UI. */
    hud::HudRuntimeClient hudRuntime_ {};
    /** Deterministic SI-unit state producer used by the sample controls. */
    HudSimulation simulation_ {};
    /** Latest semantic aircraft/target/weapon sample published to the HUD. */
    hud::HudInputSample hudInputs_ {};
    /** Pilot and environment intent collected from ImGui controls or scripted maneuvers. */
    SimulationControls simulationControls_ {};
    /** Current source of pilot commands. */
    HudManeuver maneuver_ = HudManeuver::Manual;
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
} // namespace hud_main
