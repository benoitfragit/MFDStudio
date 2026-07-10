/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Deterministic mini-simulation and sample armament owned by the
 * interactive HUD client.
 *
 * Everything in this header is main-client scenario material: pilot controls,
 * environment settings, the AIM-120C/AIM-9M sample armament and the
 * launch-zone/time-of-flight models. None of it belongs to the reusable
 * `hud_runtime` contract; the simulation resolves these facts and hands them
 * to the HUD through the generic `hud::HudInputSample` fields.
 */

#include <string>
#include <vector>

#include "hud/HudProjection.h"
#include "hud_main/HudPhysics.h"

namespace hud_main
{
/**
 * @brief Missile type selectable in the sample A-A inventory.
 *
 * This is main-client armament, not a HUD contract: the reusable HUD runtime
 * only receives the resolved label, mnemonic, quantity, launch zone and time
 * of flight through `hud::WeaponInputSample`.
 */
enum class MissileType
{
    /** Short-range infrared missile cue. */
    Aim9M,
    /** Medium-range radar missile cue. */
    Aim120C
};

/**
 * @brief Missile inventory owned by the sample client.
 */
struct MissileInventory
{
    /** Remaining AIM-9M missiles. */
    int aim9m = 2;
    /** Remaining AIM-120C missiles. */
    int aim120c = 4;
};

/**
 * @brief Computes dynamic launch zone values from physical SI inputs.
 * @param aircraft SI-unit aircraft input sample.
 * @param target SI-unit target input sample.
 * @param selectedMissile Selected sample missile type.
 * @return Simplified but ordered DLZ values in nautical miles.
 * @note Sample armament model. A production avionics integration computes its
 * own launch zone and fills `hud::WeaponInputSample::launchZone` directly.
 */
hud::LaunchZone ComputeLaunchZone(const hud::AircraftInputSample& aircraft,
                                  const hud::TargetInputSample& target,
                                  MissileType selectedMissile) noexcept;

/**
 * @brief Computes the sample missile time of flight from physical SI inputs.
 * @param aircraft SI-unit aircraft input sample.
 * @param target SI-unit target input sample.
 * @param selectedMissile Selected sample missile type.
 * @return Time of flight in seconds, bounded by the selected sample profile.
 */
float ComputeMissileTimeOfFlight(const hud::AircraftInputSample& aircraft,
                                 const hud::TargetInputSample& target,
                                 MissileType selectedMissile) noexcept;

/**
 * @brief Returns the HUD inventory label of one sample missile type.
 * @param selectedMissile Missile type.
 * @return `"AIM-120C"` or `"AIM-9M"`.
 */
const char* MissileLabel(MissileType selectedMissile) noexcept;

/**
 * @brief Returns the short HUD mnemonic of one sample missile type.
 * @param selectedMissile Missile type.
 * @return `"MRM"` for AIM-120C and `"SRM"` for AIM-9M.
 */
const char* MissileMnemonic(MissileType selectedMissile) noexcept;

/**
 * @brief Returns the missile diamond scale of one sample missile type.
 * @param selectedMissile Missile type.
 * @return Scale multiplier applied to the HUD missile diamond.
 */
float MissileDiamondScale(MissileType selectedMissile) noexcept;

/**
 * @brief Formats the selected missile label plus remaining quantity.
 * @param selectedMissile Missile type whose remaining count should be shown.
 * @param inventory Current missile inventory.
 * @return HUD text such as `"AIM-120C 4"` or `"AIM-9M 2"`.
 */
std::string FormatMissileInventory(MissileType selectedMissile, const MissileInventory& inventory);

/**
 * @brief Pilot controls sampled once per simulation step.
 *
 * These values are sample-only inputs for the bundled mini-simulation. They are
 * intentionally not a HUD integration contract: a real aircraft adapter should
 * provide `hud::HudInputSample` directly through `hud/HudRuntimeClient.h`.
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
    hud::HudMasterMode masterMode = hud::HudMasterMode::Nav;
    /** Panel-selected weapon symbology mode. */
    hud::HudWeaponMode weaponMode = hud::HudWeaponMode::None;
    /** Panel-selected gun sight mode. */
    hud::HudGunMode gunMode = hud::HudGunMode::None;
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
 * @brief Complete control level consumed by the mini-simulation each step.
 *
 * Groups the pilot intent (`PilotControls`) and the environment settings
 * (`EnvironmentControls` from `hud_main/HudPhysics.h`) collected by the
 * interactive client. Like both members, this is sample-only material: a real
 * integration replaces the whole simulation with its own INU/air-data producer
 * and fills `hud::HudInputSample` directly.
 */
struct SimulationControls
{
    /** Pilot stick, throttle and panel intent. */
    PilotControls pilot {};
    /** Wind, atmosphere and terrain settings. */
    EnvironmentControls environment {};
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
    hud::MissileFlightPhase phase = hud::MissileFlightPhase::Boost;
};

/**
 * @brief Stable HUD-focused HUD mini-simulation for the interactive HUD client.
 *
 * @note This model is deliberately simplified. It preserves coherent relations
 * between attitude, energy, speed, altitude, wind, atmosphere, terrain, A-A
 * mode and missile symbology rather than attempting to be a flight dynamics
 * simulator. Its only integration output is `hud::HudInputSample`, so a real
 * INU/air-data/environment producer can replace this class and keep the same
 * HUD runtime, controller and funnel projection.
 */
class HudSimulation
{
public:
    HudSimulation();

    /**
     * @brief Restores aircraft, target, inventory and in-flight missiles.
     * @post `Inputs()` returns the default semantic HUD sample.
     */
    void Reset();

    /**
     * @brief Replaces the pilot and environment controls used by the next call to `Step`.
     * @param controls Latest pilot intent and environment settings; values are
     * clamped/wrapped into their valid ranges before being stored.
     */
    void SetSimulationControls(const SimulationControls& controls) noexcept;

    /**
     * @brief Advances the simulation by one bounded deterministic step.
     *
     * Pilot commands are read from `SimulationControls::pilot`, wind,
     * atmosphere and terrain from `SimulationControls::environment`. The
     * aircraft NED velocity written into `hud::AircraftInputSample` is the
     * ground velocity (air velocity plus wind), so wind shows up in the HUD
     * only through the resolved physical data.
     *
     * @param deltaSeconds Wall-clock delta in seconds; non-finite and negative
     * values are discarded, very large values are clamped.
     */
    void Step(float deltaSeconds);

    /**
     * @brief Selects the active missile type shown by the HUD.
     * @param type Missile type to select.
     * @post `SelectedMissile()` matches `type` and the generic weapon
     * presentation in `Inputs()` is refreshed.
     */
    void SelectMissile(MissileType type);

    /**
     * @brief Cycles to the next available missile type.
     * @post `SelectedMissile()` is toggled between supported missile types.
     */
    void CycleSelectedMissile();

    /**
     * @brief Attempts to launch the selected missile through simulated launch gates.
     * @return `true` if inventory was consumed and a missile shot was created.
     */
    bool FireSelectedMissile();

    /**
     * @brief Builds the current projected HUD frame from the simulation input sample.
     * @return Stateless HUD projection equivalent to `hud::BuildHudFrame(Inputs())`.
     */
    hud::HudFrame BuildHudFrame() const noexcept;

    /**
     * @brief Returns the current semantic HUD input sample.
     * @return Complete SI-unit input sample ready for `hud::HudRuntimeClient`.
     */
    const hud::HudInputSample& Inputs() const noexcept;

    /**
     * @brief Returns the current derived aircraft display state.
     * @return Aircraft state converted to HUD display units.
     */
    hud::AircraftState Aircraft() const noexcept;

    /**
     * @brief Returns the current derived target display state.
     * @return Target state converted to HUD display units.
     */
    hud::TargetState Target() const noexcept;

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
    hud::HudMasterMode MasterMode() const noexcept;

private:
    /**
     * @brief Resolves the sample armament into the generic weapon presentation.
     *
     * Fills the label, mnemonic, quantity, diamond scale, launch zone and time
     * of flight expected by `hud::WeaponInputSample`. This is exactly what a
     * real avionics integration does before publishing a HUD frame.
     */
    void RefreshWeaponPresentation();

    /** Semantic aircraft, target and weapon sample produced by the simulation. */
    hud::HudInputSample inputs_ {};
    /** Latest pilot intent and environment settings after UI collection. */
    SimulationControls controls_ {};
    /** Missile type currently selected by the sample client. */
    MissileType selectedMissile_ = MissileType::Aim120C;
    /** Remaining sample inventory by missile type. */
    MissileInventory inventory_ {};
    /** Low-pass filtered pitch command used to avoid frame-to-frame jumps. */
    float filteredPitchCommand_ = 0.0f;
    /** Low-pass filtered roll command used to avoid frame-to-frame jumps. */
    float filteredRollCommand_ = 0.0f;
    /** Air-mass-relative speed in meters per second; the published NED velocity adds the wind. */
    float trueAirspeedMps_ = 0.0f;
    /** Smoothed air-mass flight-path slope in radians, positive while climbing. */
    float flightPathSlopeRad_ = 0.0f;
    /** Active and recently impacted missiles retained for HUD timing/status. */
    std::vector<MissileShot> missileShots_ {};
};
} // namespace hud_main
