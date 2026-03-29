// SPDX-License-Identifier: Apache-2.0

#include <nlohmann/json.hpp>

#include <dude/BaselineStore.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <ranges>
#include <string>
#include <unordered_set>

namespace dude
{

namespace
{

auto RelativePath(std::filesystem::path const& filePath, std::filesystem::path const& projectRoot) -> std::string
{
    return std::filesystem::relative(filePath, projectRoot).generic_string();
}

auto CurrentTimestamp() -> std::string
{
    auto const now = std::chrono::system_clock::now();
    auto const timeT = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &timeT);
#else
    gmtime_r(&timeT, &utc);
#endif
    return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}Z", utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                       utc.tm_hour, utc.tm_min, utc.tm_sec);
}

/// @brief Boost-style hash combine for building composite hashes.
void HashCombine(size_t& h, auto const& value)
{
    h ^= std::hash<std::remove_cvref_t<decltype(value)>>{}(value) + 0x9e3779b9 + (h << 6) + (h >> 2);
}

struct CloneIdentityHash
{
    auto operator()(CloneIdentity const& id) const -> size_t
    {
        size_t h = 0;
        for (auto const& m : id.members)
        {
            HashCombine(h, m.filePath);
            HashCombine(h, m.functionName);
            HashCombine(h, m.startLine);
            HashCombine(h, m.endLine);
        }
        return h;
    }
};

struct IntraCloneIdentityHash
{
    auto operator()(IntraCloneIdentity const& id) const -> size_t
    {
        size_t h = 0;
        HashCombine(h, id.filePath);
        HashCombine(h, id.functionName);
        HashCombine(h, id.startLine);
        HashCombine(h, id.regionAStart);
        HashCombine(h, id.regionALength);
        HashCombine(h, id.regionBStart);
        HashCombine(h, id.regionBLength);
        return h;
    }
};

} // namespace

BaselineStore::BaselineStore(std::filesystem::path baselineDir) : _baselineDir(std::move(baselineDir)) {}

auto BaselineStore::BaselinePath(std::string const& name) const -> std::filesystem::path
{
    return _baselineDir / (name + ".json");
}

auto BaselineStore::Save(std::string const& name, std::vector<CloneGroup> const& groups,
                         std::vector<IntraCloneResult> const& intraResults, std::vector<CodeBlock> const& blocks,
                         std::span<std::filesystem::path const> files, std::filesystem::path const& projectRoot)
    -> std::expected<void, BaselineError>
{
    std::error_code ec;
    std::filesystem::create_directories(_baselineDir, ec);
    if (ec)
        return std::unexpected(
            BaselineError{.message = std::format("Cannot create baseline directory: {}", ec.message())});

    auto const identities = BuildCloneIdentities(groups, blocks, files, projectRoot);
    auto const intraIdentities = BuildIntraCloneIdentities(intraResults, blocks, files, projectRoot);

    nlohmann::json root;
    root["version"] = 1;
    root["name"] = name;
    root["timestamp"] = CurrentTimestamp();

    auto groupsJson = nlohmann::json::array();
    for (auto const& identity : identities)
    {
        nlohmann::json groupJson;
        groupJson["avgSimilarity"] = identity.avgSimilarity;
        auto membersJson = nlohmann::json::array();
        for (auto const& member : identity.members)
        {
            membersJson.push_back(nlohmann::json{
                {"filePath", member.filePath},
                {"name", member.functionName},
                {"startLine", member.startLine},
                {"endLine", member.endLine},
            });
        }
        groupJson["members"] = std::move(membersJson);
        groupsJson.push_back(std::move(groupJson));
    }
    root["cloneGroups"] = std::move(groupsJson);

    auto intrasJson = nlohmann::json::array();
    for (auto const& identity : intraIdentities)
    {
        intrasJson.push_back(nlohmann::json{
            {"filePath", identity.filePath},
            {"name", identity.functionName},
            {"startLine", identity.startLine},
            {"endLine", identity.endLine},
            {"regionAStart", identity.regionAStart},
            {"regionALength", identity.regionALength},
            {"regionBStart", identity.regionBStart},
            {"regionBLength", identity.regionBLength},
            {"similarity", identity.similarity},
        });
    }
    root["intraClones"] = std::move(intrasJson);

    auto const path = BaselinePath(name);
    auto const tempPath = path.string() + ".tmp";
    {
        std::ofstream file(tempPath);
        if (!file)
            return std::unexpected(BaselineError{.message = std::format("Cannot write baseline file: {}", tempPath)});
        file << root.dump(2);
    }

    std::filesystem::rename(tempPath, path, ec);
    if (ec)
    {
        std::filesystem::remove(tempPath, ec);
        return std::unexpected(BaselineError{.message = std::format("Cannot rename baseline file: {}", ec.message())});
    }

    return {};
}

auto BaselineStore::Load(std::string const& name) const -> std::expected<Baseline, BaselineError>
{
    auto const path = BaselinePath(name);
    std::ifstream file(path);
    if (!file)
        return std::unexpected(BaselineError{.message = std::format("Baseline '{}' not found", name)});

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (nlohmann::json::parse_error const& e)
    {
        return std::unexpected(BaselineError{.message = std::format("Baseline parse error: {}", e.what())});
    }

    if (!root.contains("version") || root["version"].get<int>() != 1)
        return std::unexpected(BaselineError{.message = "Unsupported baseline version"});

    Baseline baseline;
    baseline.name = root.value("name", "");
    baseline.timestamp = root.value("timestamp", "");

    if (root.contains("cloneGroups"))
    {
        for (auto const& groupJson : root["cloneGroups"])
        {
            CloneIdentity identity;
            identity.avgSimilarity = groupJson.value("avgSimilarity", 0.0);
            if (groupJson.contains("members"))
            {
                for (auto const& memberJson : groupJson["members"])
                {
                    identity.members.push_back(CloneGroupMember{
                        .filePath = memberJson.value("filePath", ""),
                        .functionName = memberJson.value("name", ""),
                        .startLine = memberJson.value("startLine", 0U),
                        .endLine = memberJson.value("endLine", 0U),
                    });
                }
            }
            std::ranges::sort(identity.members);
            baseline.cloneGroups.push_back(std::move(identity));
        }
    }

    if (root.contains("intraClones"))
    {
        for (auto const& intraJson : root["intraClones"])
        {
            baseline.intraClones.push_back(IntraCloneIdentity{
                .filePath = intraJson.value("filePath", ""),
                .functionName = intraJson.value("name", ""),
                .startLine = intraJson.value("startLine", 0U),
                .endLine = intraJson.value("endLine", 0U),
                .regionAStart = intraJson.value("regionAStart", size_t{0}),
                .regionALength = intraJson.value("regionALength", size_t{0}),
                .regionBStart = intraJson.value("regionBStart", size_t{0}),
                .regionBLength = intraJson.value("regionBLength", size_t{0}),
                .similarity = intraJson.value("similarity", 0.0),
            });
        }
    }

    return baseline;
}

auto BaselineStore::Exists(std::string const& name) const -> bool
{
    return std::filesystem::exists(BaselinePath(name));
}

auto BaselineStore::List() const -> std::vector<std::string>
{
    std::vector<std::string> names;
    if (!std::filesystem::exists(_baselineDir))
        return names;

    for (auto const& entry : std::filesystem::directory_iterator(_baselineDir))
    {
        if (entry.path().extension() == ".json")
            names.push_back(entry.path().stem().string());
    }
    std::ranges::sort(names);
    return names;
}

auto BaselineStore::BuildCloneIdentities(std::vector<CloneGroup> const& groups, std::vector<CodeBlock> const& blocks,
                                         std::span<std::filesystem::path const> files,
                                         std::filesystem::path const& projectRoot) -> std::vector<CloneIdentity>
{
    // Pre-compute relative paths per file to avoid repeated filesystem::relative calls.
    std::unordered_map<uint32_t, std::string> relPathCache;
    auto const getRelPath = [&](uint32_t fileIdx) -> std::string const&
    {
        auto const [it, inserted] = relPathCache.try_emplace(fileIdx);
        if (inserted)
            it->second = RelativePath(files[fileIdx], projectRoot);
        return it->second;
    };

    std::vector<CloneIdentity> identities;
    identities.reserve(groups.size());

    for (auto const& group : groups)
    {
        CloneIdentity identity;
        identity.avgSimilarity = group.avgSimilarity;
        for (auto const blockIdx : group.blockIndices)
        {
            auto const& block = blocks[blockIdx];
            identity.members.push_back(CloneGroupMember{
                .filePath = getRelPath(block.sourceRange.start.fileIndex),
                .functionName = block.name,
                .startLine = block.sourceRange.start.line,
                .endLine = block.sourceRange.end.line,
            });
        }
        std::ranges::sort(identity.members);
        identities.push_back(std::move(identity));
    }

    return identities;
}

auto BaselineStore::BuildIntraCloneIdentities(std::vector<IntraCloneResult> const& results,
                                              std::vector<CodeBlock> const& blocks,
                                              std::span<std::filesystem::path const> files,
                                              std::filesystem::path const& projectRoot)
    -> std::vector<IntraCloneIdentity>
{
    std::unordered_map<uint32_t, std::string> relPathCache;
    auto const getRelPath = [&](uint32_t fileIdx) -> std::string const&
    {
        auto const [it, inserted] = relPathCache.try_emplace(fileIdx);
        if (inserted)
            it->second = RelativePath(files[fileIdx], projectRoot);
        return it->second;
    };

    std::vector<IntraCloneIdentity> identities;
    for (auto const& result : results)
    {
        auto const& block = blocks[result.blockIndex];
        auto const& relPath = getRelPath(block.sourceRange.start.fileIndex);

        for (auto const& pair : result.pairs)
        {
            identities.push_back(IntraCloneIdentity{
                .filePath = relPath,
                .functionName = block.name,
                .startLine = block.sourceRange.start.line,
                .endLine = block.sourceRange.end.line,
                .regionAStart = pair.regionA.start,
                .regionALength = pair.regionA.length,
                .regionBStart = pair.regionB.start,
                .regionBLength = pair.regionB.length,
                .similarity = pair.similarity,
            });
        }
    }
    return identities;
}

auto BaselineStore::FindNewCloneGroups(std::vector<CloneGroup> const& currentGroups, Baseline const& baseline,
                                       std::vector<CodeBlock> const& blocks,
                                       std::span<std::filesystem::path const> files,
                                       std::filesystem::path const& projectRoot) -> std::vector<CloneGroup>
{
    // Build a set of baseline identities for fast lookup.
    std::unordered_set<CloneIdentity, CloneIdentityHash> baselineSet(baseline.cloneGroups.begin(),
                                                                     baseline.cloneGroups.end());

    auto const currentIdentities = BuildCloneIdentities(currentGroups, blocks, files, projectRoot);

    std::vector<CloneGroup> newGroups;
    for (size_t i = 0; i < currentGroups.size(); ++i)
    {
        if (!baselineSet.contains(currentIdentities[i]))
            newGroups.push_back(currentGroups[i]);
    }
    return newGroups;
}

auto BaselineStore::FindNewIntraClones(std::vector<IntraCloneResult> const& currentResults, Baseline const& baseline,
                                       std::vector<CodeBlock> const& blocks,
                                       std::span<std::filesystem::path const> files,
                                       std::filesystem::path const& projectRoot) -> std::vector<IntraCloneResult>
{
    std::unordered_set<IntraCloneIdentity, IntraCloneIdentityHash> baselineSet(baseline.intraClones.begin(),
                                                                               baseline.intraClones.end());

    auto const currentIdentities = BuildIntraCloneIdentities(currentResults, blocks, files, projectRoot);

    // Build a set of current identity indices that are new.
    std::unordered_set<size_t> newIdentityIndices;
    for (size_t i = 0; i < currentIdentities.size(); ++i)
    {
        if (!baselineSet.contains(currentIdentities[i]))
            newIdentityIndices.insert(i);
    }

    // Reconstruct results keeping only new pairs.
    std::vector<IntraCloneResult> newResults;
    size_t identityIdx = 0;
    for (auto const& result : currentResults)
    {
        IntraCloneResult filtered;
        filtered.blockIndex = result.blockIndex;
        for (auto const& pair : result.pairs)
        {
            if (newIdentityIndices.contains(identityIdx))
                filtered.pairs.push_back(pair);
            ++identityIdx;
        }
        if (!filtered.pairs.empty())
            newResults.push_back(std::move(filtered));
    }
    return newResults;
}

} // namespace dude
