/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Pure, deterministic physics helpers for the HUD mini-simulation.
 *
 * Everything in this header is main-client scenario material: it produces the
 * physical facts (wind vector, air data, terrain, energy) that
 * `hud_main::HudSimulation` writes into `hud::HudInputSample`. None of it
 * belongs to the reusable `hud_runtime` contract, which stays a passive
 * consumer of already-resolved physical data.
 *
 * The model is deliberately simplified: deterministic, stable and pedagogic,
 * not a certifiable atmosphere or flight-dynamics reference.
 */

namespace hud_main
{
/**
 * @brief Environment settings sampled once per simulation step.
 *
 * These values are sample-only inputs for the bundled mini-simulation, exactly
 * like `PilotControls`. A real integration replaces them with its own
 * environment/air-data producer and fills `hud::HudInputSample` directly.
 *
 * @note `windDirectionRad` uses the meteorological FROM convention: 0 means
 * wind coming from the North (blowing toward the South), pi/2 (90 degrees)
 * wind coming from the East, pi (180 degrees) wind coming from the South and
 * 3*pi/2 (270 degrees) wind coming from the West.
 */
struct EnvironmentControls
{
    /** Wind speed in knots. */
    float windSpeedKts = 0.0f;
    /** Wind FROM direction in radians; 0 = wind from the North. */
    float windDirectionRad = 0.0f;
    /** Normalized turbulence intensity in [0, 1]; 0 disables every gust term. */
    float turbulenceIntensity = 0.0f;
    /** Base terrain elevation in meters MSL under the aircraft. */
    float terrainElevationMeters = 128.0f;
    /** Outside air temperature in kelvin (standard atmosphere sea level). */
    float outsideAirTemperatureKelvin = 288.15f;
    /** Static pressure in hectopascals (standard atmosphere sea level). */
    float pressureHpa = 1013.25f;
};

/**
 * @brief Wind velocity vector in the NED navigation frame, meters per second.
 */
struct WindVectorNed
{
    /** North wind velocity component in meters per second. */
    float northMps = 0.0f;
    /** East wind velocity component in meters per second. */
    float eastMps = 0.0f;
    /** Down wind velocity component in meters per second. */
    float downMps = 0.0f;
};

/**
 * @brief Wraps an angle into [0, 2*pi).
 * @param value Angle in radians; non-finite values collapse to 0.
 * @return Equivalent angle in [0, 2*pi).
 */
float WrapRadiansTwoPi(float value) noexcept;

/**
 * @brief Converts a FROM wind direction and speed into a NED wind vector.
 *
 * The FROM convention means the air mass moves toward the opposite bearing:
 * a 0-degree (North) wind produces a negative north component, a 90-degree
 * (East) wind produces a negative east component.
 *
 * @param windSpeedKts Wind speed in knots; non-finite or negative values give a zero vector.
 * @param windDirectionRad Wind FROM direction in radians; 0 = wind from the North.
 * @return Wind velocity in the NED frame, meters per second, with a zero down component.
 */
WindVectorNed ComputeWindVectorNed(float windSpeedKts, float windDirectionRad) noexcept;

/**
 * @brief Deterministic turbulence gust added on top of the steady wind.
 *
 * The gust is a sum of bounded sinusoids of `elapsedSeconds`, so two runs with
 * the same inputs produce the same trajectory: there is no random source.
 *
 * @param turbulenceIntensity Normalized intensity, clamped into [0, 1]; 0 returns a zero vector.
 * @param elapsedSeconds Deterministic simulation time in seconds.
 * @return Bounded gust velocity in the NED frame, meters per second.
 */
WindVectorNed ComputeTurbulenceGustNed(float turbulenceIntensity, float elapsedSeconds) noexcept;

/**
 * @brief Speed of sound from the outside air temperature (ideal dry air).
 * @param outsideAirTemperatureKelvin Static air temperature in kelvin.
 * @return Speed of sound in meters per second, or 0 for non-finite or non-positive temperatures.
 */
float ComputeSpeedOfSoundMps(float outsideAirTemperatureKelvin) noexcept;

/**
 * @brief Air density from the simplified ideal-gas law.
 * @param pressureHpa Static pressure in hectopascals.
 * @param outsideAirTemperatureKelvin Static air temperature in kelvin.
 * @return Density in kilograms per cubic meter, or 0 for invalid inputs.
 */
float ComputeAirDensityKgPerM3(float pressureHpa, float outsideAirTemperatureKelvin) noexcept;

/**
 * @brief Mach number from true airspeed and speed of sound.
 * @param trueAirspeedMps True airspeed in meters per second.
 * @param speedOfSoundMps Local speed of sound in meters per second.
 * @return `trueAirspeedMps / speedOfSoundMps`, or 0 when either input is non-finite or non-positive.
 */
float ComputeMach(float trueAirspeedMps, float speedOfSoundMps) noexcept;

/**
 * @brief Radio (radar) altitude above the local terrain.
 * @param altitudeMeters Aircraft altitude in meters MSL.
 * @param terrainElevationMeters Terrain elevation in meters MSL under the aircraft.
 * @return `altitudeMeters - terrainElevationMeters` clamped to a minimum of 0.
 */
float ComputeRadioAltitudeMeters(float altitudeMeters, float terrainElevationMeters) noexcept;

/**
 * @brief Specific energy rate `h_dot + V * V_dot / g` in meters per second.
 * @param trueAirspeedMps True airspeed in meters per second.
 * @param verticalSpeedMps Climb rate in meters per second, positive up.
 * @param longitudinalAccelerationMps2 Along-path acceleration in meters per second squared.
 * @return Specific energy rate; non-finite inputs contribute 0.
 */
float ComputeSpecificEnergyRateMps(float trueAirspeedMps,
                                   float verticalSpeedMps,
                                   float longitudinalAccelerationMps2) noexcept;

/**
 * @brief Terrain elevation under the aircraft for the mini-simulation.
 *
 * The user-selected `environment.terrainElevationMeters` is the dominant base
 * value; a small deterministic sinusoidal undulation (a few tens of meters,
 * derived from `elapsedSeconds` and `headingRad`) is added on top so the radar
 * altitude keeps moving during flight. The undulation amplitude is fixed and
 * bounded, so the user setting always dominates the result.
 *
 * @param environment Environment settings providing the terrain base elevation.
 * @param elapsedSeconds Deterministic simulation time in seconds.
 * @param headingRad Aircraft heading in radians, used to vary terrain along the route.
 * @return Terrain elevation in meters MSL.
 */
float ComputeTerrainElevationMeters(const EnvironmentControls& environment,
                                    float elapsedSeconds,
                                    float headingRad) noexcept;
} // namespace hud_main
