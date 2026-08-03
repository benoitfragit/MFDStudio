#pragma once

/**
 * @file
 * @brief Allocation-conscious helpers for unambiguous internal composite string keys.
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace mfd::detail
{
inline constexpr std::size_t kCompositeKeyLengthBytes = sizeof(std::uint64_t);
inline constexpr std::size_t kCompositeKeyFieldOverhead = 1U + kCompositeKeyLengthBytes;

/**
 * @brief Appends one tagged length-prefixed byte string to an internal composite key.
 * @param key Destination key.
 * @param tag Domain-specific field tag.
 * @param value Field bytes; embedded separators and null bytes are preserved unambiguously.
 */
inline void AppendCompositeStringField(std::string& key, const char tag, const std::string_view value)
{
    key.push_back(tag);
    const std::uint64_t length = static_cast<std::uint64_t>(value.size());
    for (std::size_t byteIndex = 0U; byteIndex < kCompositeKeyLengthBytes; ++byteIndex)
    {
        const std::size_t shift = (kCompositeKeyLengthBytes - 1U - byteIndex) * 8U;
        key.push_back(static_cast<char>((length >> shift) & 0xFFU));
    }
    key.append(value.data(), value.size());
}

/**
 * @brief Appends one tagged fixed-width unsigned integer to an internal composite key.
 * @param key Destination key.
 * @param tag Domain-specific field tag distinct from textual identifiers.
 * @param value Unsigned value to encode in big-endian order.
 */
inline void AppendCompositeUnsignedField(std::string& key, const char tag, const std::uint64_t value)
{
    key.push_back(tag);
    for (std::size_t byteIndex = 0U; byteIndex < sizeof(value); ++byteIndex)
    {
        const std::size_t shift = (sizeof(value) - 1U - byteIndex) * 8U;
        key.push_back(static_cast<char>((value >> shift) & 0xFFU));
    }
}
} // namespace mfd::detail
