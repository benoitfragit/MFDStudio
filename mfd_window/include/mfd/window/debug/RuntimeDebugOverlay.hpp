/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Integrated runtime debug overlay used by `mfd_window`.
 */

#include <string>
#include <string_view>
#include <vector>

#include "mfd/control/CommandTypes.h"
#include "mfd/control/UdpRuntimeBridge.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/runtime/SceneRegistry.h"

#include "mfd/window/debug/RuntimeDebugPreview.hpp"
#include "mfd/window/debug/RuntimeDebugState.hpp"

namespace mfd::window::debug
{
namespace internal
{
struct RuntimeDebugOverlayInternalAccess;
}

/**
 * @brief Facade orchestrating the runtime debug state, preview scene and ImGui overlay.
 */
class RuntimeDebugOverlay
{
public:
    /**
     * @brief Initializes the ImGui integration used by the overlay.
     */
    void Initialize();

    /**
     * @brief Shuts the overlay down and releases the ImGui integration.
     */
    void Shutdown() noexcept;

    /**
     * @brief Handles the global `F1` shortcut toggling the runtime debug mode.
     * @param liveScene Current live runtime scene.
     * @return `true` when the shortcut was consumed.
     */
    bool HandleShortcut([[maybe_unused]] const SceneRegistry& liveScene);

    /**
     * @brief Resets the overlay state after the window JSON has been reloaded.
     * @param liveScene Newly loaded live runtime scene.
     */
    void OnRuntimeReloaded(const SceneRegistry& liveScene);

    /**
     * @brief Synchronizes transport telemetry and preview batches with the live runtime.
     * @param liveScene Current live runtime scene.
     * @param bridge Active UDP bridge, or `nullptr` when none exists.
     * @param commandStatus Last command-side status.
     * @param feedbackStatus Last feedback-side status.
     * @param drainedBatches Command batches already accepted by the live runtime.
     */
    void Synchronize(const SceneRegistry& liveScene,
                     const UdpRuntimeBridge* bridge,
                     std::string_view commandStatus,
                     std::string_view feedbackStatus,
                     const std::vector<CommandBatch>& drainedBatches);

    /**
     * @brief Returns whether the runtime debug mode is currently active.
     * @return `true` when the preview scene owns the rendering output.
     */
    [[nodiscard]] bool Active() const noexcept;

    /**
     * @brief Returns whether the launcher should bypass the startup splash.
     * @return `true` while the debug overlay is active.
     */
    [[nodiscard]] bool SuppressStartupSplash() const noexcept;

    /**
     * @brief Returns whether standard runtime hotkeys should be ignored.
     * @return `true` while the debug overlay is active.
     */
    [[nodiscard]] bool ConsumeRuntimeShortcuts() const noexcept;

    /**
     * @brief Returns the preferred width of the dedicated debug side panel.
     * @return Side-panel width in pixels.
     */
    [[nodiscard]] int PreferredPanelWidth() const noexcept;

    /**
     * @brief Returns the scene that should currently be rendered.
     * @param liveScene Live runtime scene used when no preview is active.
     * @return Scene to render for the current frame.
     */
    [[nodiscard]] const SceneRegistry& RenderScene(const SceneRegistry& liveScene) const noexcept;

    /**
     * @brief Draws the runtime debug overlay on top of the already rendered scene.
     * @param liveScene Live runtime scene.
     * @param windowDefinition Window metadata shown by the overlay.
     * @param applicationName Human-readable application name printed by the launcher.
     * @param runtimeError Last runtime-side error surfaced by the host loop.
     */
    void Draw(const SceneRegistry& liveScene,
              const WindowAssetDefinition& windowDefinition,
              std::string_view applicationName,
              std::string_view runtimeError);

private:
    friend struct internal::RuntimeDebugOverlayInternalAccess;

    bool Activate();
    void Deactivate();
    bool RefreshPreviewFromLive(const SceneRegistry& liveScene);
    void RecordObservedRuntimeState(const SceneRegistry& liveScene, const std::vector<CommandBatch>& drainedBatches);
    void SelectFirstReticleOnActivePage(const SceneRegistry& displayScene, std::string* statusOut);
    [[nodiscard]] const ReticleGroup* FindSelectedReticle(const SceneRegistry& displayScene) const;
    template <typename Mutation, typename... Args>
    bool MutateSelectedReticleWithRollback(const SceneRegistry& liveScene,
                                           const SceneRegistry& displayScene,
                                           Mutation&& mutation,
                                           const char* successMessage,
                                           Args&&... args);
    void DrawManualTestPanel(const SceneRegistry& liveScene, const SceneRegistry& displayScene);
    [[nodiscard]] bool UsesPreviewScene() const noexcept;
    [[nodiscard]] UdpRuntimeBridgeMetrics CollectTransportMetrics(const UdpRuntimeBridge* bridge,
                                                                  std::size_t observedCommandCount);

    RuntimeDebugState state_ {};
    RuntimeDebugPreview preview_ {};
    bool initialized_ = false;
    bool transportMetricsRefreshInitialized_ = false;
    RuntimeDebugState::Clock::time_point lastTransportMetricsRefresh_ {};
};
} // namespace mfd::window::debug
