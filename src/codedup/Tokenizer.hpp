// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <codedup/Api.hpp>
#include <codedup/Encoding.hpp>
#include <codedup/Token.hpp>

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace codedup
{

/// @brief Error information from the tokenizer.
struct TokenizerError
{
    std::string message;     ///< Description of the error.
    SourceLocation location; ///< Location where the error occurred.
};

/// @brief Hand-written C++ lexer that produces a stream of tokens.
///
/// Recognizes all C++ keywords, operators, literals (including raw strings,
/// digit separators, hex/bin/oct), line and block comments, and preprocessor directives.
class CODEDUP_API Tokenizer
{
public:
    /// @brief Tokenizes C++ source code from a string.
    /// @param source The source code to tokenize.
    /// @param filePath The file path for source location tracking.
    /// @return A vector of tokens on success, or a TokenizerError on failure.
    [[nodiscard]] static auto Tokenize(std::string_view source, std::filesystem::path const& filePath = {})
        -> std::expected<std::vector<Token>, TokenizerError>;

    /// @brief Tokenizes a C++ source file.
    /// @param filePath Path to the source file.
    /// @param encoding The input encoding to use (default: auto-detect).
    /// @return A vector of tokens on success, or a TokenizerError on failure.
    [[nodiscard]] static auto TokenizeFile(std::filesystem::path const& filePath,
                                           InputEncoding encoding = InputEncoding::Auto)
        -> std::expected<std::vector<Token>, TokenizerError>;
};

} // namespace codedup
