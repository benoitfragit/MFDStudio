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

namespace mfd::api1::internal
{
/**
 * @brief Validates every host-owned invariant after an external editor draw call.
 * @param draft Draft potentially mutated by the editor DLL.
 * @return `true` only when the structure size is unchanged and every fixed buffer is null-terminated.
 */
[[nodiscard]] bool IsWidgetEditorDraftValidAfterDraw(const MfdWidgetEditorDraftV1& draft) noexcept;
} // namespace mfd::api1::internal
