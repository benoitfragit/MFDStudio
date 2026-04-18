/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Helpers used to normalize and compare page identifiers.
 */

#include <functional>
#include <string>
#include <string_view>

#include "mfd/MfdExport.h"

namespace mfd
{
/**
 * @brief Transparent string hash for heterogeneous lookups in unordered maps.
 *
 * @note This hash lets the runtime query page and reticle maps with
 * `std::string`, `std::string_view` or raw C strings without temporary
 * allocations.
 */
struct TransparentStringHash
{
    using is_transparent = void;

    std::size_t operator()(const std::string_view value) const noexcept
    {
        return std::hash<std::string_view> {}(value);
    }

    std::size_t operator()(const std::string& value) const noexcept
    {
        return (*this)(std::string_view(value));
    }

    std::size_t operator()(const char* value) const noexcept
    {
        return (*this)(std::string_view(value));
    }
};

/**
 * @brief Transparent string equality predicate matching normalized string keys.
 */
struct TransparentStringEqual
{
    using is_transparent = void;

    bool operator()(const std::string_view lhs, const std::string_view rhs) const noexcept
    {
        return lhs == rhs;
    }
};

/**
 * @brief Normalizes a page-like identifier for runtime lookup.
 * @param value Raw identifier provided by JSON or by user code.
 * @return Lower-cased normalized name with stable comparison semantics.
 */
MFD_API std::string NormalizePageName(std::string_view value);

/**
 * @brief Normalizes a generic identifier for runtime lookup.
 * @param value Raw identifier provided by JSON or by user code.
 * @return Lower-cased normalized name with stable comparison semantics.
 */
MFD_API std::string NormalizeIdentifier(std::string_view value);

/**
 * @brief Compares two page names after normalization.
 * @param lhs First page name.
 * @param rhs Second page name.
 * @return `true` if both names refer to the same normalized identifier.
 */
MFD_API bool PageNamesEqual(std::string_view lhs, std::string_view rhs);
} // namespace mfd
