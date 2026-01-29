// SPDX-License-Identifier: Apache-2.0
#include <mcpprotocol/McpProtocol.hpp>

namespace mcpprotocol
{

auto BuildInitializeResult(ServerInfo const& info, bool hasTools, bool hasPrompts) -> nlohmann::json
{
    auto capabilities = nlohmann::json::object();
    if (hasTools)
        capabilities["tools"] = nlohmann::json::object();
    if (hasPrompts)
        capabilities["prompts"] = nlohmann::json::object();

    auto serverInfo = nlohmann::json{{"name", info.name}, {"version", info.version}};
    if (!info.title.empty())
        serverInfo["title"] = info.title;
    if (!info.description.empty())
        serverInfo["description"] = info.description;
    if (!info.websiteUrl.empty())
        serverInfo["websiteUrl"] = info.websiteUrl;

    return nlohmann::json{
        {"protocolVersion", kProtocolVersion},
        {"capabilities", capabilities},
        {"serverInfo", serverInfo},
    };
}

auto BuildToolsListResult(std::vector<ToolDescriptor> const& tools) -> nlohmann::json
{
    auto toolsArray = nlohmann::json::array();
    for (auto const& tool : tools)
    {
        auto entry = nlohmann::json{
            {"name", tool.name},
            {"description", tool.description},
            {"inputSchema", tool.inputSchema},
        };
        if (!tool.title.empty())
            entry["title"] = tool.title;
        if (!tool.outputSchema.is_null())
            entry["outputSchema"] = tool.outputSchema;

        entry["annotations"] = nlohmann::json{
            {"readOnlyHint", tool.annotations.readOnlyHint},
            {"destructiveHint", tool.annotations.destructiveHint},
            {"idempotentHint", tool.annotations.idempotentHint},
            {"openWorldHint", tool.annotations.openWorldHint},
        };

        toolsArray.push_back(std::move(entry));
    }
    return nlohmann::json{{"tools", toolsArray}};
}

auto BuildPromptsListResult(std::vector<PromptDescriptor> const& prompts) -> nlohmann::json
{
    auto promptsArray = nlohmann::json::array();
    for (auto const& prompt : prompts)
    {
        auto entry = nlohmann::json{
            {"name", prompt.name},
            {"description", prompt.description},
        };
        if (!prompt.title.empty())
            entry["title"] = prompt.title;
        if (!prompt.arguments.is_null() && !prompt.arguments.empty())
            entry["arguments"] = prompt.arguments;
        promptsArray.push_back(std::move(entry));
    }
    return nlohmann::json{{"prompts", promptsArray}};
}

auto BuildToolResult(std::string const& text, bool isError) -> nlohmann::json
{
    auto result = nlohmann::json{
        {"content", nlohmann::json::array({{{"type", "text"}, {"text", text}}})},
    };
    if (isError)
        result["isError"] = true;
    return result;
}

auto BuildToolResultJson(nlohmann::json const& data, bool isError) -> nlohmann::json
{
    return BuildToolResult(data.dump(2), isError);
}

} // namespace mcpprotocol
