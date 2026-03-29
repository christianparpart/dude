// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/Api.hpp>
#include <dude/CodeBlock.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace dude
{

/// @brief Error information from block cache operations.
struct BlockCacheError
{
    std::string message; ///< Description of the error.
};

/// @brief Caches per-file block extraction results to disk.
///
/// Cache entries are keyed by a composite of (content SHA-256 hash, language name,
/// minTokens, textSensitivity). This ensures that changing analysis parameters
/// correctly invalidates the cache. The cache is stored as a single JSON file.
///
/// Cached blocks store NoFileIndex in their sourceRange. The caller must patch
/// the fileIndex after loading from cache.
///
/// Entries older than maxAge are evicted on Load() to prevent unbounded growth.
class DUDE_API BlockCache
{
public:
    /// @brief Default maximum age for cache entries (4 weeks).
    static constexpr auto DefaultMaxAge = std::chrono::hours{24 * 28};

    /// @brief Constructs a BlockCache with the given cache file path.
    /// @param cachePath Path to the cache JSON file (e.g., .dude-cache/blocks.json).
    /// @param maxAge Maximum age for cache entries. Entries older than this are evicted on Load().
    explicit BlockCache(std::filesystem::path cachePath,
                        std::chrono::seconds maxAge = std::chrono::duration_cast<std::chrono::seconds>(DefaultMaxAge));

    /// @brief Looks up cached blocks for a file given its content hash and extraction parameters.
    /// @param contentHash SHA-256 hex digest of the file content.
    /// @param languageName Language name used for tokenization.
    /// @param minTokens Minimum token count used during extraction.
    /// @param textSensitivity Text sensitivity used during extraction.
    /// @return A span over the cached blocks if found, or std::nullopt.
    /// @note Updates the last-accessed timestamp on cache hit.
    [[nodiscard]] auto Lookup(std::string const& contentHash, std::string_view languageName, size_t minTokens,
                              double textSensitivity) -> std::optional<std::span<CodeBlock const>>;

    /// @brief Stores extracted blocks for a file in the cache.
    /// @param contentHash SHA-256 hex digest of the file content.
    /// @param languageName Language name used for tokenization.
    /// @param minTokens Minimum token count used during extraction.
    /// @param textSensitivity Text sensitivity used during extraction.
    /// @param blocks The extracted code blocks to cache.
    void Store(std::string const& contentHash, std::string_view languageName, size_t minTokens, double textSensitivity,
               std::vector<CodeBlock> const& blocks);

    /// @brief Loads the cache from disk.
    /// @return void on success, or an error.
    auto Load() -> std::expected<void, BlockCacheError>;

    /// @brief Persists the cache to disk if modified since last load/save.
    /// @return void on success, or an error. Returns success immediately if not dirty.
    auto Save() -> std::expected<void, BlockCacheError>;

    /// @brief Returns the number of cache entries.
    [[nodiscard]] auto Size() const -> size_t;

    /// @brief Clears all cache entries in memory.
    void Clear();

private:
    /// @brief Builds the composite cache key from parameters.
    [[nodiscard]] static auto MakeKey(std::string const& contentHash, std::string_view languageName, size_t minTokens,
                                      double textSensitivity) -> std::string;

    /// @brief A cache entry with blocks and a last-accessed timestamp.
    struct CacheEntry
    {
        std::vector<CodeBlock> blocks;
        int64_t lastAccessedEpoch = 0; ///< Unix epoch seconds when last accessed.
    };

    std::filesystem::path _cachePath;
    std::chrono::seconds _maxAge;
    std::unordered_map<std::string, CacheEntry> _entries;
    bool _dirty = false; ///< True when entries have been modified since last load/save.
};

} // namespace dude
