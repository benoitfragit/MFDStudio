/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Small bounded byte reader shared by the dedicated libFuzzer targets.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

namespace mfd::fuzz
{
/**
 * @brief Bounded reader turning raw fuzzer bytes into simple scalar values.
 */
class FuzzInput
{
public:
    FuzzInput(const std::uint8_t* data, const std::size_t size) noexcept
        : data_(data),
          size_(size)
    {
    }

    [[nodiscard]] bool Empty() const noexcept
    {
        return size_ == 0U;
    }

    [[nodiscard]] std::size_t Remaining() const noexcept
    {
        return size_ > offset_ ? size_ - offset_ : 0U;
    }

    std::uint8_t NextByte(const std::uint8_t fallback = 0U) noexcept
    {
        if (offset_ >= size_)
        {
            return fallback;
        }

        return data_[offset_++];
    }

    bool NextBool() noexcept
    {
        return (NextByte() & 1U) != 0U;
    }

    std::size_t NextIndex(const std::size_t upperBoundExclusive) noexcept
    {
        if (upperBoundExclusive == 0U)
        {
            return 0U;
        }

        return static_cast<std::size_t>(NextByte()) % upperBoundExclusive;
    }

    int NextInt(const int minimumInclusive, const int maximumInclusive) noexcept
    {
        if (maximumInclusive <= minimumInclusive)
        {
            return minimumInclusive;
        }

        const auto span = static_cast<std::uint32_t>(maximumInclusive - minimumInclusive + 1);
        return minimumInclusive + static_cast<int>(NextU32() % span);
    }

    float NextFloat(const float minimumInclusive, const float maximumInclusive) noexcept
    {
        if (!(maximumInclusive > minimumInclusive))
        {
            return minimumInclusive;
        }

        const float ratio =
            static_cast<float>(NextU32()) / static_cast<float>(std::numeric_limits<std::uint32_t>::max());
        return minimumInclusive + ((maximumInclusive - minimumInclusive) * ratio);
    }

    float NextRawFloat() noexcept
    {
        const std::uint32_t bits = NextU32();
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    std::string NextString(const std::size_t maximumLength)
    {
        static constexpr std::array<char, 71> kAlphabet {
            'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
            's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
            'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1',
            '2', '3', '4', '5', '6', '7', '8', '9', '_', '-', '.', ' ', '/', '\\', ':', '#', '%'};

        const std::size_t length = std::min<std::size_t>(maximumLength, NextIndex(maximumLength + 1U));
        std::string result;
        result.reserve(length);
        for (std::size_t index = 0; index < length; ++index)
        {
            result.push_back(kAlphabet[NextIndex(kAlphabet.size())]);
        }

        return result;
    }

private:
    std::uint32_t NextU32() noexcept
    {
        std::uint32_t value = 0U;
        for (std::size_t index = 0; index < sizeof(value); ++index)
        {
            value |= static_cast<std::uint32_t>(NextByte()) << (index * 8U);
        }

        return value;
    }

    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0U;
    std::size_t offset_ = 0U;
};
} // namespace mfd::fuzz
