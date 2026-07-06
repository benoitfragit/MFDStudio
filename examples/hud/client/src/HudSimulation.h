/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Deterministic mini-simulation used only by the interactive HUD client.
 */

#include <vector>

#include "HudProjection.h"

namespace hud
{
/**
 * @brief Pilot controls sampled once per simulation step.
 *
 * These values are sample-only inputs for the bundled mini-simulation. They are
 * intentionally not a HUD integration contract: a real aircraft adapter should
 * provide `HudInputSample` directly through `HudProjection.h`.
 */
struct PilotControls
{
    /** Normalized pitch-stick command in [-1, 1]. Positive pitches up. */
    float pitchCommand = 0.0f;
    /** Normalized roll-stick command in [-1, 1]. Positive rolls right. */
    float rollCommand = 0.0f;
    /** Normalized throttle command in [0, 1]. */
    float throttle = 0.62f;
    /** True when the pilot requests afterburner at high throttle. */
    bool afterburnerRequested = false;
    /** Panel-selected HUD master mode. */
    HudMasterMode masterMode = HudMasterMode::Nav;
    /** Panel-selected weapon symbology mode. */
    HudWeaponMode weaponMode = HudWeaponMode::None;
    /** Panel-selected gun sight mode. */
    HudGunMode gunMode = HudGunMode::None;
    /** Panel-resolved master arm state. */
    bool masterArm = true;
    /** Panel-resolved SIM state. */
    bool simulateMode = false;
    /** Panel-resolved gun trigger state. */
    bool triggerHeld = false;
    /** Panel-resolved radar/gun target lock state. */
    bool targetLocked = false;
    /** Panel landing gear state. */
    bool landingGearDown = false;
    /** Panel landing mode state. */
    bool landingModeActive = false;
    /** Panel landing declutter state. */
    bool landingDeclutterActive = false;
    /** Panel ILS receiver power state. */
    bool ilsPowered = false;
    /** Panel ILS selection state. */
    bool ilsSelected = false;
    /** Panel ILS signal-valid state. */
    bool ilsSignalValid = false;
    /** Panel ILS command-steering state. */
    bool ilsCommandSteeringActive = false;
};

/**
 * @brief One simulated missile currently in flight.
 */
struct MissileShot
{
    /** Missile type associated with this shot. */
    MissileType type = MissileType::Aim120C;
    /** Initial computed time of flight in seconds. */
    float timeOfFlightSeconds = 0.0f;
    /** Remaining time before impact/status completion in seconds. */
    float timeRemainingSeconds = 0.0f;
    /** Target range at launch in nautical miles. */
    float launchRangeNm = 0.0f;
    /** Current display phase of the missile. */
    MissileFlightPhase phase = MissileFlightPhase::Boost;
};

/**
 * @brief Stable HUD-focused HUD mini-simulation for the interactive HUD client.
 *
 * @note This model is deliberately simplified. It preserves coherent relations
 * between attitude, energy, speed, altitude, A-A mode and missile symbology
 * rather than attempting to be a flight dynamics simulator. Its only integration
 * output is `HudInputSample`, so a real aircraft can replace this class and keep
 * the same HUD controller and funnel projection.
 */
class HudSimulation
{
public:
    HudSimulation();

    /**
     * @brief Restores aircraft, target, inventory and in-flight missiles.
     * @post `Inputs()` returns the default semantic HUD sample.
     */
    void Reset() noexcept;

    /**
     * @brief Replaces the pilot controls used by the next call to `Step`.
     * @param controls Latest pilot controls.
     */
    void SetControls(const PilotControls& controls) noexcept;

    /**
     * @brief Advances the simulation by one bounded deterministic step.
     * @param deltaSeconds Wall-clock delta in seconds; non-finite and negative
     * values are discarded, very large values are clamped.
     */
    void Step(float deltaSeconds) noexcept;

    /**
     * @brief Selects the active missile type shown by the HUD.
     * @param type Missile type to select.
     * @post `Inputs().weapon.selectedMissile` matches `type`.
     */
    void SelectMissile(MissileType type) noexcept;

    /**
     * @brief Cycles to the next available missile type.
     * @post `Inputs().weapon.selectedMissile` is toggled between supported missile types.
     */
    void CycleSelectedMissile() noexcept;

    /**
     * @brief Attempts to launch the selected missile through simulated launch gates.
     * @return `true` if inventory was consumed and a missile shot was created.
     */
    bool FireSelectedMissile() noexcept;

    /**
     * @brief Builds the current projected HUD frame from the simulation input sample.
     * @return Stateless HUD projection equivalent to `hud::BuildHudFrame(Inputs())`.
     */
    HudFrame BuildHudFrame() const noexcept;

    /**
     * @brief Returns the current semantic HUD input sample.
     * @return Complete SI-unit input sample ready for `HudController`.
     */
    const HudInputSample& Inputs() const noexcept;

    /**
     * @brief Returns the current derived aircraft display state.
     * @return Aircraft state converted to HUD display units.
     */
    AircraftState Aircraft() const noexcept;

    /**
     * @brief Returns the current derived target display state.
     * @return Target state converted to HUD display units.
     */
    TargetState Target() const noexcept;

    /**
     * @brief Returns the current missile inventory.
     * @return Remaining missile counts by type.
     */
    const MissileInventory& Inventory() const noexcept;

    /**
     * @brief Returns the selected missile type.
     * @return Missile type used by launch gating and HUD text.
     */
    MissileType SelectedMissile() const noexcept;

    /**
     * @brief Returns currently tracked in-flight missiles.
     * @return Missile-shot list ordered by launch time.
     */
    const std::vector<MissileShot>& MissileShots() const noexcept;

    /**
     * @brief Returns the current master mode.
     * @return Active HUD master mode.
     */
    HudMasterMode MasterMode() const noexcept;

private:
    /** Semantic aircraft, target and weapon sample produced by the simulation. */
    HudInputSample inputs_ {};
    /** Latest pilot intent after UI and scripted maneuver collection. */
    PilotControls controls_ {};
    /** Low-pass filtered pitch command used to avoid frame-to-frame jumps. */
    float filteredPitchCommand_ = 0.0f;
    /** Low-pass filtered roll command used to avoid frame-to-frame jumps. */
    float filteredRollCommand_ = 0.0f;
    /** Active and recently impacted missiles retained for HUD timing/status. */
    std::vector<MissileShot> missileShots_ {};
};
} // namespace hud
