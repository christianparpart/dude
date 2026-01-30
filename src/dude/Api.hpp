// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @brief Export/import macro for the dude library.
///
/// Currently builds as a static library, so this is a no-op.
/// Defined for future shared library support.
#if defined(_WIN32) && defined(DUDE_SHARED)
#if defined(DUDE_EXPORTS)
#define DUDE_API __declspec(dllexport)
#else
#define DUDE_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define DUDE_API __attribute__((visibility("default")))
#else
#define DUDE_API
#endif
