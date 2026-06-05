/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal validation helpers for authored `strftime` time formats.
 */

#include <string_view>

namespace mfd::detail
{
/**
 * @brief Returns whether one conversion character stays within the supported `strftime` subset.
 * @param conversion Candidate conversion character immediately following one `%`.
 * @return `true` when the conversion stays supported by the runtime formatter.
 */
inline bool IsSupportedTimeFormatConversion(const char conversion) noexcept
{
    switch (conversion)
    {
    case 'a':
    case 'A':
    case 'b':
    case 'B':
    case 'c':
    case 'C':
    case 'd':
    case 'D':
    case 'e':
    case 'F':
    case 'g':
    case 'G':
    case 'h':
    case 'H':
    case 'I':
    case 'j':
    case 'm':
    case 'M':
    case 'n':
    case 'p':
    case 'r':
    case 'R':
    case 'S':
    case 't':
    case 'T':
    case 'u':
    case 'U':
    case 'V':
    case 'w':
    case 'W':
    case 'x':
    case 'X':
    case 'y':
    case 'Y':
    case 'z':
    case 'Z':
        return true;
    default:
        return false;
    }
}

/**
 * @brief Returns whether one authored time format is safe to pass to `std::strftime`.
 * @param format Candidate authored format string.
 * @return `true` when the format is non-empty and every `%` directive is complete and supported.
 * @note The Windows CRT can abort on malformed directives such as a trailing `%`, so callers must validate first.
 */
inline bool IsValidTimeFormatString(const std::string_view format) noexcept
{
    if (format.empty())
    {
        return false;
    }

    for (std::size_t index = 0; index < format.size(); ++index)
    {
        if (format[index] != '%')
        {
            continue;
        }

        ++index;
        if (index >= format.size())
        {
            return false;
        }

        const char directive = format[index];
        if (directive == '%')
        {
            continue;
        }

#if defined(_WIN32)
        if (directive == '#')
        {
            ++index;
            if (index >= format.size())
            {
                return false;
            }

            if (!IsSupportedTimeFormatConversion(format[index]))
            {
                return false;
            }

            continue;
        }
#endif

        if (!IsSupportedTimeFormatConversion(directive))
        {
            return false;
        }
    }

    return true;
}
} // namespace mfd::detail
