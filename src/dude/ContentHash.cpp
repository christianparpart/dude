// SPDX-License-Identifier: Apache-2.0

#include <dude/ContentHash.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>
#include <string_view>

namespace dude
{

namespace
{

// SHA-256 constants: first 32 bits of the fractional parts of the cube roots of the first 64 primes.
constexpr std::array<uint32_t, 64> K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

constexpr auto Rotr(uint32_t x, unsigned n) -> uint32_t
{
    return (x >> n) | (x << (32 - n));
}

constexpr auto Ch(uint32_t x, uint32_t y, uint32_t z) -> uint32_t
{
    return (x & y) ^ (~x & z);
}

constexpr auto Maj(uint32_t x, uint32_t y, uint32_t z) -> uint32_t
{
    return (x & y) ^ (x & z) ^ (y & z);
}

constexpr auto Sigma0(uint32_t x) -> uint32_t
{
    return Rotr(x, 2) ^ Rotr(x, 13) ^ Rotr(x, 22);
}

constexpr auto Sigma1(uint32_t x) -> uint32_t
{
    return Rotr(x, 6) ^ Rotr(x, 11) ^ Rotr(x, 25);
}

constexpr auto SmallSigma0(uint32_t x) -> uint32_t
{
    return Rotr(x, 7) ^ Rotr(x, 18) ^ (x >> 3);
}

constexpr auto SmallSigma1(uint32_t x) -> uint32_t
{
    return Rotr(x, 17) ^ Rotr(x, 19) ^ (x >> 10);
}

/// @brief SHA-256 state for incremental hashing.
struct Sha256State
{
    std::array<uint32_t, 8> hash = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
    uint64_t totalBits = 0;

    /// @brief Processes a single 64-byte (512-bit) block.
    void ProcessBlock(uint8_t const* block)
    {
        std::array<uint32_t, 64> w{};
        for (size_t i = 0; i < 16; ++i)
        {
            w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) | (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8) | static_cast<uint32_t>(block[i * 4 + 3]);
        }
        for (size_t i = 16; i < 64; ++i)
            w[i] = SmallSigma1(w[i - 2]) + w[i - 7] + SmallSigma0(w[i - 15]) + w[i - 16];

        auto a = hash[0];
        auto b = hash[1];
        auto c = hash[2];
        auto d = hash[3];
        auto e = hash[4];
        auto f = hash[5];
        auto g = hash[6];
        auto h = hash[7];

        for (size_t i = 0; i < 64; ++i)
        {
            auto const t1 = h + Sigma1(e) + Ch(e, f, g) + K[i] + w[i];
            auto const t2 = Sigma0(a) + Maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    /// @brief Returns the final SHA-256 digest as a 64-character lowercase hex string.
    [[nodiscard]] auto Finalize() const -> std::string
    {
        std::string result;
        result.reserve(64);
        for (auto const word : hash)
            result += std::format("{:08x}", word);
        return result;
    }
};

} // namespace

auto ComputeContentHash(std::string_view data) -> std::string
{
    Sha256State state;
    state.totalBits = data.size() * 8;

    auto const* ptr = reinterpret_cast<uint8_t const*>(data.data());
    auto remaining = data.size();

    // Process complete 64-byte blocks.
    while (remaining >= 64)
    {
        state.ProcessBlock(ptr);
        ptr += 64;
        remaining -= 64;
    }

    // Pad the final block(s).
    std::array<uint8_t, 128> padded{};
    std::memcpy(padded.data(), ptr, remaining);
    padded[remaining] = 0x80;

    // If remaining + 1 + 8 > 64, we need two blocks for padding.
    auto const padBlockStart = (remaining + 1 + 8 > 64) ? size_t{64} : size_t{0};
    auto const lengthOffset = padBlockStart + 56;

    // Write the total bit length as big-endian 64-bit value.
    auto const bits = state.totalBits;
    for (size_t i = 0; i < 8; ++i)
        padded[lengthOffset + i] = static_cast<uint8_t>(bits >> (56 - i * 8));

    state.ProcessBlock(padded.data());
    if (padBlockStart > 0)
        state.ProcessBlock(padded.data() + 64);

    return state.Finalize();
}

} // namespace dude
