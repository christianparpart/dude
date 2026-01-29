# CodeDupDetector - Code Duplication Analysis Tool

A platform-independent C++23 CLI tool for **Type-2 clone detection** across multiple programming languages.
It identifies duplicated code blocks even when identifiers, types, or function names differ,
and supports interactive use via the [Model Context Protocol](https://modelcontextprotocol.io/) (MCP).

## Features

- **Multi-Language Support**: Built-in tokenizers and block extractors for C++, C#, and Python.
- **Type-2 Clone Detection**: Finds structurally identical code blocks regardless of renamed variables, types, or functions.
- **Function-Level Analysis**: Extracts and compares function bodies for meaningful, actionable results.
- **Intra-Function Clone Detection**: Finds duplicated regions within large functions using fingerprint self-join and maximal match extension.
- **Text Sensitivity Blending**: Configurable blend between structural and textual similarity scoring.
- **Analysis Scopes**: Selectively detect inter-file, intra-file, inter-function, or intra-function clones.
- **CI / Diff Mode**: Restrict analysis to code changed relative to a Git branch, suitable for CI gates.
- **Multiple Output Formats**: Human-readable console output with syntax highlighting, or structured JSON for tooling.
- **MCP Server Mode**: Expose analysis tools to AI coding assistants via JSON-RPC over stdio.
- **Git-Aware Scanning**: Respects `.gitignore` rules and supports glob-based filename filtering.
- **Syntax-Highlighted Output**: Truecolor (24-bit RGB) ANSI output with dark and light theme support.
- **Fast Fingerprint Filtering**: Rabin-Karp rolling hash fingerprints avoid expensive all-pairs comparison.
- **No External Dependencies**: Custom tokenizer, hand-written argument parser, no heavyweight libraries required.

## Supported Languages

| Language | Extensions | Block Extraction |
|----------|-----------|------------------|
| C++      | `.cpp`, `.cxx`, `.cc`, `.c`, `.h`, `.hpp`, `.hxx` | Brace-delimited function bodies |
| C#       | `.cs` | Brace-delimited method bodies |
| Python   | `.py`, `.pyw` | Indentation-based `def`/`class` blocks |

## Building

### Prerequisites

- C++23 compatible compiler (Clang 17+ or GCC 13+)
- CMake 3.25+
- Catch2 v3 (fetched automatically via CPM if not installed)

### Build Commands

```bash
# Configure and build (debug, with ASAN/UBSAN/clang-tidy)
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug

# Run tests
ctest --preset linux-clang-debug

# Release build
cmake --preset linux-clang-release
cmake --build --preset linux-clang-release
```

### Additional Build Presets

| Preset | Purpose |
|--------|---------|
| `linux-clang-debug` | Debug with ASAN, UBSAN, clang-tidy |
| `linux-clang-release` | Optimized release build |
| `linux-clang-release-native` | Release with `-march=native` |
| `linux-clang-release-static` | Statically linked release build |
| `linux-clang-coverage` | Code coverage instrumentation |
| `linux-clang-tsan` | ThreadSanitizer build |
| `linux-gcc-debug` | GCC debug build |

## Usage

```
codedupdetector [OPTIONS] <directory>
```

### Detection Parameters

| Option | Description |
|--------|-------------|
| `-t, --threshold <N>` | Similarity threshold 0.0-1.0 (default: `0.80`) |
| `-m, --min-tokens <N>` | Minimum block size in tokens (default: `30`) |
| `--text-sensitivity <N>` | Text sensitivity blend factor 0.0-1.0 (default: `0.3`) |

### File Filtering

| Option | Description |
|--------|-------------|
| `-e, --extensions <list>` | Comma-separated extensions (default: all registered languages) |
| `-g, --glob <pattern>` | Filename glob filter (repeatable, e.g. `-g '*Controller*'`) |
| `--encoding <enc>` | Input encoding: `auto`, `utf8`, `windows-1252` (default: `auto`) |
| `--gitignore` | Respect `.gitignore` when scanning (default) |
| `--no-gitignore` | Include gitignored files in analysis |

### Analysis Scope

| Option | Description |
|--------|-------------|
| `-s, --scope <scopes>` | Comma-separated scopes (default: `all`) |

Valid scopes: `inter-file`, `intra-file`, `inter-function`, `intra-function`, `all`.

### Output

| Option | Description |
|--------|-------------|
| `--reporter <spec>` | Output reporter: `console` (default), `json`, `json:file=<path>` |
| `--no-color` | Disable ANSI color output |
| `--no-source` | Don't print source code snippets |
| `--theme <dark\|light\|auto>` | Color theme (default: `auto`) |

### CI / Git Integration

| Option | Description |
|--------|-------------|
| `--diff-base <ref>` | Git ref to diff against (enables diff mode) |

### Miscellaneous

| Option | Description |
|--------|-------------|
| `-v, --verbose` | Show progress during scanning |
| `--mcp` | Run as MCP server (JSON-RPC over stdio) |
| `--show-examples` | Show categorized usage examples |
| `-h, --help` | Show help |
| `--version` | Show version |

### Examples

```bash
# Scan a project directory
codedupdetector /path/to/project/src/

# Higher threshold (only report near-identical clones)
codedupdetector -t 0.95 /path/to/project/

# Structural-only detection (ignore identifier names)
codedupdetector --text-sensitivity 0.0 /path/to/project/

# Scan only C# files
codedupdetector -e .cs /path/to/csharp_project/

# Scan only Python files
codedupdetector -e .py /path/to/python_project/

# Filter by filename pattern
codedupdetector -g '*Controller*' -g '*Service*' /path/to/project/

# Only detect cross-file clones
codedupdetector -s inter-file /path/to/project/

# Only detect within-function copy-paste
codedupdetector -s intra-function /path/to/project/

# JSON output to a file
codedupdetector --reporter json:file=report.json /path/to/project/

# CI gate: check changed code against a branch
codedupdetector --diff-base origin/main -t 0.90 --no-color --no-source \
    -s inter-file /path/to/project/

# Machine-readable output (no colors, no source)
codedupdetector --no-color --no-source /path/to/project/
```

### Exit Codes

| Code | Meaning |
|------|---------|
| `0` | No clones found |
| `1` | Clones detected |
| `2` | Error (invalid arguments, directory not found, etc.) |

## MCP Server Mode

CodeDupDetector can run as a [Model Context Protocol](https://modelcontextprotocol.io/) (MCP)
server, enabling AI coding assistants to analyze code duplication interactively.

### Starting the MCP Server

```bash
codedupdetector --mcp
```

This starts a JSON-RPC 2.0 server on stdio. The server exposes tools for
analyzing directories, querying clone groups, reading source code of duplicated
blocks, and generating reports.

### Available MCP Tools

| Tool | Description |
|------|-------------|
| `analyze_directory` | Scan a directory and detect code duplicates |
| `get_clone_groups` | Retrieve detected clone groups with pagination |
| `get_code_block` | Read source code of a specific code block |
| `query_file_duplicates` | Find all duplicates involving a specific file |
| `get_summary` | Get a summary report (text or JSON) |
| `configure_analysis` | Update detection parameters and re-analyze |

### Claude Code Integration

Add the server to your project configuration (`.mcp.json` in project root):

```json
{
  "mcpServers": {
    "codedupdetector": {
      "type": "stdio",
      "command": "/path/to/codedupdetector",
      "args": ["--mcp"]
    }
  }
}
```

Or add it via the CLI:

```bash
claude mcp add codedupdetector -- /path/to/codedupdetector --mcp
```

### Gemini CLI / Antigravity IDE Integration

Edit `~/.gemini/settings.json` or the MCP config (open via **Manage MCP Servers** ->
**View raw config**):

```json
{
  "mcpServers": {
    "codedupdetector": {
      "command": "/path/to/codedupdetector",
      "args": ["--mcp"]
    }
  }
}
```

Restart the IDE for changes to take effect.

## Algorithm

### Pipeline

1. **File Scanning**: Recursively find source files by extension, respecting `.gitignore` and glob filters.
2. **Tokenization**: Language-specific hand-written lexers produce token streams with source location tracking.
3. **Normalization**: Tokens are mapped to integer IDs. All identifiers map to a single generic ID; all numeric, string, and character literals to their respective generic IDs. Comments and preprocessor directives are stripped. This enables Type-2 clone detection. A parallel text-preserving normalization is computed for blended similarity scoring.
4. **Block Extraction**: Language-aware extraction of function/method bodies (brace-delimited for C++/C#, indentation-based for Python).
5. **Inter-Function Clone Detection** (two phases):
   - **Phase 1 - Fingerprinting**: Rabin-Karp rolling hash over sliding windows of normalized token IDs builds an inverted index. Candidate pairs share a minimum number of fingerprints.
   - **Phase 2 - LCS Similarity**: Longest Common Subsequence with Dice coefficient scoring on candidate pairs only.
6. **Grouping**: Union-Find merges clone pairs into connected components.
7. **Intra-Function Clone Detection** (three phases):
   - **Phase 1 - Fingerprint Self-Join**: Rolling hash fingerprints within a single block build an inverted index to find candidate position pairs.
   - **Phase 2 - Match Extension**: Candidate pairs are extended to maximal regions by matching tokens forward and backward.
   - **Phase 3 - Deduplication**: Redundant pairs (both regions substantially overlap) are merged, keeping all distinct clone pairs.
8. **Scope Filtering**: Results are filtered by the requested analysis scopes.
9. **Diff Filtering**: In diff mode, results are restricted to blocks touched by the diff.
10. **Reporting**: Results are formatted as syntax-highlighted console output or structured JSON.

### Key Design Decisions

- **Custom tokenizers** (no libclang): No heavyweight dependency, works on incomplete code, fast.
- **Function-level blocks**: More meaningful results than sliding-window approaches.
- **Dice coefficient**: `2 * |LCS| / (|A| + |B|)` handles different-length blocks well.
- **Fingerprint pre-filtering**: O(total_tokens) to build the index; LCS only computed for candidates.
- **Pluggable language system**: Adding a new language requires implementing a tokenizer and block extractor.
- **Blended similarity**: Weighted combination of structural and text-preserving similarity allows tuning for different use cases.

## License

Apache-2.0
