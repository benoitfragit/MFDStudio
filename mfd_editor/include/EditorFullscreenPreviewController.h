/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Session-only controller used to toggle the editor page preview into fullscreen mode.
 */

namespace editor
{
/**
 * @brief Snapshot of the editor workspace state affected by fullscreen preview mode.
 *
 * @details This state is editor-only and must never be serialized into authored
 * JSON assets. It only captures the pane visibility and sizing needed to
 * restore the previous GUI arrangement when fullscreen preview is exited.
 */
struct FullscreenPreviewLayoutState
{
    /** @brief Indicates whether the left sidebar is visible. */
    bool sidebarVisible = true;
    /** @brief Indicates whether the right inspector is visible. */
    bool inspectorVisible = true;
    /** @brief Indicates whether the page-context split is visible in reticle studio mode. */
    bool pageContextVisible = true;
    /** @brief Indicates whether the docked layer inspector is visible next to the preview. */
    bool layerInspectorVisible = false;
    /** @brief Indicates whether the preview minimap overlay is visible. */
    bool minimapVisible = false;
    /** @brief Indicates whether the docked problems panel is visible below the preview. */
    bool problemsVisible = false;
    /** @brief Persisted width of the left sidebar. */
    float sidebarWidth = 0.0f;
    /** @brief Persisted width of the right inspector. */
    float inspectorWidth = 0.0f;
    /** @brief Persisted width of the page-context pane inside the split workspace. */
    float pageContextWidth = 0.0f;
};

/**
 * @brief Transition returned by the fullscreen controller after one state change request.
 */
struct FullscreenPreviewTransition
{
    /** @brief Indicates whether the request changed the fullscreen mode. */
    bool changed = false;
    /** @brief Target layout state that should be applied by the caller. */
    FullscreenPreviewLayoutState state {};
};

/**
 * @brief Pure controller toggling the editor workspace between normal and fullscreen preview states.
 */
class FullscreenPreviewController
{
public:
    /**
     * @brief Enters fullscreen preview and returns the editor-only layout to apply.
     * @param currentState Current layout state before entering fullscreen.
     * @return Transition describing the fullscreen layout.
     */
    [[nodiscard]] FullscreenPreviewTransition Enter(const FullscreenPreviewLayoutState& currentState);

    /**
     * @brief Exits fullscreen preview and restores the previously captured layout.
     * @return Transition describing the layout to restore.
     */
    [[nodiscard]] FullscreenPreviewTransition Exit();

    /**
     * @brief Toggles fullscreen preview on or off.
     * @param currentState Current layout state before toggling on.
     * @return Transition describing the layout to apply.
     */
    [[nodiscard]] FullscreenPreviewTransition Toggle(const FullscreenPreviewLayoutState& currentState);

    /**
     * @brief Returns whether fullscreen preview is currently active.
     * @return `true` when fullscreen preview is enabled.
     */
    [[nodiscard]] bool IsActive() const noexcept;

private:
    bool active_ = false;
    FullscreenPreviewLayoutState previousState_ {};
};
} // namespace editor
