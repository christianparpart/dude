// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <functional>

namespace codedup
{

/// @brief Callback type for reporting progress from detection stages.
///
/// @param current The current item index (1-based).
/// @param total The total number of items to process.
using ProgressCallback = std::function<void(size_t current, size_t total)>;

} // namespace codedup
