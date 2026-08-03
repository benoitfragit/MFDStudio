/**
 * @file
 * @brief Allocation-bounded SHA-256 implementation for transport-map integrity checks.
 */

#include "Sha256.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mfd::detail
{
namespace
{
constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

constexpr std::array<std::uint32_t, 8> kInitialState = {
    0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
    0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

std::uint32_t RotateRight(const std::uint32_t value, const std::uint32_t count) noexcept
{
    return (value >> count) | (value << (32U - count));
}

void TransformBlock(std::array<std::uint32_t, 8>& state, const std::uint8_t* block) noexcept
{
    std::array<std::uint32_t, 64> words {};
    for (std::size_t index = 0U; index < 16U; ++index)
    {
        const std::size_t offset = index * 4U;
        words[index] = (static_cast<std::uint32_t>(block[offset]) << 24U) |
                       (static_cast<std::uint32_t>(block[offset + 1U]) << 16U) |
                       (static_cast<std::uint32_t>(block[offset + 2U]) << 8U) |
                       static_cast<std::uint32_t>(block[offset + 3U]);
    }
    for (std::size_t index = 16U; index < words.size(); ++index)
    {
        const std::uint32_t previous = words[index - 15U];
        const std::uint32_t sigma0 = RotateRight(previous, 7U) ^ RotateRight(previous, 18U) ^ (previous >> 3U);
        const std::uint32_t recent = words[index - 2U];
        const std::uint32_t sigma1 = RotateRight(recent, 17U) ^ RotateRight(recent, 19U) ^ (recent >> 10U);
        words[index] = words[index - 16U] + sigma0 + words[index - 7U] + sigma1;
    }

    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];
    std::uint32_t f = state[5];
    std::uint32_t g = state[6];
    std::uint32_t h = state[7];

    for (std::size_t index = 0U; index < words.size(); ++index)
    {
        const std::uint32_t sum1 = RotateRight(e, 6U) ^ RotateRight(e, 11U) ^ RotateRight(e, 25U);
        const std::uint32_t choice = (e & f) ^ ((~e) & g);
        const std::uint32_t temporary1 = h + sum1 + choice + kRoundConstants[index] + words[index];
        const std::uint32_t sum0 = RotateRight(a, 2U) ^ RotateRight(a, 13U) ^ RotateRight(a, 22U);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temporary2 = sum0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}
} // namespace

std::string ComputeSha256Hex(const std::string_view input)
{
    std::vector<std::uint8_t> padded;
    const std::size_t paddingBytes = 1U + ((55U - (input.size() % 64U) + 64U) % 64U) + 8U;
    padded.reserve(input.size() + paddingBytes);
    for (const char byte : input)
    {
        padded.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    }
    padded.push_back(0x80U);
    while ((padded.size() % 64U) != 56U)
    {
        padded.push_back(0U);
    }

    const std::uint64_t bitLength = static_cast<std::uint64_t>(input.size()) * 8U;
    for (std::size_t byteIndex = 0U; byteIndex < 8U; ++byteIndex)
    {
        const std::size_t shift = (7U - byteIndex) * 8U;
        padded.push_back(static_cast<std::uint8_t>((bitLength >> shift) & 0xFFU));
    }

    std::array<std::uint32_t, 8> state = kInitialState;
    for (std::size_t offset = 0U; offset < padded.size(); offset += 64U)
    {
        TransformBlock(state, padded.data() + offset);
    }

    constexpr char kHexDigits[] = "0123456789abcdef";
    std::string digest;
    digest.reserve(64U);
    for (const std::uint32_t word : state)
    {
        for (std::size_t nibbleIndex = 0U; nibbleIndex < 8U; ++nibbleIndex)
        {
            const std::size_t shift = (7U - nibbleIndex) * 4U;
            digest.push_back(kHexDigits[(word >> shift) & 0x0FU]);
        }
    }
    return digest;
}
} // namespace mfd::detail
