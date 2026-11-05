/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal validation helpers for the widget-editor DLL boundary.
 */

#include "mfd/apilevel1/widgets/WidgetEditorAbi.h"

#include <imgui.h>
#include <string_view>

namespace mfd::api1::internal
{
struct TextFieldBounds
{
    ImVec2 min {};
    ImVec2 max {};
};

/** @brief Draw the host text field and report a changed value only when the user commits. */
bool DrawWidgetTextField(TextFieldBounds& bounds, const char* label, char* value, std::size_t capacity);

/** @brief Identify the owner of a changed text field in a validated DLL draft. */
inline uint32_t WidgetTextAction(const MfdWidgetEditorDraftV1& draft,
                                      std::string_view instanceId, std::string_view parent, std::string_view text,
                                      std::string_view definitionId, std::string_view defaultText) noexcept
{
    if (!draft.definition_only && (instanceId != draft.id || parent != draft.parent || text != draft.text)) return 1U;
    if (definitionId != draft.definition_id || defaultText != draft.default_text) return 2U;
    return 0U;
}

/**
 * @brief Validates every host-owned invariant after an external editor draw call.
 * @param draft Draft potentially mutated by the editor DLL.
 * @return `true` only when the structure size is unchanged and every fixed buffer is null-terminated.
 */
[[nodiscard]] bool IsWidgetEditorDraftValidAfterDraw(const MfdWidgetEditorDraftV1& draft) noexcept;
} // namespace mfd::api1::internal
