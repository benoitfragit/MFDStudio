/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Adapter from semantic Demo HUD inputs to the generated HUD UI.
 */

#include "DemoHudProjection.h"

namespace demo_hud_ui
{
class DemoHudUi;
}

namespace demo_hud
{
/**
 * @brief Updates the generated HUD asset from one simulation sample.
 * @ingroup demo_hud_integration
 *
 * The controller is intentionally stateless: each call receives a complete
 * semantic input sample, projects it into HUD-space values, and writes every
 * generated handle needed by the current frame.
 */
class DemoHudController
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
     * `DemoHudUi::SubmitLatest()`.
     *
     * This is the integration entry point for external aircraft code. The demo
     * mini-simulation is only one possible producer of this semantic sample.
     *
     * @note Do not pass raw panel/HOTAS commands here. Convert them first into
     * `HudInputSample::weapon` fields such as `masterMode`, `selectedMissile`,
     * inventory and missile timing.
     */
    void Populate(demo_hud_ui::DemoHudUi& ui, const HudInputSample& input) const;

};
} // namespace demo_hud
