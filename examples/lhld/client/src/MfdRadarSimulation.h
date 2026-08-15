/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Deterministic mini radar/airspace simulation for the LHLD client.
 *
 * @note This model is deliberately simplified. It keeps coherent relations
 * between track geometry, closure and aspect rather than being a sensor
 * fidelity model. Its only integration output is @ref MfdInputSample, so a real
 * avionics producer can replace this class and keep the same projection,
 * controller and generated UI. Operator controls (radar scan, master mode,
 * stores, navigation) are pushed in from the panel and are never mutated here.
 */

#include "MfdInputSource.h"

namespace lhld
{
/**
 * @brief Deterministic airspace/radar producer for the demo MFD pages.
 */
class MfdRadarSimulation final : public MfdInputSource
{
public:
    MfdRadarSimulation();

    /**
     * @brief Restores ownship, airspace, stores and navigation to demo defaults.
     * @post @ref Inputs returns the default semantic MFD sample.
     */
    void Reset() noexcept override;

    /**
     * @brief Copies operator radar controls into the next published sample.
     * @param controls Panel-owned radar controls; the animated sweep is preserved.
     */
    void ApplyRadarControls(const RadarSettings& controls) noexcept override;

    /**
     * @brief Selects the shared aircraft master mode.
     * @param mode Master mode requested by the panel.
     */
    void SetMasterMode(MasterMode mode) noexcept override;

    /**
     * @brief Copies operator stores-management controls into the sample.
     * @param stores Panel-owned stores state.
     */
    void SetStores(const StoresState& stores) noexcept override;

    /**
     * @brief Copies operator navigation controls and flight plan into the sample.
     * @param nav Panel-owned navigation state.
     */
    void SetNavState(const NavState& nav) noexcept override;

    /**
     * @brief Advances the airspace and sweep by one bounded deterministic step.
     * @param deltaSeconds Wall-clock delta; non-finite or negative values are
     * discarded and large values are clamped.
     */
    void Step(float deltaSeconds) noexcept override;

    /**
     * @brief Returns the current semantic MFD input sample.
     */
    const MfdInputSample& Inputs() const noexcept override;

private:
    /**
     * @brief One airspace track expressed in a nose-relative Cartesian frame.
     */
    struct SimTrack
    {
        /** @brief True when the track exists in the airspace. */
        bool alive = false;
        /** @brief Right offset from ownship nose in nautical miles. */
        float rightNm = 0.0f;
        /** @brief Forward offset from ownship nose in nautical miles. */
        float forwardNm = 0.0f;
        /** @brief Rightward velocity in nautical miles per second. */
        float velRightNmS = 0.0f;
        /** @brief Forward velocity in nautical miles per second. */
        float velForwardNmS = 0.0f;
        /** @brief Track altitude in feet MSL. */
        float altitudeFt = 20000.0f;
        /** @brief True when the track is classified hostile. */
        bool hostile = true;
    };

    /**
     * @brief Recomputes radar-owned lifecycle and measurements from airspace.
     * @note This is the sole owner of detection-volume and track suppression
     * rules in the demo; downstream page code only presents this result.
     */
    void RefreshPublishedTracks() noexcept;

    /** @brief Semantic sample produced for the projection and controller. */
    MfdInputSample inputs_ {};
    /** @brief Nose-relative airspace tracks integrated each step. */
    std::array<SimTrack, kMaxRadarTracks> airspace_ {};
    /** @brief Animated antenna azimuth normalized to the active scan width. */
    float sweepFraction_ = 0.0f;
    /** @brief Sweep travel direction, +1 or -1. */
    float sweepDirection_ = 1.0f;
    /** @brief Bounded simulation clock used for deterministic track quality. */
    float elapsedSeconds_ = 0.0f;
};
} // namespace lhld
