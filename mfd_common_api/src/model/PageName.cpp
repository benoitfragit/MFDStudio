/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for PageName.
 */

#include "mfd/model/PageName.h"

#include <cctype>
#include <cstddef>

namespace mfd
{
namespace
{
std::string_view TrimAsciiWhitespace(std::string_view value) noexcept
{
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
    {
        --last;
    }

    return value.substr(first, last - first);
}
} // namespace

std::string NormalizePageName(const std::string_view value)
{
    const std::string_view trimmed = TrimAsciiWhitespace(value);

    std::string normalized;
    normalized.reserve(trimmed.size());

    for (const char character : trimmed)
    {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }

    return normalized;
}

bool PageNamesEqual(const std::string_view lhs, const std::string_view rhs)
{
    const std::string_view left = TrimAsciiWhitespace(lhs);
    const std::string_view right = TrimAsciiWhitespace(rhs);
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        const int leftLowered = std::tolower(static_cast<unsigned char>(left[index]));
        const int rightLowered = std::tolower(static_cast<unsigned char>(right[index]));
        if (leftLowered != rightLowered)
        {
            return false;
        }
    }

    return true;
}

bool PageNameIsBlank(const std::string_view value) noexcept
{
    return TrimAsciiWhitespace(value).empty();
}
} // namespace mfd
