// SPDX-License-Identifier: Apache-2.0
#include <mcp/McpTooling.hpp>

#include <codedup/AnalysisScope.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <ranges>
#include <string>

namespace mcp
{

namespace
{

/// @brief Builds summary stats JSON from an analysis session.
auto BuildSummaryStats(AnalysisSession const& session) -> nlohmann::json
{
    size_t totalDuplicatedLines = 0;
    for (auto const& group : session.CloneGroups())
        for (auto const blockIdx : group.blockIndices)
        {
            auto const& range = session.AllBlocks()[blockIdx].sourceRange;
            totalDuplicatedLines += range.end.line - range.start.line + 1;
        }

    size_t totalIntraPairs = 0;
    for (auto const& r : session.IntraResults())
        totalIntraPairs += r.pairs.size();

    auto const totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(session.Timing().Total()).count();

    return nlohmann::json{
        {"total_files", session.Files().size()},
        {"total_blocks", session.AllBlocks().size()},
        {"total_clone_groups", session.CloneGroups().size()},
        {"total_intra_pairs", totalIntraPairs},
        {"total_duplicated_lines", totalDuplicatedLines},
        {"timing_ms", totalMs},
    };
}

/// @brief Checks that analysis has been run.
auto RequireAnalysis(AnalysisSession const& session) -> std::expected<void, std::string>
{
    if (!session.HasResults())
        return std::unexpected(std::string("No analysis results available. Call analyze_directory first."));
    return {};
}

// ---------------------------------------------------------------------------
// Tool: analyze_directory
// ---------------------------------------------------------------------------

auto MakeAnalyzeDirectoryDescriptor() -> mcpprotocol::ToolDescriptor
{
    return {
        .name = "analyze_directory",
        .title = "Analyze Directory",
        .description = "Scan a directory for code duplication. Returns summary statistics.",
        .inputSchema =
            nlohmann::json{
                {"type", "object"},
                {"required", nlohmann::json::array({"directory"})},
                {"properties",
                 {
                     {"directory", {{"type", "string"}, {"description", "Path to the directory to scan"}}},
                     {"threshold",
                      {{"type", "number"}, {"description", "Similarity threshold 0.0-1.0 (default: 0.80)"}}},
                     {"min_tokens",
                      {{"type", "integer"}, {"description", "Minimum block size in tokens (default: 30)"}}},
                     {"text_sensitivity",
                      {{"type", "number"}, {"description", "Text sensitivity 0.0-1.0 (default: 0.3)"}}},
                     {"scope",
                      {{"type", "string"},
                       {"description",
                        "Analysis scope: all, inter-file, intra-file, intra-function, inter-function (default: all)"}}},
                     {"extensions",
                      {{"type", "array"},
                       {"items", {{"type", "string"}}},
                       {"description", "File extensions (converted to glob patterns, e.g. \".cpp\" → \"*.cpp\")"}}},
                     {"glob_patterns",
                      {{"type", "array"},
                       {"items", {{"type", "string"}}},
                       {"description", R"(Filename glob patterns (e.g. "*.cpp", "*Controller*"))"}}},
                 }},
            },
        .outputSchema = nullptr,
        .annotations = {.readOnlyHint = true,
                        .destructiveHint = false,
                        .idempotentHint = false,
                        .openWorldHint = false},
    };
}

auto HandleAnalyzeDirectory(AnalysisSession& session, nlohmann::json const& args)
    -> std::expected<nlohmann::json, std::string>
{
    AnalysisConfig config;
    config.directory = args.at("directory").get<std::string>();
    config.threshold = args.value("threshold", 0.80);
    config.minTokens = args.value("min_tokens", size_t{30});
    config.textSensitivity = args.value("text_sensitivity", 0.3);

    if (args.contains("scope"))
    {
        auto const scopeResult = codedup::ParseAnalysisScope(args["scope"].get<std::string>());
        if (!scopeResult)
            return std::unexpected(std::string("Invalid scope: ") + scopeResult.error().message);
        config.scope = *scopeResult;
    }

    if (args.contains("extensions"))
    {
        for (auto const& ext : args["extensions"])
        {
            auto s = ext.get<std::string>();
            if (!s.empty() && s[0] != '.')
                s.insert(0, ".");
            config.globPatterns.push_back("*" + s);
        }
    }
    if (args.contains("glob_patterns"))
    {
        for (auto const& pat : args["glob_patterns"])
            config.globPatterns.push_back(pat.get<std::string>());
    }

    auto result = session.Analyze(config);
    if (!result)
        return std::unexpected(result.error().message);

    return mcpprotocol::BuildToolResultJson(BuildSummaryStats(session));
}

// ---------------------------------------------------------------------------
// Tool: get_clone_groups
// ---------------------------------------------------------------------------

auto MakeGetCloneGroupsDescriptor() -> mcpprotocol::ToolDescriptor
{
    return {
        .name = "get_clone_groups",
        .title = "Get Clone Groups",
        .description = "Retrieve detected inter-function clone groups with pagination.",
        .inputSchema =
            nlohmann::json{
                {"type", "object"},
                {"properties",
                 {
                     {"limit", {{"type", "integer"}, {"description", "Maximum groups to return (default: 20)"}}},
                     {"offset", {{"type", "integer"}, {"description", "Number of groups to skip (default: 0)"}}},
                     {"min_similarity", {{"type", "number"}, {"description", "Minimum average similarity filter"}}},
                     {"file_filter", {{"type", "string"}, {"description", "Substring filter on file paths"}}},
                 }},
            },
        .outputSchema = nullptr,
        .annotations = {.readOnlyHint = true, .destructiveHint = false, .idempotentHint = true, .openWorldHint = false},
    };
}

auto HandleGetCloneGroups(AnalysisSession const& session, nlohmann::json const& args)
    -> std::expected<nlohmann::json, std::string>
{
    auto const check = RequireAnalysis(session);
    if (!check)
        return std::unexpected(check.error());

    auto const limit = args.value("limit", size_t{20});
    auto const offset = args.value("offset", size_t{0});
    auto const minSimilarity = args.value("min_similarity", 0.0);
    auto const fileFilter = args.value("file_filter", std::string{});

    auto const& groups = session.CloneGroups();
    auto const& blocks = session.AllBlocks();
    auto const& blockToFileIndex = session.BlockToFileIndex();
    auto const& files = session.Files();

    auto groupsArray = nlohmann::json::array();
    size_t groupIdx = 0;
    size_t emitted = 0;

    for (auto const& group : groups)
    {
        if (group.avgSimilarity < minSimilarity)
        {
            ++groupIdx;
            continue;
        }

        if (!fileFilter.empty())
        {
            bool matchesFilter = false;
            for (auto const bi : group.blockIndices)
            {
                auto const fi = blockToFileIndex[bi];
                if (files[fi].string().find(fileFilter) != std::string::npos)
                {
                    matchesFilter = true;
                    break;
                }
            }
            if (!matchesFilter)
            {
                ++groupIdx;
                continue;
            }
        }

        if (emitted < offset)
        {
            ++emitted;
            ++groupIdx;
            continue;
        }

        if (groupsArray.size() >= limit)
            break;

        auto blocksArray = nlohmann::json::array();
        for (auto const bi : group.blockIndices)
        {
            auto const fi = blockToFileIndex[bi];
            blocksArray.push_back({
                {"block_index", bi},
                {"name", blocks[bi].name},
                {"file", files[fi].string()},
                {"start_line", blocks[bi].sourceRange.start.line},
                {"end_line", blocks[bi].sourceRange.end.line},
                {"token_count", blocks[bi].tokenEnd - blocks[bi].tokenStart},
            });
        }

        groupsArray.push_back({
            {"group_index", groupIdx},
            {"block_count", group.blockIndices.size()},
            {"avg_similarity", group.avgSimilarity},
            {"blocks", blocksArray},
        });

        ++groupIdx;
    }

    return mcpprotocol::BuildToolResultJson(nlohmann::json{
        {"total_groups", groups.size()},
        {"returned", groupsArray.size()},
        {"groups", groupsArray},
    });
}

// ---------------------------------------------------------------------------
// Tool: get_code_block
// ---------------------------------------------------------------------------

auto MakeGetCodeBlockDescriptor() -> mcpprotocol::ToolDescriptor
{
    return {
        .name = "get_code_block",
        .title = "Get Code Block",
        .description = "Read the source code and metadata of a specific code block by index.",
        .inputSchema =
            nlohmann::json{
                {"type", "object"},
                {"required", nlohmann::json::array({"block_index"})},
                {"properties",
                 {
                     {"block_index", {{"type", "integer"}, {"description", "Index of the code block"}}},
                 }},
            },
        .outputSchema = nullptr,
        .annotations = {.readOnlyHint = true, .destructiveHint = false, .idempotentHint = true, .openWorldHint = false},
    };
}

auto HandleGetCodeBlock(AnalysisSession const& session, nlohmann::json const& args)
    -> std::expected<nlohmann::json, std::string>
{
    auto const check = RequireAnalysis(session);
    if (!check)
        return std::unexpected(check.error());

    auto const blockIndex = args.at("block_index").get<size_t>();
    auto const& blocks = session.AllBlocks();

    if (blockIndex >= blocks.size())
        return std::unexpected(std::format("Block index {} out of range (total: {})", blockIndex, blocks.size()));

    auto const& block = blocks[blockIndex];
    auto const fi = session.BlockToFileIndex()[blockIndex];

    auto sourceResult = session.ReadBlockSource(blockIndex);
    auto const sourceCode = sourceResult ? *sourceResult : std::string("(failed to read source)");

    return mcpprotocol::BuildToolResultJson(nlohmann::json{
        {"block_index", blockIndex},
        {"name", block.name},
        {"file", session.Files()[fi].string()},
        {"start_line", block.sourceRange.start.line},
        {"end_line", block.sourceRange.end.line},
        {"token_count", block.tokenEnd - block.tokenStart},
        {"source_code", sourceCode},
    });
}

// ---------------------------------------------------------------------------
// Tool: query_file_duplicates
// ---------------------------------------------------------------------------

auto MakeQueryFileDuplicatesDescriptor() -> mcpprotocol::ToolDescriptor
{
    return {
        .name = "query_file_duplicates",
        .title = "Query File Duplicates",
        .description = "Find all clone groups and intra-function clones involving a specific file.",
        .inputSchema =
            nlohmann::json{
                {"type", "object"},
                {"required", nlohmann::json::array({"file_path"})},
                {"properties",
                 {
                     {"file_path", {{"type", "string"}, {"description", "File path (substring match)"}}},
                 }},
            },
        .outputSchema = nullptr,
        .annotations = {.readOnlyHint = true, .destructiveHint = false, .idempotentHint = true, .openWorldHint = false},
    };
}

auto HandleQueryFileDuplicates(AnalysisSession const& session, nlohmann::json const& args)
    -> std::expected<nlohmann::json, std::string>
{
    auto const check = RequireAnalysis(session);
    if (!check)
        return std::unexpected(check.error());

    auto const filePath = args.at("file_path").get<std::string>();
    auto const& groups = session.CloneGroups();
    auto const& blocks = session.AllBlocks();
    auto const& blockToFileIndex = session.BlockToFileIndex();
    auto const& files = session.Files();
    auto const& intraResults = session.IntraResults();

    // Find inter-function clone groups involving this file
    auto interGroups = nlohmann::json::array();
    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        bool involves = false;
        for (auto const bi : groups[gi].blockIndices)
        {
            auto const fi = blockToFileIndex[bi];
            if (files[fi].string().find(filePath) != std::string::npos)
            {
                involves = true;
                break;
            }
        }
        if (!involves)
            continue;

        auto blocksArray = nlohmann::json::array();
        for (auto const bi : groups[gi].blockIndices)
        {
            auto const fi = blockToFileIndex[bi];
            blocksArray.push_back({
                {"block_index", bi},
                {"name", blocks[bi].name},
                {"file", files[fi].string()},
                {"start_line", blocks[bi].sourceRange.start.line},
                {"end_line", blocks[bi].sourceRange.end.line},
            });
        }
        interGroups.push_back({
            {"group_index", gi},
            {"avg_similarity", groups[gi].avgSimilarity},
            {"blocks", blocksArray},
        });
    }

    // Find intra-function clones in this file
    auto intraArray = nlohmann::json::array();
    for (auto const& result : intraResults)
    {
        auto const fi = blockToFileIndex[result.blockIndex];
        if (files[fi].string().find(filePath) == std::string::npos)
            continue;

        auto pairsArray = nlohmann::json::array();
        for (auto const& pair : result.pairs)
        {
            pairsArray.push_back({
                {"region_a_start", pair.regionA.start},
                {"region_a_length", pair.regionA.length},
                {"region_b_start", pair.regionB.start},
                {"region_b_length", pair.regionB.length},
                {"similarity", pair.similarity},
            });
        }
        intraArray.push_back({
            {"block_index", result.blockIndex},
            {"name", blocks[result.blockIndex].name},
            {"file", files[fi].string()},
            {"pairs", pairsArray},
        });
    }

    return mcpprotocol::BuildToolResultJson(nlohmann::json{
        {"file_filter", filePath},
        {"inter_function_clones", interGroups},
        {"intra_function_clones", intraArray},
    });
}

// ---------------------------------------------------------------------------
// Tool: get_summary
// ---------------------------------------------------------------------------

auto MakeGetSummaryDescriptor() -> mcpprotocol::ToolDescriptor
{
    return {
        .name = "get_summary",
        .title = "Get Summary",
        .description = "Get a summary report of the analysis results.",
        .inputSchema =
            nlohmann::json{
                {"type", "object"},
                {"properties",
                 {
                     {"format",
                      {{"type", "string"},
                       {"enum", nlohmann::json::array({"text", "json"})},
                       {"description", "Output format (default: text)"}}},
                 }},
            },
        .outputSchema = nullptr,
        .annotations = {.readOnlyHint = true, .destructiveHint = false, .idempotentHint = true, .openWorldHint = false},
    };
}

auto HandleGetSummary(AnalysisSession const& session, nlohmann::json const& args)
    -> std::expected<nlohmann::json, std::string>
{
    auto const check = RequireAnalysis(session);
    if (!check)
        return std::unexpected(check.error());

    auto const format = args.value("format", std::string("text"));

    if (format == "json")
    {
        auto stats = BuildSummaryStats(session);
        stats["directory"] = session.Config().directory.string();
        stats["threshold"] = session.Config().threshold;
        stats["min_tokens"] = session.Config().minTokens;
        stats["text_sensitivity"] = session.Config().textSensitivity;
        stats["scope"] = codedup::FormatAnalysisScope(session.Config().scope);
        return mcpprotocol::BuildToolResultJson(stats);
    }

    // Text format
    size_t totalDuplicatedLines = 0;
    size_t totalFunctions = 0;
    for (auto const& group : session.CloneGroups())
        for (auto const blockIdx : group.blockIndices)
        {
            auto const& range = session.AllBlocks()[blockIdx].sourceRange;
            totalDuplicatedLines += range.end.line - range.start.line + 1;
            ++totalFunctions;
        }

    size_t totalIntraPairs = 0;
    for (auto const& r : session.IntraResults())
        totalIntraPairs += r.pairs.size();

    auto const totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(session.Timing().Total()).count();

    auto text = std::format("Analysis Summary\n"
                            "================\n"
                            "Directory:           {}\n"
                            "Files scanned:       {}\n"
                            "Code blocks:         {}\n"
                            "Clone groups:        {}\n"
                            "Duplicated lines:    {}\n"
                            "Functions in clones: {}\n"
                            "Intra-function pairs: {}\n"
                            "Total time:          {} ms\n"
                            "Threshold:           {:.2f}\n"
                            "Min tokens:          {}\n"
                            "Text sensitivity:    {:.2f}\n"
                            "Scope:               {}",
                            session.Config().directory.string(), session.Files().size(), session.AllBlocks().size(),
                            session.CloneGroups().size(), totalDuplicatedLines, totalFunctions, totalIntraPairs,
                            totalMs, session.Config().threshold, session.Config().minTokens,
                            session.Config().textSensitivity, codedup::FormatAnalysisScope(session.Config().scope));

    return mcpprotocol::BuildToolResult(text);
}

// ---------------------------------------------------------------------------
// Tool: configure_analysis
// ---------------------------------------------------------------------------

auto MakeConfigureAnalysisDescriptor() -> mcpprotocol::ToolDescriptor
{
    return {
        .name = "configure_analysis",
        .title = "Configure Analysis",
        .description = "Update detection parameters and re-run analysis on cached data. Returns updated summary.",
        .inputSchema =
            nlohmann::json{
                {"type", "object"},
                {"properties",
                 {
                     {"threshold", {{"type", "number"}, {"description", "Similarity threshold 0.0-1.0"}}},
                     {"min_tokens", {{"type", "integer"}, {"description", "Minimum block size in tokens"}}},
                     {"text_sensitivity", {{"type", "number"}, {"description", "Text sensitivity 0.0-1.0"}}},
                     {"scope",
                      {{"type", "string"}, {"description", "Analysis scope: all, inter-file, intra-file, etc."}}},
                 }},
            },
        .outputSchema = nullptr,
        .annotations = {.readOnlyHint = true,
                        .destructiveHint = false,
                        .idempotentHint = false,
                        .openWorldHint = false},
    };
}

auto HandleConfigureAnalysis(AnalysisSession& session, nlohmann::json const& args)
    -> std::expected<nlohmann::json, std::string>
{
    auto const check = RequireAnalysis(session);
    if (!check)
        return std::unexpected(check.error());

    auto threshold = args.value("threshold", session.Config().threshold);
    auto minTokens = args.value("min_tokens", session.Config().minTokens);
    auto textSensitivity = args.value("text_sensitivity", session.Config().textSensitivity);
    auto scope = session.Config().scope;

    if (args.contains("scope"))
    {
        auto const scopeResult = codedup::ParseAnalysisScope(args["scope"].get<std::string>());
        if (!scopeResult)
            return std::unexpected(std::string("Invalid scope: ") + scopeResult.error().message);
        scope = *scopeResult;
    }

    auto result = session.Reconfigure(threshold, minTokens, textSensitivity, scope);
    if (!result)
        return std::unexpected(result.error().message);

    return mcpprotocol::BuildToolResultJson(BuildSummaryStats(session));
}

// ---------------------------------------------------------------------------
// Prompt: analyze_and_report
// ---------------------------------------------------------------------------

auto MakeAnalyzeAndReportDescriptor() -> mcpprotocol::PromptDescriptor
{
    return {
        .name = "analyze_and_report",
        .title = "Analyze and Report",
        .description =
            "Analyze a directory for code duplication and produce a detailed report with refactoring suggestions.",
        .arguments = nlohmann::json::array({
            {{"name", "directory"}, {"description", "Path to the directory to analyze"}, {"required", true}},
        }),
    };
}

auto HandleAnalyzeAndReport(nlohmann::json const& args) -> std::expected<nlohmann::json, std::string>
{
    auto const directory = args.value("directory", std::string{});
    if (directory.empty())
        return std::unexpected(std::string("Missing required argument: directory"));

    auto const message =
        std::format("Please analyze the directory '{}' for code duplication and produce a detailed report.\n\n"
                    "Steps:\n"
                    "1. Call the `analyze_directory` tool with the directory path.\n"
                    "2. Call `get_clone_groups` to retrieve the detected clone groups.\n"
                    "3. For clone groups with high similarity (>= 0.90), use `get_code_block` to read the source code "
                    "of each block in the group.\n"
                    "4. For each high-similarity group, suggest specific refactoring strategies:\n"
                    "   - Extract shared logic into a common function or template.\n"
                    "   - Use polymorphism or strategy pattern if variants differ by behavior.\n"
                    "   - Consolidate into a parameterized helper if differences are minor.\n"
                    "5. Provide a summary with the total number of clone groups, the most critical ones, "
                    "and an overall assessment of the codebase's duplication level.",
                    directory);

    return nlohmann::json{
        {"messages", nlohmann::json::array({
                         {{"role", "user"}, {"content", {{"type", "text"}, {"text", message}}}},
                     })},
    };
}

// ---------------------------------------------------------------------------
// Prompt: review_file
// ---------------------------------------------------------------------------

auto MakeReviewFileDescriptor() -> mcpprotocol::PromptDescriptor
{
    return {
        .name = "review_file",
        .title = "Review File",
        .description = "Review a specific file for code duplication and suggest improvements.",
        .arguments = nlohmann::json::array({
            {{"name", "file_path"}, {"description", "Path to the file to review"}, {"required", true}},
        }),
    };
}

auto HandleReviewFile(nlohmann::json const& args) -> std::expected<nlohmann::json, std::string>
{
    auto const filePath = args.value("file_path", std::string{});
    if (filePath.empty())
        return std::unexpected(std::string("Missing required argument: file_path"));

    auto const message =
        std::format("Please review the file '{}' for code duplication.\n\n"
                    "Steps:\n"
                    "1. If no analysis has been run yet, call `analyze_directory` with the project directory.\n"
                    "2. Call `query_file_duplicates` with the file path to find all duplicates involving this file.\n"
                    "3. For any inter-function clone groups found, use `get_code_block` to read the source code.\n"
                    "4. Examine the duplicated code and suggest specific improvements:\n"
                    "   - Identify which blocks are duplicates of each other.\n"
                    "   - Suggest concrete refactoring steps to eliminate the duplication.\n"
                    "   - Note any intra-function duplication that could be simplified.\n"
                    "5. Provide a file-level summary with the overall duplication assessment.",
                    filePath);

    return nlohmann::json{
        {"messages", nlohmann::json::array({
                         {{"role", "user"}, {"content", {{"type", "text"}, {"text", message}}}},
                     })},
    };
}

} // anonymous namespace

void RegisterCodeDupTools(mcpprotocol::McpServer& server, AnalysisSession& session)
{
    // Tools
    server.RegisterTool(MakeAnalyzeDirectoryDescriptor(),
                        [&session](auto const& args) { return HandleAnalyzeDirectory(session, args); });

    server.RegisterTool(MakeGetCloneGroupsDescriptor(),
                        [&session](auto const& args) { return HandleGetCloneGroups(session, args); });

    server.RegisterTool(MakeGetCodeBlockDescriptor(),
                        [&session](auto const& args) { return HandleGetCodeBlock(session, args); });

    server.RegisterTool(MakeQueryFileDuplicatesDescriptor(),
                        [&session](auto const& args) { return HandleQueryFileDuplicates(session, args); });

    server.RegisterTool(MakeGetSummaryDescriptor(),
                        [&session](auto const& args) { return HandleGetSummary(session, args); });

    server.RegisterTool(MakeConfigureAnalysisDescriptor(),
                        [&session](auto const& args) { return HandleConfigureAnalysis(session, args); });

    // Prompts
    server.RegisterPrompt(MakeAnalyzeAndReportDescriptor(),
                          [](auto const& args) { return HandleAnalyzeAndReport(args); });

    server.RegisterPrompt(MakeReviewFileDescriptor(), [](auto const& args) { return HandleReviewFile(args); });
}

} // namespace mcp
