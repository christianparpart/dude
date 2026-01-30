// SPDX-License-Identifier: Apache-2.0

#include <dude/HelpFormatter.hpp>

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace dude
{

namespace
{

/// @brief Returns an ANSI truecolor foreground escape sequence.
[[nodiscard]] auto Fg(RGB const& c) -> std::string
{
    return std::format("\033[38;2;{};{};{}m", c.r, c.g, c.b);
}

/// @brief Returns the ANSI bold escape sequence.
[[nodiscard]] constexpr auto Bold() -> std::string_view
{
    return "\033[1m";
}

/// @brief Returns the ANSI dim escape sequence.
[[nodiscard]] constexpr auto Dim() -> std::string_view
{
    return "\033[2m";
}

/// @brief Splits text into lines, preserving empty lines.
[[nodiscard]] auto SplitLines(std::string_view text) -> std::vector<std::string_view>
{
    std::vector<std::string_view> lines;
    size_t start = 0;
    while (start <= text.size())
    {
        auto const pos = text.find('\n', start);
        if (pos == std::string_view::npos)
        {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    return lines;
}

/// @brief Returns the leading whitespace prefix of a line.
[[nodiscard]] auto LeadingWhitespace(std::string_view line) -> std::string_view
{
    auto const pos = line.find_first_not_of(" \t");
    if (pos == std::string_view::npos)
        return line;
    return line.substr(0, pos);
}

/// @brief Returns the trimmed (leading whitespace removed) content of a line.
[[nodiscard]] auto Trimmed(std::string_view line) -> std::string_view
{
    auto const pos = line.find_first_not_of(" \t");
    if (pos == std::string_view::npos)
        return {};
    return line.substr(pos);
}

/// @brief Checks if a line consists only of repeated '=' characters (with optional whitespace).
[[nodiscard]] auto IsEqualsUnderline(std::string_view line) -> bool
{
    auto const trimmed = Trimmed(line);
    return !trimmed.empty() && std::ranges::all_of(trimmed, [](char c) { return c == '='; });
}

/// @brief Checks if a line consists only of repeated '-' characters (with optional whitespace).
[[nodiscard]] auto IsDashUnderline(std::string_view line) -> bool
{
    auto const trimmed = Trimmed(line);
    return !trimmed.empty() && std::ranges::all_of(trimmed, [](char c) { return c == '-'; });
}

/// @brief Checks if a trimmed line looks like a shell comment (starts with #).
[[nodiscard]] auto IsShellComment(std::string_view trimmed) -> bool
{
    return !trimmed.empty() && trimmed[0] == '#';
}

/// @brief Checks if a trimmed line looks like a shell command (starts with known command names).
[[nodiscard]] auto IsShellCommand(std::string_view trimmed) -> bool
{
    // Shell commands in the examples start with the tool name
    return trimmed.starts_with("dude");
}

/// @brief Checks if a line starts an option definition (e.g. "  -t, --threshold").
[[nodiscard]] auto IsOptionDefinition(std::string_view line) -> bool
{
    auto const trimmed = Trimmed(line);
    return trimmed.starts_with("-");
}

/// @brief Checks if a character can appear in a shell flag.
[[nodiscard]] constexpr auto IsFlagChar(char c) -> bool
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
}

/// @brief Checks if a character is a digit or dot (for numeric values).
[[nodiscard]] constexpr auto IsNumericChar(char c) -> bool
{
    return (c >= '0' && c <= '9') || c == '.';
}

/// @brief Checks if a string looks like a path (starts with / or ./).
[[nodiscard]] auto IsPath(std::string_view s) -> bool
{
    return s.starts_with("/") || s.starts_with("./");
}

/// @brief Checks if a string is a known shell command keyword.
[[nodiscard]] auto IsShellKeyword(std::string_view s) -> bool
{
    return s == "dude";
}

/// @brief State machine for shell line syntax highlighting.
///
/// Processes the line character by character, identifying:
/// - Command name (bold + keywords color)
/// - Flags (-f, --flag) in preprocessor color
/// - Numeric arguments in numbers color
/// - Paths (/path/to/...) in identifiers color
/// - Quoted strings in strings color
/// - Backslash continuation in operators color
[[nodiscard]] auto HighlightShellTokens(std::string_view line, ThemeColors const& colors) -> std::string
{
    std::string result;
    result.reserve(line.size() * 2); // Rough estimate for ANSI overhead

    auto const ws = LeadingWhitespace(line);
    result.append(ws);

    auto const content = line.substr(ws.size());
    size_t i = 0;

    // Track whether the first word (command name) has been processed.
    bool commandSeen = false;

    while (i < content.size())
    {
        // Whitespace pass-through
        if (content[i] == ' ' || content[i] == '\t')
        {
            result += content[i++];
            continue;
        }

        // Backslash continuation at end of line
        if (content[i] == '\\' && i + 1 >= content.size())
        {
            result += Fg(colors.operators);
            result += '\\';
            result += ansiReset;
            ++i;
            continue;
        }

        // Single-quoted string
        if (content[i] == '\'')
        {
            auto const end = content.find('\'', i + 1);
            auto const len = (end == std::string_view::npos) ? content.size() - i : end - i + 1;
            result += Fg(colors.strings);
            result.append(content.substr(i, len));
            result += ansiReset;
            i += len;
            continue;
        }

        // Double-quoted string
        if (content[i] == '"')
        {
            auto const end = content.find('"', i + 1);
            auto const len = (end == std::string_view::npos) ? content.size() - i : end - i + 1;
            result += Fg(colors.strings);
            result.append(content.substr(i, len));
            result += ansiReset;
            i += len;
            continue;
        }

        // Flag: starts with '-'
        if (content[i] == '-' && !commandSeen == false)
        {
            size_t j = i + 1;
            while (j < content.size() && IsFlagChar(content[j]))
                ++j;
            // Include a potential '=' separator attached to the flag (e.g., --flag=value)
            if (j < content.size() && content[j] == '=')
                ++j;
            result += Fg(colors.preprocessor);
            result.append(content.substr(i, j - i));
            result += ansiReset;
            i = j;
            continue;
        }

        // Numeric value: starts with a digit
        if (content[i] >= '0' && content[i] <= '9')
        {
            size_t j = i;
            while (j < content.size() && IsNumericChar(content[j]))
                ++j;
            result += Fg(colors.numbers);
            result.append(content.substr(i, j - i));
            result += ansiReset;
            i = j;
            continue;
        }

        // Path: starts with / or ./
        if (IsPath(content.substr(i)))
        {
            size_t j = i;
            while (j < content.size() && content[j] != ' ' && content[j] != '\t')
                ++j;
            result += Fg(colors.identifiers);
            result.append(content.substr(i, j - i));
            result += ansiReset;
            i = j;
            continue;
        }

        // Word token: collect a contiguous word
        {
            size_t j = i;
            while (j < content.size() && content[j] != ' ' && content[j] != '\t')
                ++j;
            auto const word = content.substr(i, j - i);
            if (!commandSeen && IsShellKeyword(word))
            {
                result += std::string(Bold());
                result += Fg(colors.keywords);
                result.append(word);
                result += ansiReset;
                commandSeen = true;
            }
            else
            {
                result.append(word);
            }
            i = j;
        }
    }

    return result;
}

/// @brief Highlight JSON syntax in a single line.
///
/// Identifies:
/// - "key": patterns → keywords color
/// - "string values" → strings color
/// - Numbers → numbers color
/// - Punctuation ({}[],:) → punctuation color
[[nodiscard]] auto HighlightJsonTokens(std::string_view line, ThemeColors const& colors) -> std::string
{
    std::string result;
    result.reserve(line.size() * 2);

    size_t i = 0;
    while (i < line.size())
    {
        // Whitespace pass-through
        if (line[i] == ' ' || line[i] == '\t')
        {
            result += line[i++];
            continue;
        }

        // JSON string (either key or value)
        if (line[i] == '"')
        {
            auto const end = line.find('"', i + 1);
            auto const len = (end == std::string_view::npos) ? line.size() - i : end - i + 1;
            auto const str = line.substr(i, len);

            // Check if this is a key (followed by ':')
            auto const afterStr = (end != std::string_view::npos) ? end + 1 : line.size();
            auto isKey = false;
            for (auto k = afterStr; k < line.size(); ++k)
            {
                if (line[k] == ' ' || line[k] == '\t')
                    continue;
                if (line[k] == ':')
                    isKey = true;
                break;
            }

            if (isKey)
                result += Fg(colors.keywords);
            else
                result += Fg(colors.strings);
            result.append(str);
            result += ansiReset;
            i += len;
            continue;
        }

        // JSON punctuation
        if (line[i] == '{' || line[i] == '}' || line[i] == '[' || line[i] == ']' || line[i] == ':' || line[i] == ',')
        {
            result += Fg(colors.punctuation);
            result += line[i];
            result += ansiReset;
            ++i;
            continue;
        }

        // Numbers
        if (line[i] >= '0' && line[i] <= '9')
        {
            size_t j = i;
            while (j < line.size() && IsNumericChar(line[j]))
                ++j;
            result += Fg(colors.numbers);
            result.append(line.substr(i, j - i));
            result += ansiReset;
            i = j;
            continue;
        }

        // Booleans and null
        for (auto const* const kw : {"true", "false", "null"})
        {
            auto const kwLen = std::string_view(kw).size();
            if (line.substr(i).starts_with(kw) &&
                (i + kwLen >= line.size() || !std::isalnum(static_cast<unsigned char>(line[i + kwLen]))))
            {
                result += Fg(colors.keywords);
                result.append(line.substr(i, kwLen));
                result += ansiReset;
                i += kwLen;
                goto nextChar;
            }
        }

        // Default: pass through
        result += line[i++];
        continue;

    nextChar:;
    }

    return result;
}

/// @brief Highlight an option definition line.
///
/// Identifies:
/// - Flags (-f, --long-flag) → preprocessor color
/// - <param> placeholders → strings color
/// - (default: value) → numbers color for value
/// - Description text → default
[[nodiscard]] auto HighlightOptionTokens(std::string_view line, ThemeColors const& colors) -> std::string
{
    std::string result;
    result.reserve(line.size() * 2);

    size_t i = 0;

    // Leading whitespace
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
        result += line[i++];

    // Parse flag definitions until we hit the description
    // Flags are like: -t, --threshold <N>
    // They end at double-space or at the description text.
    while (i < line.size())
    {
        // Flag: starts with '-'
        if (line[i] == '-')
        {
            size_t j = i + 1;
            while (j < line.size() && IsFlagChar(line[j]))
                ++j;
            result += Fg(colors.preprocessor);
            result.append(line.substr(i, j - i));
            result += ansiReset;
            i = j;
            continue;
        }

        // Comma separator between flags
        if (line[i] == ',')
        {
            result += line[i++];
            continue;
        }

        // Whitespace in flag area
        if (line[i] == ' ')
        {
            // Check for double-space (separator between flags and description)
            if (i + 1 < line.size() && line[i + 1] == ' ')
            {
                // We've reached the description portion — check for <param> first
                if (i + 1 < line.size())
                {
                    // Look ahead: is the next non-space a '<'? Then it's still a param.
                    size_t k = i;
                    while (k < line.size() && line[k] == ' ')
                        ++k;
                    if (k < line.size() && line[k] == '<')
                    {
                        // Output single space and continue to let the '<' handler work
                        result += line[i++];
                        continue;
                    }
                }
                // Output remaining as description text
                result.append(line.substr(i));
                break;
            }
            result += line[i++];
            continue;
        }

        // <param> placeholder
        if (line[i] == '<')
        {
            auto const end = line.find('>', i);
            if (end != std::string_view::npos)
            {
                auto const len = end - i + 1;
                result += Fg(colors.strings);
                result.append(line.substr(i, len));
                result += ansiReset;
                i += len;
                continue;
            }
        }

        // Anything else in the flag area — just pass through
        result += line[i++];
    }

    // Now scan the description portion for "(default: ...)" patterns
    // The description is already appended above; we need to re-scan.
    // Instead, let's find and highlight the default value inline.

    // Search for "(default:" in the result and highlight the value
    auto const defaultTag = std::string_view("(default: ");
    auto pos = result.find(defaultTag);
    if (pos != std::string::npos)
    {
        auto const valueStart = pos + defaultTag.size();
        auto const valueEnd = result.find(')', valueStart);
        if (valueEnd != std::string::npos)
        {
            auto const value = result.substr(valueStart, valueEnd - valueStart);
            auto highlighted = Fg(colors.numbers) + value + ansiReset;
            result.replace(valueStart, valueEnd - valueStart, highlighted);
        }
    }

    return result;
}

/// @brief Formats a classified line with appropriate ANSI styling.
[[nodiscard]] auto FormatLine(std::string_view line, HelpLineType type, ThemeColors const& colors) -> std::string
{
    switch (type)
    {
        case HelpLineType::Empty:
            return std::string(line);
        case HelpLineType::Title:
            return std::string(Bold()) + std::string(line) + std::string(ansiReset);
        case HelpLineType::TitleUnderline:
        case HelpLineType::SectionUnderline:
            return std::string(Dim()) + std::string(line) + std::string(ansiReset);
        case HelpLineType::SectionHeader:
            return std::string(Bold()) + std::string(line) + std::string(ansiReset);
        case HelpLineType::UsageLine:
            return std::string(Bold()) + std::string(line) + std::string(ansiReset);
        case HelpLineType::OptionsLabel:
            return std::string(Bold()) + std::string(line) + std::string(ansiReset);
        case HelpLineType::OptionLine:
            return HelpFormatter::HighlightOptionLine(line, colors);
        case HelpLineType::ShellComment:
            return std::string(LeadingWhitespace(line)) + Fg(colors.comments) + std::string(Trimmed(line)) +
                   std::string(ansiReset);
        case HelpLineType::ShellCommand:
        case HelpLineType::ShellContinuation:
            return HelpFormatter::HighlightShellLine(line, colors);
        case HelpLineType::JsonLine:
            return HelpFormatter::HighlightJsonLine(line, colors);
        case HelpLineType::PlainText:
            return std::string(line);
    }
    return std::string(line);
}

} // namespace

auto HelpFormatter::ClassifyLine(std::string_view line, int& jsonDepth, bool& inContinuation, bool isExamplesMode)
    -> HelpLineType
{
    auto const trimmed = Trimmed(line);

    // Empty line
    if (trimmed.empty())
    {
        inContinuation = false;
        return HelpLineType::Empty;
    }

    // Shell continuation from previous line
    if (inContinuation && isExamplesMode)
    {
        inContinuation = !trimmed.empty() && trimmed.back() == '\\';
        return HelpLineType::ShellContinuation;
    }

    // JSON context: if we're inside a JSON block, classify as JsonLine
    if (jsonDepth > 0 && isExamplesMode)
    {
        for (auto c : trimmed)
        {
            if (c == '{')
                ++jsonDepth;
            else if (c == '}')
                --jsonDepth;
        }
        return HelpLineType::JsonLine;
    }

    // JSON block start (line starts with '{')
    if (isExamplesMode && !trimmed.empty() && trimmed[0] == '{')
    {
        for (auto c : trimmed)
        {
            if (c == '{')
                ++jsonDepth;
            else if (c == '}')
                --jsonDepth;
        }
        return HelpLineType::JsonLine;
    }

    // Underlines
    if (isExamplesMode && IsEqualsUnderline(line))
        return HelpLineType::TitleUnderline;
    if (isExamplesMode && IsDashUnderline(line))
        return HelpLineType::SectionUnderline;

    // Help mode specific classifications
    if (!isExamplesMode)
    {
        if (trimmed.starts_with("Usage:"))
            return HelpLineType::UsageLine;
        if (trimmed == "Options:")
            return HelpLineType::OptionsLabel;
        if (IsOptionDefinition(line) && LeadingWhitespace(line).size() >= 2)
            return HelpLineType::OptionLine;
        // Continuation lines for options (deeply indented description text)
        if (LeadingWhitespace(line).size() >= 6 && !trimmed.starts_with("-"))
            return HelpLineType::PlainText;
        return HelpLineType::PlainText;
    }

    // Examples mode: shell comments
    if (IsShellComment(trimmed))
        return HelpLineType::ShellComment;

    // Examples mode: shell commands
    if (IsShellCommand(trimmed))
    {
        inContinuation = !trimmed.empty() && trimmed.back() == '\\';
        return HelpLineType::ShellCommand;
    }

    // Examples mode: lines with no leading whitespace are section headers or titles
    if (LeadingWhitespace(line).empty() && !trimmed.empty())
    {
        // The very first non-empty, non-underline line is the title
        return HelpLineType::SectionHeader;
    }

    return HelpLineType::PlainText;
}

auto HelpFormatter::FormatHelp(std::string_view text, bool useColor, ColorTheme theme) -> std::string
{
    if (!useColor)
        return std::string(text);

    auto const colors = GetThemeColors(theme);
    auto const lines = SplitLines(text);
    std::string result;
    result.reserve(text.size() * 2);

    int jsonDepth = 0;
    bool inContinuation = false;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        auto const type = ClassifyLine(lines[i], jsonDepth, inContinuation, /*isExamplesMode=*/false);
        result += FormatLine(lines[i], type, colors);
        if (i + 1 < lines.size())
            result += '\n';
    }

    return result;
}

auto HelpFormatter::FormatExamples(std::string_view text, bool useColor, ColorTheme theme) -> std::string
{
    if (!useColor)
        return std::string(text);

    auto const colors = GetThemeColors(theme);
    auto const lines = SplitLines(text);
    std::string result;
    result.reserve(text.size() * 2);

    int jsonDepth = 0;
    bool inContinuation = false;

    // Detect the title: the first non-empty line that is followed by '===' underline
    bool titleSeen = false;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        auto type = ClassifyLine(lines[i], jsonDepth, inContinuation, /*isExamplesMode=*/true);

        // Promote the first SectionHeader to Title if followed by '===' underline
        if (!titleSeen && type == HelpLineType::SectionHeader)
        {
            if (i + 1 < lines.size() && IsEqualsUnderline(lines[i + 1]))
            {
                type = HelpLineType::Title;
                titleSeen = true;
            }
        }

        result += FormatLine(lines[i], type, colors);
        if (i + 1 < lines.size())
            result += '\n';
    }

    return result;
}

auto HelpFormatter::HighlightShellLine(std::string_view line, ThemeColors const& colors) -> std::string
{
    return HighlightShellTokens(line, colors);
}

auto HelpFormatter::HighlightJsonLine(std::string_view line, ThemeColors const& colors) -> std::string
{
    return HighlightJsonTokens(line, colors);
}

auto HelpFormatter::HighlightOptionLine(std::string_view line, ThemeColors const& colors) -> std::string
{
    return HighlightOptionTokens(line, colors);
}

} // namespace dude
