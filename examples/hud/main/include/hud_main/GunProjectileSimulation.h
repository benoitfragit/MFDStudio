/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include <array>
#include <cstddef>

#include "hud/HudTypes.h"
#include "hud_main/HudMathTypes.h"
#include "hud_main/HudPhysics.h"

namespace hud_main
{
/** @brief Fixed SI configuration of the virtual EEGS projectile model. */
struct GunProjectileConfig
{
    /** Muzzle velocity relative to the aircraft, meters per second. */
    double muzzleVelocityMps = 1036.0;
    /** Gun muzzle position in body axes X-forward, Y-right, Z-down, meters. */
    Vec3d muzzleOffsetBodyMeters {4.2, -0.25, 0.35};
    /** Down-positive gravitational acceleration, meters per second squared. */
    double gravityMps2 = 9.80665;
    /** Drag area times drag coefficient divided by mass, square meters per kilogram. */
    double dragAreaCoefficientOverMass = 0.00011;
};

/** @brief Aircraft launch state sampled at one fixed simulation tick. */
struct GunLaunchState
{
    Vec3d positionNedMeters {};
    Vec3d groundVelocityNedMps {};
    double yawRad = 0.0;
    double pitchRad = 0.0;
    double rollRad = 0.0;
};

/** @brief Owns and range-resamples the dense ballistic history of the sample HUD client. */
class GunProjectileSimulation
{
public:
    explicit GunProjectileSimulation(GunProjectileConfig config = {}) noexcept;

    /**
     * @brief Builds an immediately available 1.5-second deterministic history.
     * @param aircraft Current aircraft state; its state is assumed constant over the startup history.
     * @param environment Current uniform atmosphere and wind controls.
     */
    void Reset(const GunLaunchState& aircraft, const EnvironmentControls& environment) noexcept;

    /**
     * @brief Advances the ballistic history and launches one virtual projectile every 20 ms tick.
     * @param aircraft Aircraft state at the end of the same global tick.
     * @param environment Uniform atmosphere and wind field for this tick.
     */
    void Step(const GunLaunchState& aircraft, const EnvironmentControls& environment) noexcept;

    /**
     * @brief Publishes near-to-far fixed-range positions relative to the current aircraft in NED axes.
     * @param aircraftPositionNedMeters Current absolute aircraft position in the fixed local NED frame.
     * @return Sixteen regularly spaced SI trajectory stations from 600 to 3000 feet.
     * @note Interpolation is performed only between adjacent physical history samples;
     * the runtime receives no simulation state and performs no temporal filtering.
     */
    hud::GunTrajectoryInputSample BuildSnapshot(const Vec3d& aircraftPositionNedMeters) const noexcept;

private:
    static constexpr std::size_t kProjectileHistoryCapacity = 76U;

    struct ProjectileState
    {
        Vec3d positionNedMeters {};
        Vec3d velocityNedMps {};
        std::size_t ageTicks = 0U;
        bool valid = false;
    };

    void SpawnProjectile(ProjectileState& projectile, const GunLaunchState& aircraft) noexcept;
    void AdvanceProjectile(ProjectileState& projectile,
                           const Vec3d& windVelocityNedMps,
                           double airDensityKgPerM3) const noexcept;

    GunProjectileConfig config_ {};
    std::array<ProjectileState, kProjectileHistoryCapacity> projectiles_ {};
    std::size_t nextProjectileSlot_ = 0U;
};
} // namespace hud_main
