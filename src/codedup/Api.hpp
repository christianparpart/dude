// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @brief Export/import macro for the codedup library.
///
/// Currently builds as a static library, so this is a no-op.
/// Defined for future shared library support.
#if defined(_WIN32) && defined(CODEDUP_SHARED)
#if defined(CODEDUP_EXPORTS)
#define CODEDUP_API __declspec(dllexport)
#else
#define CODEDUP_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define CODEDUP_API __attribute__((visibility("default")))
#else
#define CODEDUP_API
#endif
