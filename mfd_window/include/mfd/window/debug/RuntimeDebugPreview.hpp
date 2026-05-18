/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal preview scene used by the runtime debug overlay.
 */

#include <memory>
#include <string>
#include <vector>

#include "mfd/control/CommandProcessor.h"
#include "mfd/runtime/SceneRegistry.h"

#include "mfd/window/debug/RuntimeDebugState.hpp"

namespace mfd::window::debug
{
/**
 * @brief Preview scene mirroring the live runtime while applying local debug bypasses.
 *
 * @note The live `SceneRegistry` remains the source of truth for UDP-driven
 * state. This preview scene is rebuilt from the live runtime when the debug
 * mode starts and then receives the same command batches, except for the
 * reticles currently owned by local bypass drafts.
 */
class RuntimeDebugPreview
{
public:
    /**
     * @brief Rebuilds the preview scene from the current live runtime state.
     * @param liveScene Live runtime scene driven by UDP.
     * @param state Current debug state including page, reticle and template overrides.
     * @return `true` on success, otherwise `false`.
     */
    bool ResetFromLive(const SceneRegistry& liveScene, const RuntimeDebugState& state);

    /**
     * @brief Applies the latest live command batches to the preview scene.
     * @param batches Command batches already accepted by the live runtime.
     * @param state Current debug state used to filter locally bypassed reticles.
     * @return `true` on success, otherwise `false`.
     */
    bool ApplyLiveBatches(const std::vector<CommandBatch>& batches, const RuntimeDebugState& state);

    /**
     * @brief Re-applies every local bypass to the preview scene.
     * @param state Current debug state.
     * @return `true` on success, otherwise `false`.
     */
    bool ApplyStateOverrides(const RuntimeDebugState& state);

    /**
     * @brief Invalidates the preview scene.
     */
    void Invalidate() noexcept;

    /**
     * @brief Returns whether the preview scene is currently valid.
     * @return `true` when the preview can be rendered safely.
     */
    [[nodiscard]] bool Ready() const noexcept;

    /**
     * @brief Returns the last preview-side error.
     * @return Human-readable error string, or an empty string when the preview is valid.
     */
    [[nodiscard]] const std::string& LastError() const noexcept;

    /**
     * @brief Returns the current preview scene.
     * @return Const preview scene reference.
     */
    [[nodiscard]] const SceneRegistry& Scene() const noexcept;

private:
    bool ApplyReticleDraft(const ReticleBypassState& bypass);
    bool ApplyDynamicTemplateVisibility(const RuntimeDebugState& state);

    SceneRegistry scene_ {};
    std::unique_ptr<CommandProcessor> processor_ {};
    std::string lastError_ {};
    bool ready_ = false;
};
} // namespace mfd::window::debug
