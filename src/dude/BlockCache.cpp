// SPDX-License-Identifier: Apache-2.0

#include <nlohmann/json.hpp>

#include <dude/BlockCache.hpp>
#include <dude/SourceLocation.hpp>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

namespace dude
{

namespace
{

/// @brief Returns the current time as Unix epoch seconds.
auto NowEpochSeconds() -> int64_t
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace

BlockCache::BlockCache(std::filesystem::path cachePath, std::chrono::seconds maxAge)
    : _cachePath(std::move(cachePath)), _maxAge(maxAge)
{
}

auto BlockCache::MakeKey(std::string const& contentHash, std::string_view languageName, size_t minTokens,
                         double textSensitivity) -> std::string
{
    return std::format("{}:{}:{}:{:.6f}", contentHash, languageName, minTokens, textSensitivity);
}

auto BlockCache::Lookup(std::string const& contentHash, std::string_view languageName, size_t minTokens,
                        double textSensitivity) -> std::optional<std::span<CodeBlock const>>
{
    auto const key = MakeKey(contentHash, languageName, minTokens, textSensitivity);
    auto const it = _entries.find(key);
    if (it == _entries.end())
        return std::nullopt;
    // Update last-accessed timestamp on cache hit so hot entries are not evicted.
    it->second.lastAccessedEpoch = NowEpochSeconds();
    _dirty = true;
    return std::span<CodeBlock const>{it->second.blocks};
}

void BlockCache::Store(std::string const& contentHash, std::string_view languageName, size_t minTokens,
                       double textSensitivity, std::vector<CodeBlock> const& blocks)
{
    auto const key = MakeKey(contentHash, languageName, minTokens, textSensitivity);
    _entries[key] = CacheEntry{.blocks = blocks, .lastAccessedEpoch = NowEpochSeconds()};
    _dirty = true;
}

auto BlockCache::Load() -> std::expected<void, BlockCacheError>
{
    if (!std::filesystem::exists(_cachePath))
        return {};

    std::ifstream file(_cachePath);
    if (!file)
        return std::unexpected(
            BlockCacheError{.message = std::format("Cannot open cache file: {}", _cachePath.string())});

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (nlohmann::json::parse_error const& e)
    {
        return std::unexpected(BlockCacheError{.message = std::format("Cache file parse error: {}", e.what())});
    }

    if (!root.contains("version") || root["version"].get<int>() != 1)
    {
        _entries.clear();
        return {};
    }

    if (!root.contains("entries") || !root["entries"].is_object())
        return {};

    auto const now = NowEpochSeconds();
    auto const cutoff = now - _maxAge.count();
    size_t evicted = 0;

    for (auto const& [key, value] : root["entries"].items())
    {
        if (!value.contains("blocks") || !value["blocks"].is_array())
            continue;

        auto const lastAccessed = value.value("lastAccessed", now);

        // Evict stale entries.
        if (lastAccessed < cutoff)
        {
            ++evicted;
            continue;
        }

        std::vector<CodeBlock> blocks;
        for (auto const& blockJson : value["blocks"])
        {
            CodeBlock block;
            block.name = blockJson.value("name", "");
            block.sourceRange.start.fileIndex = NoFileIndex;
            block.sourceRange.start.line = blockJson.value("startLine", 1U);
            block.sourceRange.start.column = blockJson.value("startColumn", 1U);
            block.sourceRange.end.fileIndex = NoFileIndex;
            block.sourceRange.end.line = blockJson.value("endLine", 1U);
            block.sourceRange.end.column = blockJson.value("endColumn", 1U);
            block.tokenStart = blockJson.value("tokenStart", size_t{0});
            block.tokenEnd = blockJson.value("tokenEnd", size_t{0});

            if (blockJson.contains("normalizedIds"))
                block.normalizedIds = blockJson["normalizedIds"].get<std::vector<NormalizedTokenId>>();
            if (blockJson.contains("textPreservingIds"))
                block.textPreservingIds = blockJson["textPreservingIds"].get<std::vector<NormalizedTokenId>>();

            blocks.push_back(std::move(block));
        }
        _entries[key] = CacheEntry{.blocks = std::move(blocks), .lastAccessedEpoch = lastAccessed};
    }

    // If entries were evicted, mark dirty so Save() will write the trimmed cache.
    if (evicted > 0)
        _dirty = true;

    return {};
}

auto BlockCache::Save() -> std::expected<void, BlockCacheError>
{
    if (!_dirty)
        return {};

    // Ensure parent directory exists.
    auto const parentDir = _cachePath.parent_path();
    if (!parentDir.empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(parentDir, ec);
        if (ec)
            return std::unexpected(
                BlockCacheError{.message = std::format("Cannot create cache directory: {}", ec.message())});
    }

    nlohmann::json root;
    root["version"] = 1;
    auto& entries = root["entries"];

    for (auto const& [key, entry] : _entries)
    {
        auto blocksJson = nlohmann::json::array();
        for (auto const& block : entry.blocks)
        {
            nlohmann::json blockJson;
            blockJson["name"] = block.name;
            blockJson["startLine"] = block.sourceRange.start.line;
            blockJson["startColumn"] = block.sourceRange.start.column;
            blockJson["endLine"] = block.sourceRange.end.line;
            blockJson["endColumn"] = block.sourceRange.end.column;
            blockJson["tokenStart"] = block.tokenStart;
            blockJson["tokenEnd"] = block.tokenEnd;
            blockJson["normalizedIds"] = block.normalizedIds;
            if (!block.textPreservingIds.empty())
                blockJson["textPreservingIds"] = block.textPreservingIds;
            blocksJson.push_back(std::move(blockJson));
        }
        entries[key] = nlohmann::json{
            {"blocks", std::move(blocksJson)},
            {"lastAccessed", entry.lastAccessedEpoch},
        };
    }

    // Write to a temp file first, then rename for atomicity.
    auto const tempPath = _cachePath.string() + ".tmp";
    {
        std::ofstream file(tempPath);
        if (!file)
            return std::unexpected(BlockCacheError{.message = std::format("Cannot write cache file: {}", tempPath)});
        file << root.dump(2);
    }

    std::error_code ec;
    std::filesystem::rename(tempPath, _cachePath, ec);
    if (ec)
    {
        std::filesystem::remove(tempPath, ec);
        return std::unexpected(BlockCacheError{.message = std::format("Cannot rename cache file: {}", ec.message())});
    }

    _dirty = false;
    return {};
}

auto BlockCache::Size() const -> size_t
{
    return _entries.size();
}

void BlockCache::Clear()
{
    _entries.clear();
}

} // namespace dude
