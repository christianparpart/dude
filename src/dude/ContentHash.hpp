// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/Api.hpp>

#include <string>
#include <string_view>

namespace dude
{

/// @brief Computes a SHA-256 hex digest of the given data.
/// @param data The data to hash.
/// @return Lowercase hex string of the SHA-256 digest.
[[nodiscard]] DUDE_API auto ComputeContentHash(std::string_view data) -> std::string;

} // namespace dude
