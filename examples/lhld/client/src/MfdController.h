/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Adapter from semantic LHLD inputs to the generated MFD UI.
 */

#include <array>
#include <string>

#include "MfdTypes.h"

namespace lhld_ui
{
class LhldUi;
class RadarBuggedTrackDynamicReticle;
class RadarRwsTrackDynamicReticle;
class RadarSttTrackDynamicReticle;
class RadarTwsTrackDynamicReticle;
}

namespace lhld
{
/**
 * @brief Writes the generated MFD pages from one semantic input sample.
 * @ingroup lhld_integration
 *
 * The controller owns only generated dynamic-reticle handles. Each population
 * call receives a complete semantic sample, projects it through
 * @ref BuildMfdFrame, and writes every generated handle required by the Radar,
 * SMS, HSD and A-G pages. It never owns or mutates simulation state.
 */
class MfdController
{
public:
    /**
     * @brief Resets the lazily allocated capturable FCR track handles.
     * @param ui Generated UI wrapper whose current authored state is initialized.
     * @note Call after every `LhldUi::Initialize()` because reset invalidates
     * previously created generated dynamic-reticle handles. Reticles are then
     * allocated only when their presentation mode needs them.
     */
    void Initialize(lhld_ui::LhldUi& ui);

    /**
     * @brief Applies one complete semantic sample to the generated MFD UI tree.
     * @param ui Generated UI wrapper for `examples/lhld/assets`.
     * @param input Semantic ownship, radar, stores and navigation sample.
     * @pre `ui.Initialize()` and the generated startup path ran before realtime calls.
     */
    void Populate(lhld_ui::LhldUi& ui, const MfdInputSample& input);

    /**
     * @brief Returns the track slot captured by the runtime acquisition strobe.
     * @return Track index in [0, kMaxRadarTracks), or -1 when nothing is captured.
     * @note Uses authoritative runtime feedback; no geometric nearest-track
     * approximation is performed in the simulation or application.
     */
    int CapturedTrackIndex() const noexcept;

    /**
     * @brief Returns the on-glass legend for one option-select button.
     * @param page Page owning the legend row.
     * @param osbOneBased Button number in [1, kOsbCount].
     * @param input Semantic sample used for data-driven legends (range, bars...).
     * @return Legend text, or an empty string for an unused button.
     *
     * @note Shared with the panel so the drawn legend and the button action stay
     * consistent.
     */
    static std::string OsbLegend(MfdPage page, int osbOneBased, const MfdInputSample& input);

private:
    std::array<lhld_ui::RadarRwsTrackDynamicReticle*, kMaxRadarTracks> rwsTracks_ {};
    std::array<lhld_ui::RadarTwsTrackDynamicReticle*, kMaxRadarTracks> twsTracks_ {};
    std::array<lhld_ui::RadarBuggedTrackDynamicReticle*, kMaxRadarTracks> buggedTracks_ {};
    std::array<lhld_ui::RadarSttTrackDynamicReticle*, kMaxRadarTracks> sttTracks_ {};
};
} // namespace lhld
