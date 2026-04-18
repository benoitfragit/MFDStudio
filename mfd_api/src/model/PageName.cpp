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

std::string NormalizeIdentifier(const std::string_view value)
{
    return NormalizePageName(value);
}

bool PageNamesEqual(const std::string_view lhs, const std::string_view rhs)
{
    return NormalizePageName(lhs) == NormalizePageName(rhs);
}
} // namespace mfd
