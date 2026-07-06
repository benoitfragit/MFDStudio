/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Adapter from semantic HUD inputs to the generated HUD UI.
 */

#include "HudProjection.h"

namespace hud_ui
{
class HudUi;
}

namespace hud
{
/**
 * @brief Updates the generated HUD asset from one simulation sample.
 * @ingroup hud_integration
 *
 * The controller is intentionally stateless: each call receives a complete
 * semantic input sample, projects it into HUD-space values, and writes every
 * generated handle needed by the current frame.
 */
class HudController
{
public:
    /**
     * @brief Applies one complete physical aircraft sample to the generated HUD UI tree.
     * @param ui Generated UI wrapper for `examples/hud/assets`.
     * @param input SI-unit aircraft, target and weapon sample.
     * @pre `ui.Initialize()` and the generated startup path have been executed
     * before realtime calls start.
     * @pre `input` contains resolved semantic state for one aircraft timestamp.
     * @post Generated HUD handles contain a complete frame ready for
     * `HudUi::SubmitLatest()`.
     *
     * This is the integration entry point for external aircraft code. The sample
     * mini-simulation is only one possible producer of this semantic sample.
     *
     * @note Do not pass raw panel/HOTAS commands here. Convert them first into
     * `HudInputSample::weapon` fields such as `masterMode`, `selectedMissile`,
     * inventory and missile timing.
     */
    void Populate(hud_ui::HudUi& ui, const HudInputSample& input) const;

};
} // namespace hud
