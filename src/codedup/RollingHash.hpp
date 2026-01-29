// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/TokenNormalizer.hpp>

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

namespace codedup
{

/// @brief Rabin-Karp hash base for rolling hash computation.
constexpr uint64_t hashBase = 257;

/// @brief Mersenne prime 2^61 - 1, used as modulus for rolling hash.
constexpr uint64_t hashPrime = (1ULL << 61) - 1;

/// @brief Modular multiplication optimized for Mersenne prime 2^61 - 1.
///
/// Exploits the identity: for p = 2^61 - 1 (a Mersenne prime),
///   a * b mod p = lo + hi, where:
///     product = a * b              (128-bit)
///     lo      = product & p        (lower 61 bits)
///     hi      = product >> 61      (upper bits)
///
/// Since p = 2^61 - 1, we have 2^61 ≡ 1 (mod p), so the upper bits contribute their
/// face value. The sum lo + hi is at most 2*p, requiring at most one conditional subtraction.
/// This replaces the general-purpose 128-bit modulo (potentially a library call on some
/// platforms) with a shift, mask, and add -- significantly faster in tight loops.
///
/// @param a First operand (must be < hashPrime).
/// @param b Second operand (must be < hashPrime).
/// @return (a * b) % hashPrime.
[[nodiscard]] constexpr auto mulmod(uint64_t a, uint64_t b) -> uint64_t
{
    auto const product = static_cast<__int128>(a) * static_cast<__int128>(b);
    auto const lo = static_cast<uint64_t>(product) & hashPrime;
    auto const hi = static_cast<uint64_t>(static_cast<unsigned __int128>(product) >> 61);
    auto const result = lo + hi;
    return result >= hashPrime ? result - hashPrime : result;
}

/// @brief Computes rolling hash fingerprints over a sliding window of normalized token IDs.
/// @param ids The normalized token ID sequence.
/// @param windowSize The size of the sliding window.
/// @return A vector of fingerprints, one per window position.
[[nodiscard]] inline auto computeRollingFingerprints(std::span<NormalizedTokenId const> ids, size_t windowSize)
    -> std::vector<uint64_t>
{
    if (ids.size() < windowSize)
        return {};

    std::vector<uint64_t> fingerprints;
    fingerprints.reserve(ids.size() - windowSize + 1);

    // Compute BASE^(W-1) mod PRIME for removing the leading term
    uint64_t basePow = 1;
    for ([[maybe_unused]] auto const _ : std::views::iota(size_t{0}, windowSize - 1))
        basePow = mulmod(basePow, hashBase);

    // Initial hash for the first window
    uint64_t hash = 0;
    for (auto const i : std::views::iota(size_t{0}, windowSize))
        hash = (mulmod(hash, hashBase) + ids[i]) % hashPrime;

    fingerprints.push_back(hash);

    // Rolling hash for subsequent windows
    for (size_t i = windowSize; i < ids.size(); ++i)
    {
        auto const remove = mulmod(ids[i - windowSize], basePow);
        hash = (hash + hashPrime - remove) % hashPrime;
        hash = (mulmod(hash, hashBase) + ids[i]) % hashPrime;
        fingerprints.push_back(hash);
    }

    return fingerprints;
}

} // namespace codedup
