#pragma once

/**
 * @file
 * @brief Internal SHA-256 helper used to verify generated transport maps.
 */

#include <string>
#include <string_view>

namespace mfd::detail
{
/**
 * @brief Computes the lowercase hexadecimal SHA-256 digest of a byte string.
 * @param input Bytes to hash.
 * @return Sixty-four lowercase hexadecimal characters.
 */
std::string ComputeSha256Hex(std::string_view input);
} // namespace mfd::detail
