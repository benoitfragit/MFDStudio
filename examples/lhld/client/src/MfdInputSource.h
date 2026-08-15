/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Replaceable semantic input-source contract for the LHLD client.
 */

#include "MfdTypes.h"

namespace lhld
{
/**
 * @brief Producer of one complete semantic LHLD MFD sample.
 *
 * Implement this interface to replace the bundled fake radar simulation with a
 * real aircraft, avionics or network-fed source. The generated UI and the
 * controller only consume @ref MfdInputSample, so implementations should keep
 * raw cockpit commands, transport details and simulator-specific state behind
 * this boundary.
 *
 * @note A radar implementation is responsible for publishing authoritative
 * @ref RadarTrack::active and @ref RadarTrack::state values. Detection volume,
 * track loss, designation and STT lifecycle must be resolved before the sample
 * reaches the projection/controller page code.
 */
class MfdInputSource
{
public:
    virtual ~MfdInputSource() = default;

    /**
     * @brief Restores the source to its startup state.
     */
    virtual void Reset() noexcept = 0;

    /**
     * @brief Applies operator radar controls gathered by the demo panel.
     * @param controls Panel-owned radar controls.
     */
    virtual void ApplyRadarControls(const RadarSettings& controls) noexcept = 0;

    /**
     * @brief Applies the selected master mode.
     * @param mode Requested aircraft master mode.
     */
    virtual void SetMasterMode(MasterMode mode) noexcept = 0;

    /**
     * @brief Applies the stores state owned by the demo panel.
     * @param stores Stores-management state to publish.
     */
    virtual void SetStores(const StoresState& stores) noexcept = 0;

    /**
     * @brief Applies the navigation state gathered by the panel.
     * @param nav Panel-owned HSD range, current steerpoint and flight plan.
     */
    virtual void SetNavState(const NavState& nav) noexcept = 0;

    /**
     * @brief Advances the source by one frame.
     * @param deltaSeconds Host frame delta in seconds.
     */
    virtual void Step(float deltaSeconds) noexcept = 0;

    /**
     * @brief Returns the complete semantic sample for the next publish pass.
     */
    virtual const MfdInputSample& Inputs() const noexcept = 0;
};
} // namespace lhld
