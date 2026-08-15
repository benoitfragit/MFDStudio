/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Reusable Dear ImGui primitives for the LHLD cockpit shell.
 */

#include <imgui.h>

namespace lhld::cockpit
{
/** @brief Visual emphasis applied to a cockpit action button. */
enum class ActionTone
{
    Normal,
    Caution
};

/** @brief Responsive arrangement selected for the MFD and avionics console. */
struct ConsoleLayout
{
    bool sideBySide = false;
    float mfdWidth = 0.0f;
    float controlWidth = 0.0f;
    float gap = 0.0f;
};

/**
 * @brief Selects a side-by-side or stacked cockpit arrangement.
 * @param available Remaining ImGui content size below the annunciator rail.
 * @return Sizes used by the LHLD application for its two cockpit bays.
 */
ConsoleLayout ResolveConsoleLayout(const ImVec2& available) noexcept;

/** @brief Pushes the matte cockpit palette and compact instrument spacing. */
void PushTheme();

/** @brief Restores the ImGui style stack pushed by PushTheme(). */
void PopTheme();

/** @brief Paints the current window with a subtly graded instrument-panel backplate. */
void DrawBackplate();

/**
 * @brief Draws a raised avionics panel header in the current child window.
 * @param title Primary engraved label.
 * @param subtitle Optional right-aligned equipment label.
 */
void DrawPanelHeader(const char* title, const char* subtitle = nullptr);

/**
 * @brief Draws a separator and engraved label for one functional group.
 * @param title Functional group name.
 * @param subtitle Optional equipment or state annotation.
 */
void DrawSectionHeader(const char* title, const char* subtitle = nullptr);

/**
 * @brief Draws a small backlit status annunciator.
 * @param label Text rendered beside the lamp.
 * @param active Whether the lamp is illuminated.
 * @param caution Uses amber instead of green when illuminated.
 */
void DrawStatusLamp(const char* label, bool active, bool caution = false);

/**
 * @brief Draws a tactile mode selector with an illuminated active state.
 * @param label Button caption.
 * @param active Whether the mode is selected.
 * @param size Requested ImGui button size.
 * @param tooltip Optional operator hint.
 * @return true on the frame in which the button is pressed.
 */
bool ModeButton(const char* label, bool active, ImVec2 size, const char* tooltip = nullptr);

/**
 * @brief Draws a tactile cockpit action button.
 * @param label Button caption.
 * @param size Requested ImGui button size.
 * @param tooltip Optional operator hint.
 * @param tone Normal or caution emphasis.
 * @return true on the frame in which the button is pressed.
 */
bool ActionButton(const char* label,
                  ImVec2 size,
                  const char* tooltip,
                  ActionTone tone = ActionTone::Normal);

/**
 * @brief Draws a vertical two-position guarded-style toggle switch.
 * @param id Stable ImGui identifier.
 * @param label Engraved switch label.
 * @param on Current mechanical position.
 * @param enabled Whether operator input is accepted.
 * @param tooltip Optional operator hint.
 * @return true when the operator requests the opposite position.
 * @note The caller remains responsible for applying the semantic state change.
 */
bool TwoPositionSwitch(const char* id,
                       const char* label,
                       bool on,
                       bool enabled,
                       const char* tooltip = nullptr);

/**
 * @brief Draws a mouse-driven vertical avionics thumbwheel.
 * @param id Stable ImGui identifier.
 * @param label Engraved control label.
 * @param value Value updated by vertical drag or the mouse wheel.
 * @param minimum Inclusive lower limit.
 * @param maximum Inclusive upper limit.
 * @param unitsPerPixel Sensitivity used for vertical mouse drag.
 * @param format printf-style readout format for the current value.
 * @param tooltip Optional operator hint.
 * @return true when operator input changed value.
 */
bool VerticalThumbwheel(const char* id,
                        const char* label,
                        float& value,
                        float minimum,
                        float maximum,
                        float unitsPerPixel,
                        const char* format,
                        const char* tooltip = nullptr);

/**
 * @brief Draws a two-axis cockpit slew controller.
 * @param id Stable ImGui identifier.
 * @param label Engraved control label.
 * @param x Normalized horizontal axis in [-1, 1].
 * @param y Normalized vertical axis in [-1, 1].
 * @param tooltip Optional operator hint.
 * @return true when operator input changed either axis.
 * @note Double-clicking the control recenters both axes.
 */
bool SlewControl(const char* id,
                 const char* label,
                 float& x,
                 float& y,
                 const char* tooltip = nullptr);
} // namespace lhld::cockpit
