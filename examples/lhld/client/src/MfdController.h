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

#include <string>

#include "MfdTypes.h"

namespace lhld_ui
{
class LhldUi;
}

namespace lhld
{
/**
 * @brief Writes the generated MFD pages from one semantic input sample.
 * @ingroup lhld_integration
 *
 * The controller is stateless: each call receives a complete semantic sample,
 * projects it through @ref BuildMfdFrame, and writes every generated handle the
 * current frame needs across the Radar, SMS, HSD and A-G pages. It owns text
 * formatting and per-page visibility policy; it never mutates simulation state.
 */
class MfdController
{
public:
    /**
     * @brief Applies one complete semantic sample to the generated MFD UI tree.
     * @param ui Generated UI wrapper for `examples/lhld/assets`.
     * @param input Semantic ownship, radar, stores and navigation sample.
     * @pre `ui.Initialize()` and the generated startup path ran before realtime calls.
     */
    void Populate(lhld_ui::LhldUi& ui, const MfdInputSample& input) const;

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
};
} // namespace lhld
