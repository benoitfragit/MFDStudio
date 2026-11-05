/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include "TimeFormatValidation.h"
#include <string_view>

namespace editor::app
{
enum class StringEditDecision { Skip, RejectFormat, Commit };

/** @brief Decide whether a finished inspector edit warrants an undo snapshot. */
inline StringEditDecision CheckInspectorStringEdit(std::string_view current, std::string_view original,
                                                   std::string_view requested, bool timeFormat) noexcept
{
    if (current != original || current == requested) return StringEditDecision::Skip;
    if (timeFormat && !mfd::detail::IsValidTimeFormatString(requested)) return StringEditDecision::RejectFormat;
    return StringEditDecision::Commit;
}
}
