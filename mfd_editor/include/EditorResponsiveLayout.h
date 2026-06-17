/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Pure, testable layout helper that resolves the editor root shell into a
 *        responsive three-column arrangement (sidebar, workspace, inspector).
 *
 * @details The helper centralizes the progressive-degradation policy so the ImGui
 * shell only draws from a resolved result instead of recomputing layout rules. The
 * editable workspace always keeps priority; auxiliary panels compress toward their
 * minimum width and then auto-collapse before the resize is allowed to block. The
 * helper never mutates the user's persisted width or visibility preferences: those
 * are inputs, and transient auto-collapse is reported separately through
 * `ShellPanelLayout::autoCollapsed`.
 */

namespace editor
{
/**
 * @brief Discrete responsive arrangement of the editor root shell.
 */
enum class ShellLayoutMode
{
    /** @brief Sidebar, workspace and inspector are all visible. */
    Wide,
    /** @brief Workspace plus exactly one auxiliary panel. */
    Compact,
    /** @brief Workspace only; both auxiliary panels are hidden. */
    Focus
};

/**
 * @brief User preferences and technical constraints for one auxiliary side panel.
 *
 * @note `preferredWidth` is the width the user expects (drag result / persisted
 * value). It is treated as a preference, not a hard constraint: the helper may
 * render the panel narrower down to `minWidth`, or hide it, without changing it.
 */
struct ShellSidePanel
{
    /** @brief Whether the user wants this panel visible (session/persisted intent). */
    bool wantVisible = true;
    /** @brief Preferred panel width when shown, in pixels. */
    float preferredWidth = 0.0f;
    /** @brief Smallest width that still keeps the panel usable, in pixels. */
    float minWidth = 0.0f;
};

/**
 * @brief Input describing the available root width and both panel preferences.
 */
struct ShellLayoutRequest
{
    /** @brief Total horizontal space available to the shell, in pixels. */
    float totalWidth = 0.0f;
    /** @brief Width of one inter-panel splitter, in pixels. */
    float splitterWidth = 0.0f;
    /** @brief Minimum editable workspace width that must always survive, in pixels. */
    float minWorkspaceWidth = 0.0f;
    /** @brief Left navigation sidebar preferences (lower degradation priority). */
    ShellSidePanel sidebar {};
    /** @brief Right inspector preferences (higher degradation priority). */
    ShellSidePanel inspector {};
};

/**
 * @brief Resolved geometry and state of one auxiliary panel for the current frame.
 */
struct ShellPanelLayout
{
    /** @brief Effective visibility this frame. */
    bool visible = false;
    /** @brief `true` when the user wants the panel but it was hidden for lack of space. */
    bool autoCollapsed = false;
    /** @brief Resolved panel width when visible, in pixels. */
    float width = 0.0f;
};

/**
 * @brief Resolved root layout consumed by the editor shell.
 */
struct ShellLayoutResult
{
    /** @brief Current responsive arrangement. */
    ShellLayoutMode mode = ShellLayoutMode::Wide;
    /** @brief Resolved sidebar state. */
    ShellPanelLayout sidebar {};
    /** @brief Resolved inspector state. */
    ShellPanelLayout inspector {};
    /** @brief Resolved width of the central editable workspace, in pixels. */
    float workspaceWidth = 0.0f;
};

/**
 * @brief Resolves the responsive editor shell layout from width and panel preferences.
 *
 * @param request Available width, splitter width, workspace floor and panel preferences.
 * @return Resolved mode, clamped panel widths, workspace width and auto-collapse flags.
 *
 * @note Inputs are sanitized: non-finite, negative or too-small values are clamped so
 * the result always stays consistent. The workspace keeps priority; the sidebar (lower
 * priority) compresses and collapses before the inspector.
 */
[[nodiscard]] ShellLayoutResult ComputeShellLayout(const ShellLayoutRequest& request) noexcept;
} // namespace editor
