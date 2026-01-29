// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace codedup
{

/// @brief Predicate that decides whether a given file path should be included in scan results.
using FileFilter = std::function<bool(std::filesystem::path const&)>;

/// @brief Error information from file scanning.
struct FileScanError
{
    std::string message; ///< Description of the error.
};

/// @brief Recursively scans directories for C++ source files.
///
/// Filters files by extension (case-insensitive) and returns them in sorted order.
class CODEDUP_API FileScanner
{
public:
    /// @brief Scans a directory recursively for C++ source files.
    /// @param directory The root directory to scan.
    /// @param extensions List of file extensions to include (e.g., ".cpp", ".hpp").
    /// @param filter Optional predicate to further filter files. If provided, only files for which
    ///               the predicate returns true are included.
    /// @return A sorted vector of matching file paths, or a FileScanError.
    [[nodiscard]] static auto Scan(std::filesystem::path const& directory,
                                   std::vector<std::string> const& extensions = DefaultExtensions(),
                                   std::optional<FileFilter> const& filter = std::nullopt)
        -> std::expected<std::vector<std::filesystem::path>, FileScanError>;

    /// @brief Returns the default set of C++ file extensions.
    [[nodiscard]] static auto DefaultExtensions() -> std::vector<std::string> const&;
};

} // namespace codedup
