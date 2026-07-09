/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the pure physics helpers used by the HUD mini-simulation.
 */

#include "hud_main/HudPhysics.h"

#include <algorithm>
#include <cmath>

namespace hud_main
{
namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kKnotsToMetersPerSecond = 0.514444444f;
/** Specific gas constant of dry air in joules per kilogram-kelvin. */
constexpr float kDryAirGasConstantJPerKgK = 287.058f;
/** Heat capacity ratio (gamma) of dry air. */
constexpr float kDryAirHeatCapacityRatio = 1.4f;
constexpr float kHectopascalToPascal = 100.0f;
constexpr float kGravityMetersPerSecondSquared = 9.80665f;

/** Primary terrain undulation amplitude in meters; small so the user base dominates. */
constexpr float kTerrainUndulationPrimaryMeters = 18.0f;
/** Secondary terrain undulation amplitude in meters. */
constexpr float kTerrainUndulationSecondaryMeters = 9.0f;
/** Primary terrain undulation angular frequency in radians per second. */
constexpr float kTerrainUndulationPrimaryRadPerSecond = 0.031f;
/** Secondary terrain undulation angular frequency in radians per second. */
constexpr float kTerrainUndulationSecondaryRadPerSecond = 0.017f;

/** Horizontal gust amplitude per axis at full turbulence intensity, meters per second. */
constexpr float kGustHorizontalMps = 4.5f;
/** Vertical gust amplitude at full turbulence intensity, meters per second. */
constexpr float kGustVerticalMps = 1.8f;
/** Deliberately incommensurate gust frequencies so the pattern never locks visually. */
constexpr float kGustNorthRadPerSecond = 1.7f;
constexpr float kGustEastRadPerSecond = 2.3f;
constexpr float kGustDownRadPerSecond = 2.9f;
constexpr float kGustEastPhaseRad = 1.3f;
constexpr float kGustDownPhaseRad = 0.7f;

float FiniteOr(const float value, const float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}
} // namespace

float WrapRadiansTwoPi(float value) noexcept
{
    value = std::fmod(FiniteOr(value, 0.0f), kTwoPi);
    if (value < 0.0f)
    {
        value += kTwoPi;
    }

    return value;
}

WindVectorNed ComputeWindVectorNed(const float windSpeedKts, const float windDirectionRad) noexcept
{
    const float windSpeedMps = std::max(FiniteOr(windSpeedKts, 0.0f), 0.0f) * kKnotsToMetersPerSecond;
    const float directionRad = WrapRadiansTwoPi(windDirectionRad);

    WindVectorNed wind;
    // FROM direction: the air mass moves toward the opposite bearing.
    wind.northMps = -std::cos(directionRad) * windSpeedMps;
    wind.eastMps = -std::sin(directionRad) * windSpeedMps;
    wind.downMps = 0.0f;
    return wind;
}

WindVectorNed ComputeTurbulenceGustNed(const float turbulenceIntensity, const float elapsedSeconds) noexcept
{
    const float intensity = std::clamp(FiniteOr(turbulenceIntensity, 0.0f), 0.0f, 1.0f);
    const float seconds = FiniteOr(elapsedSeconds, 0.0f);

    WindVectorNed gust;
    gust.northMps = intensity * kGustHorizontalMps * std::sin(seconds * kGustNorthRadPerSecond);
    gust.eastMps = intensity * kGustHorizontalMps * std::sin(seconds * kGustEastRadPerSecond + kGustEastPhaseRad);
    gust.downMps = intensity * kGustVerticalMps * std::sin(seconds * kGustDownRadPerSecond + kGustDownPhaseRad);
    return gust;
}

float ComputeSpeedOfSoundMps(const float outsideAirTemperatureKelvin) noexcept
{
    if (!std::isfinite(outsideAirTemperatureKelvin) || outsideAirTemperatureKelvin <= 0.0f)
    {
        return 0.0f;
    }

    return std::sqrt(kDryAirHeatCapacityRatio * kDryAirGasConstantJPerKgK * outsideAirTemperatureKelvin);
}

float ComputeAirDensityKgPerM3(const float pressureHpa, const float outsideAirTemperatureKelvin) noexcept
{
    if (!std::isfinite(pressureHpa) || pressureHpa <= 0.0f ||
        !std::isfinite(outsideAirTemperatureKelvin) || outsideAirTemperatureKelvin <= 0.0f)
    {
        return 0.0f;
    }

    return pressureHpa * kHectopascalToPascal / (kDryAirGasConstantJPerKgK * outsideAirTemperatureKelvin);
}

float ComputeMach(const float trueAirspeedMps, const float speedOfSoundMps) noexcept
{
    if (!std::isfinite(trueAirspeedMps) || trueAirspeedMps <= 0.0f ||
        !std::isfinite(speedOfSoundMps) || speedOfSoundMps <= 0.0f)
    {
        return 0.0f;
    }

    return trueAirspeedMps / speedOfSoundMps;
}

float ComputeRadioAltitudeMeters(const float altitudeMeters, const float terrainElevationMeters) noexcept
{
    return std::max(FiniteOr(altitudeMeters, 0.0f) - FiniteOr(terrainElevationMeters, 0.0f), 0.0f);
}

float ComputeSpecificEnergyRateMps(const float trueAirspeedMps,
                                   const float verticalSpeedMps,
                                   const float longitudinalAccelerationMps2) noexcept
{
    return FiniteOr(verticalSpeedMps, 0.0f) +
           FiniteOr(trueAirspeedMps, 0.0f) * FiniteOr(longitudinalAccelerationMps2, 0.0f) /
               kGravityMetersPerSecondSquared;
}

float ComputeTerrainElevationMeters(const EnvironmentControls& environment,
                                    const float elapsedSeconds,
                                    const float headingRad) noexcept
{
    const EnvironmentControls defaults {};
    const float baseMeters = FiniteOr(environment.terrainElevationMeters, defaults.terrainElevationMeters);
    const float seconds = FiniteOr(elapsedSeconds, 0.0f);
    const float heading = FiniteOr(headingRad, 0.0f);
    return baseMeters +
           kTerrainUndulationPrimaryMeters * std::sin(seconds * kTerrainUndulationPrimaryRadPerSecond + heading) +
           kTerrainUndulationSecondaryMeters * std::sin(seconds * kTerrainUndulationSecondaryRadPerSecond);
}
} // namespace hud_main
