/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "hud_main/GunProjectileSimulation.h"

#include "hud_main/HudSimulationTime.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace hud_main
{
namespace
{
constexpr double kMaximumProjectileSpeedMps = 2500.0;
constexpr double kMaximumAccelerationMps2 = 2000.0;
constexpr double kFeetToMeters = 0.3048;
constexpr double kRangeInterpolationEpsilonMeters = 0.000001;
constexpr double kMinimumFunnelRangeFeet = 600.0;
constexpr double kMaximumFunnelRangeFeet = 3000.0;

constexpr std::array<double, hud::kGunTrajectoryPointCount> BuildFunnelRangeStationsMeters() noexcept
{
    static_assert(hud::kGunTrajectoryPointCount >= 16U, "EEGS rails require at least sixteen ballistic stations.");
    std::array<double, hud::kGunTrajectoryPointCount> stations {};
    const double rangeStepFeet =
        (kMaximumFunnelRangeFeet - kMinimumFunnelRangeFeet) /
        static_cast<double>(stations.size() - 1U);
    for (std::size_t index = 0U; index < stations.size(); ++index)
    {
        stations[index] =
            (kMinimumFunnelRangeFeet + rangeStepFeet * static_cast<double>(index)) * kFeetToMeters;
    }
    return stations;
}

// The semantic boundary is ordered near-to-far from 600 to 3000 feet. Display
// width and HUD anchoring remain runtime concerns; the dense projectile history
// itself remains private to the simulation.
constexpr std::array<double, hud::kGunTrajectoryPointCount> kFunnelRangeStationsMeters =
    BuildFunnelRangeStationsMeters();

struct RelativeProjectileSample
{
    Vec3d positionNedMeters {};
    double slantRangeMeters = 0.0;
    double ageSeconds = 0.0;
    bool valid = false;
};

bool IsFinite(const Vec3d& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Vec3d Add(const Vec3d& left, const Vec3d& right) noexcept
{
    return Vec3d {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3d Scale(const Vec3d& value, const double scale) noexcept
{
    return Vec3d {value.x * scale, value.y * scale, value.z * scale};
}

double Magnitude(const Vec3d& value) noexcept
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vec3d Subtract(const Vec3d& left, const Vec3d& right) noexcept
{
    return Vec3d {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vec3d Interpolate(const Vec3d& first, const Vec3d& second, const double amount) noexcept
{
    return Add(first, Scale(Subtract(second, first), amount));
}

RelativeProjectileSample BuildRelativeSample(const Vec3d& projectilePositionNedMeters,
                                              const std::size_t ageTicks,
                                              const bool projectileValid,
                                              const Vec3d& aircraftPositionNedMeters) noexcept
{
    RelativeProjectileSample sample;
    sample.positionNedMeters = Subtract(projectilePositionNedMeters, aircraftPositionNedMeters);
    sample.slantRangeMeters = Magnitude(sample.positionNedMeters);
    sample.ageSeconds = static_cast<double>(ageTicks) * kHudSimulationStepSeconds;
    sample.valid = projectileValid && IsFinite(sample.positionNedMeters) &&
                   std::isfinite(sample.slantRangeMeters) && std::isfinite(sample.ageSeconds);
    return sample;
}

bool BracketsRange(const RelativeProjectileSample& farther,
                   const RelativeProjectileSample& nearer,
                   const double rangeMeters) noexcept
{
    return farther.valid && nearer.valid && farther.slantRangeMeters >= rangeMeters &&
           nearer.slantRangeMeters <= rangeMeters;
}

hud::GunTrajectoryPointNed InterpolateRangeStation(const RelativeProjectileSample& farther,
                                                   const RelativeProjectileSample& nearer,
                                                   const double rangeMeters) noexcept
{
    hud::GunTrajectoryPointNed point;
    const double rangeDelta = nearer.slantRangeMeters - farther.slantRangeMeters;
    if (!std::isfinite(rangeDelta) || std::fabs(rangeDelta) <= kRangeInterpolationEpsilonMeters)
    {
        return point;
    }

    const double interpolation = std::clamp(
        (rangeMeters - farther.slantRangeMeters) / rangeDelta,
        0.0,
        1.0);
    const Vec3d position = Interpolate(
        farther.positionNedMeters,
        nearer.positionNedMeters,
        interpolation);
    const double ageSeconds =
        farther.ageSeconds + (nearer.ageSeconds - farther.ageSeconds) * interpolation;
    if (!IsFinite(position) || !std::isfinite(ageSeconds))
    {
        return point;
    }

    point.northMeters = static_cast<float>(position.x);
    point.eastMeters = static_cast<float>(position.y);
    point.downMeters = static_cast<float>(position.z);
    point.ageSeconds = static_cast<float>(ageSeconds);
    point.valid = std::isfinite(point.northMeters) && std::isfinite(point.eastMeters) &&
                  std::isfinite(point.downMeters) && std::isfinite(point.ageSeconds);
    return point;
}

Vec3d BodyToNed(const GunLaunchState& aircraft, const Vec3d& body) noexcept
{
    const double cy = std::cos(aircraft.yawRad);
    const double sy = std::sin(aircraft.yawRad);
    const double cp = std::cos(aircraft.pitchRad);
    const double sp = std::sin(aircraft.pitchRad);
    const double cr = std::cos(aircraft.rollRad);
    const double sr = std::sin(aircraft.rollRad);
    return Vec3d {
        cy * cp * body.x + (cy * sp * sr - sy * cr) * body.y + (cy * sp * cr + sy * sr) * body.z,
        sy * cp * body.x + (sy * sp * sr + cy * cr) * body.y + (sy * sp * cr - cy * sr) * body.z,
        -sp * body.x + cp * sr * body.y + cp * cr * body.z};
}

EnvironmentControls SanitizedEnvironment(const EnvironmentControls& source) noexcept
{
    EnvironmentControls environment = source;
    if (!std::isfinite(environment.windSpeedKts) || environment.windSpeedKts < 0.0f)
    {
        environment.windSpeedKts = 0.0f;
    }
    if (!std::isfinite(environment.windDirectionRad))
    {
        environment.windDirectionRad = 0.0f;
    }
    return environment;
}

Vec3d WindVelocityNedMetersPerSecond(const EnvironmentControls& environment) noexcept
{
    const WindVectorNed wind = ComputeWindVectorNed(
        environment.windSpeedKts,
        environment.windDirectionRad);
    return Vec3d {
        static_cast<double>(wind.northMps),
        static_cast<double>(wind.eastMps),
        static_cast<double>(wind.downMps)};
}
} // namespace

GunProjectileSimulation::GunProjectileSimulation(GunProjectileConfig config) noexcept
    : config_(config)
{
    if (!std::isfinite(config_.muzzleVelocityMps) || config_.muzzleVelocityMps < 0.0)
    {
        config_.muzzleVelocityMps = 0.0;
    }
    if (!IsFinite(config_.muzzleOffsetBodyMeters))
    {
        config_.muzzleOffsetBodyMeters = {};
    }
    if (!std::isfinite(config_.gravityMps2) || config_.gravityMps2 < 0.0)
    {
        config_.gravityMps2 = 9.80665;
    }
    if (!std::isfinite(config_.dragAreaCoefficientOverMass) || config_.dragAreaCoefficientOverMass < 0.0)
    {
        config_.dragAreaCoefficientOverMass = 0.0;
    }
}

void GunProjectileSimulation::Reset(const GunLaunchState& aircraft,
                                    const EnvironmentControls& environment) noexcept
{
    projectiles_ = {};
    const EnvironmentControls safeEnvironment = SanitizedEnvironment(environment);
    const Vec3d windVelocityNedMps = WindVelocityNedMetersPerSecond(safeEnvironment);
    const double airDensityKgPerM3 = static_cast<double>(ComputeAirDensityKgPerM3(
        safeEnvironment.pressureHpa,
        safeEnvironment.outsideAirTemperatureKelvin));
    for (std::size_t index = 0U; index < projectiles_.size(); ++index)
    {
        const std::size_t ageTicks = projectiles_.size() - 1U - index;
        GunLaunchState historicalAircraft = aircraft;
        historicalAircraft.positionNedMeters = Add(
            aircraft.positionNedMeters,
            Scale(aircraft.groundVelocityNedMps, -static_cast<double>(ageTicks) * kHudSimulationStepSeconds));
        SpawnProjectile(projectiles_[index], historicalAircraft);
        for (std::size_t tick = 0U; tick < ageTicks; ++tick)
        {
            AdvanceProjectile(projectiles_[index], windVelocityNedMps, airDensityKgPerM3);
            ++projectiles_[index].ageTicks;
        }
    }
    nextProjectileSlot_ = 0U;
}

void GunProjectileSimulation::Step(const GunLaunchState& aircraft,
                                   const EnvironmentControls& environment) noexcept
{
    const EnvironmentControls safeEnvironment = SanitizedEnvironment(environment);
    const Vec3d windVelocityNedMps = WindVelocityNedMetersPerSecond(safeEnvironment);
    const double airDensityKgPerM3 = static_cast<double>(ComputeAirDensityKgPerM3(
        safeEnvironment.pressureHpa,
        safeEnvironment.outsideAirTemperatureKelvin));
    for (ProjectileState& projectile : projectiles_)
    {
        if (projectile.valid)
        {
            AdvanceProjectile(projectile, windVelocityNedMps, airDensityKgPerM3);
            ++projectile.ageTicks;
        }
    }

    SpawnProjectile(projectiles_[nextProjectileSlot_], aircraft);
    nextProjectileSlot_ = (nextProjectileSlot_ + 1U) % projectiles_.size();
}

hud::GunTrajectoryInputSample GunProjectileSimulation::BuildSnapshot(
    const Vec3d& aircraftPosition) const noexcept
{
    hud::GunTrajectoryInputSample snapshot;
    std::array<RelativeProjectileSample, kProjectileHistoryCapacity> history {};
    for (std::size_t outputIndex = 0U; outputIndex < projectiles_.size(); ++outputIndex)
    {
        const std::size_t storageIndex = (nextProjectileSlot_ + outputIndex) % projectiles_.size();
        const ProjectileState& projectile = projectiles_[storageIndex];
        history[outputIndex] = BuildRelativeSample(
            projectile.positionNedMeters,
            projectile.ageTicks,
            projectile.valid,
            aircraftPosition);
    }

    for (std::size_t stationIndex = 0U; stationIndex < kFunnelRangeStationsMeters.size(); ++stationIndex)
    {
        const double stationRangeMeters = kFunnelRangeStationsMeters[stationIndex];
        for (std::size_t historyIndex = 0U; historyIndex + 1U < history.size(); ++historyIndex)
        {
            const RelativeProjectileSample& farther = history[historyIndex];
            const RelativeProjectileSample& nearer = history[historyIndex + 1U];
            if (BracketsRange(farther, nearer, stationRangeMeters))
            {
                snapshot.points[stationIndex] = InterpolateRangeStation(
                    farther,
                    nearer,
                    stationRangeMeters);
                break;
            }
        }
    }
    return snapshot;
}

void GunProjectileSimulation::SpawnProjectile(ProjectileState& projectile,
                                              const GunLaunchState& aircraft) noexcept
{
    projectile = {};
    if (!IsFinite(aircraft.positionNedMeters) || !IsFinite(aircraft.groundVelocityNedMps) ||
        !std::isfinite(aircraft.yawRad) || !std::isfinite(aircraft.pitchRad) || !std::isfinite(aircraft.rollRad))
    {
        return;
    }
    projectile.positionNedMeters = Add(
        aircraft.positionNedMeters,
        BodyToNed(aircraft, config_.muzzleOffsetBodyMeters));
    projectile.velocityNedMps = Add(
        aircraft.groundVelocityNedMps,
        BodyToNed(aircraft, Vec3d {config_.muzzleVelocityMps, 0.0, 0.0}));
    projectile.valid = IsFinite(projectile.positionNedMeters) && IsFinite(projectile.velocityNedMps);
}

void GunProjectileSimulation::AdvanceProjectile(ProjectileState& projectile,
                                                const Vec3d& windVelocityNedMps,
                                                const double airDensityKgPerM3) const noexcept
{
    const Vec3d relativeAirVelocity {
        projectile.velocityNedMps.x - windVelocityNedMps.x,
        projectile.velocityNedMps.y - windVelocityNedMps.y,
        projectile.velocityNedMps.z - windVelocityNedMps.z};
    const double speed = Magnitude(relativeAirVelocity);
    if (!std::isfinite(speed) || speed > kMaximumProjectileSpeedMps ||
        !std::isfinite(airDensityKgPerM3) || airDensityKgPerM3 < 0.0)
    {
        projectile.valid = false;
        return;
    }
    const double dragScale = -0.5 * airDensityKgPerM3 * config_.dragAreaCoefficientOverMass * speed;
    Vec3d acceleration {
        relativeAirVelocity.x * dragScale,
        relativeAirVelocity.y * dragScale,
        config_.gravityMps2 + relativeAirVelocity.z * dragScale};
    const double accelerationMagnitude = Magnitude(acceleration);
    if (!IsFinite(acceleration) || accelerationMagnitude > kMaximumAccelerationMps2)
    {
        projectile.valid = false;
        return;
    }
    projectile.velocityNedMps = Add(projectile.velocityNedMps, Scale(acceleration, kHudSimulationStepSeconds));
    projectile.positionNedMeters = Add(projectile.positionNedMeters, Scale(projectile.velocityNedMps, kHudSimulationStepSeconds));
    projectile.valid = IsFinite(projectile.positionNedMeters) && IsFinite(projectile.velocityNedMps);
}
} // namespace hud_main
