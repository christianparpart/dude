# dude — Code Duplication Detector

![C++23](https://img.shields.io/badge/standard-C%2B%2B23-blue.svg)
![Build](https://github.com/LASTRADA-Software/dude/actions/workflows/workflow.yml/badge.svg?branch=master)
![License](https://img.shields.io/badge/license-Apache%202.0-green.svg)

A platform-independent C++23 CLI tool for **Type-2 clone detection** across multiple programming languages.
It identifies duplicated code blocks even when identifiers, types, or literal values differ —
useful for code reviews, refactoring, and CI quality gates.
Integrates with AI coding assistants via the [Model Context Protocol](https://modelcontextprotocol.io/) (MCP).

## Features

- **Type-2 Clone Detection** — finds structurally identical code blocks regardless of renamed variables, types, or functions.
- **Multi-Language Support** — built-in tokenizers for C++, C#, and Python.
- **Function-Level & Intra-Function Analysis** — extracts and compares function bodies, and detects duplicated regions within large functions.
- **Text Sensitivity Blending** — configurable blend between structural and textual similarity scoring.
- **Analysis Scopes** — selectively detect inter-file, intra-file, inter-function, or intra-function clones.
- **CI / Diff Mode** — restrict analysis to code changed relative to a Git branch, suitable for CI gates.
- **Multiple Output Formats** — human-readable console output with syntax highlighting, or structured JSON for tooling.
- **MCP Server Mode** — expose analysis tools to AI coding assistants via JSON-RPC over stdio.
- **Git-Aware Scanning** — respects `.gitignore` rules and supports glob-based filename filtering.
- **Syntax-Highlighted Output** — truecolor (24-bit RGB) ANSI output with dark and light theme support.
- **Fast Fingerprint Filtering** — Rabin-Karp rolling hash fingerprints with a five-stage filter cascade to avoid expensive all-pairs comparison.
- **No External Runtime Dependencies** — custom tokenizer, hand-written argument parser; build dependencies are fetched automatically.

## Quick Start

```bash
# Build
cmake --preset clang-release
cmake --build --preset clang-release

# Scan a project for duplicated code
./build/clang-release/src/cli/dude /path/to/project/src/

# Only C++ files, high similarity threshold
./build/clang-release/src/cli/dude -t 0.95 -g '*.cpp' -g '*.hpp' /path/to/project/

# CI gate: check for duplicates in changed code
./build/clang-release/src/cli/dude --diff-base origin/main --no-color --no-source /path/to/project/
```

## Supported Languages

| Language | Extensions |
|----------|------------|
| C++ | `.cpp`, `.hpp`, `.cc`, `.cxx`, `.h` |
| C# | `.cs` |
| Python | `.py` |

## Building

### Prerequisites

- C++23 compatible compiler (Clang 17+ or GCC 13+)
- CMake 3.25+
- Ninja (recommended)

All library dependencies are fetched automatically at configure time via [CPM](https://github.com/cpm-cmake/CPM.cmake):
Catch2 v3, nlohmann/json, unordered\_dense, NVIDIA/stdexec.

### Build Commands

```bash
# Release build
cmake --preset clang-release
cmake --build --preset clang-release

# Debug build (with ASAN, UBSAN, clang-tidy)
cmake --preset clang-debug
cmake --build --preset clang-debug

# Run tests
ctest --preset clang-debug

# Release with native arch tuning (for benchmarking)
cmake --preset clang-release-native
cmake --build --preset clang-release-native

# Static binary (single portable executable)
cmake --preset clang-release-static
cmake --build --preset clang-release-static
```

Additional presets are available for code coverage (`clang-coverage`), ThreadSanitizer (`clang-tsan`),
Profile-Guided Optimization (`clang-pgo-instrument`, `clang-pgo-optimize`), GCC (`gcc-debug`), and Windows (MSVC / Clang-CL).
See `CMakePresets.json` for the full list.

## Installation

### From Source

```bash
cmake --preset clang-release
cmake --build --preset clang-release
sudo cmake --install build/clang-release --prefix /usr/local
```

This installs the `dude` binary to `/usr/local/bin/`.

### Static Binary

Build a fully static executable that can be copied to any Linux machine without runtime dependencies:

```bash
cmake --preset clang-release-static
cmake --build --preset clang-release-static
cp build/clang-release-static/src/cli/dude /usr/local/bin/
```

Pre-built static binaries for Linux and Windows are available as
[CI build artifacts](https://github.com/LASTRADA-Software/dude/actions).

## Usage

```
dude [OPTIONS] <directory>
```

### Detection Parameters

| Option | Description |
|--------|-------------|
| `-t, --threshold <N>` | Similarity threshold 0.0–1.0 (default: `0.80`) |
| `-m, --min-tokens <N>` | Minimum block size in tokens (default: `30`) |
| `--text-sensitivity <N>` | Text sensitivity blend factor 0.0–1.0 (default: `0.3`) |

### File Filtering

| Option | Description |
|--------|-------------|
| `-g, --glob <pattern>` | Filename glob filter (repeatable, e.g. `-g '*.cpp'` `-g '*Controller*'`) |
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
| `-p, --progress` | Show progress bars during analysis |
| `-v, --verbose` | Show verbose diagnostics during scanning |
| `--mcp` | Run as MCP server (JSON-RPC over stdio) |
| `--show-examples` | Show categorized usage examples |
| `-h, --help` | Show help |
| `--version` | Show version |

### Examples

```bash
# Scan a project directory
dude /path/to/project/src/

# Higher threshold (only report near-identical clones)
dude -t 0.95 /path/to/project/

# Structural-only detection (ignore identifier names entirely)
dude --text-sensitivity 0.0 /path/to/project/

# Scan only C# files
dude -g '*.cs' /path/to/csharp_project/

# Scan only Python files
dude -g '*.py' /path/to/python_project/

# Filter by filename pattern
dude -g '*Controller*' -g '*Service*' /path/to/project/

# Only detect cross-file clones
dude -s inter-file /path/to/project/

# Only detect within-function copy-paste
dude -s intra-function /path/to/project/

# JSON output to a file
dude --reporter json:file=report.json /path/to/project/

# CI gate: check changed code against a branch
dude --diff-base origin/main -t 0.90 --no-color --no-source \
    -s inter-file /path/to/project/

# Machine-readable output (no colors, no source)
dude --no-color --no-source /path/to/project/
```

### Exit Codes

| Code | Meaning |
|------|---------|
| `0` | No clones found |
| `1` | Clones detected |
| `2` | Error (invalid arguments, directory not found, etc.) |

## MCP Server Mode

The tool can run as a [Model Context Protocol](https://modelcontextprotocol.io/) (MCP)
server, enabling AI coding assistants to analyze code duplication interactively.

### Starting the MCP Server

```bash
dude --mcp
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
    "dude": {
      "type": "stdio",
      "command": "/path/to/dude",
      "args": ["--mcp"]
    }
  }
}
```

Or add it via the CLI:

```bash
claude mcp add dude -- /path/to/dude --mcp
```

### Gemini CLI / Antigravity IDE Integration

Edit `~/.gemini/settings.json` or the MCP config (open via **Manage MCP Servers** ->
**View raw config**):

```json
{
  "mcpServers": {
    "dude": {
      "command": "/path/to/dude",
      "args": ["--mcp"]
    }
  }
}
```

Restart the IDE for changes to take effect.

## How It Works

The detection pipeline runs in five phases:

1. **File Scanning** — recursive directory walk with `.gitignore` and glob filtering.
2. **Tokenization** — parallelized across all CPU cores via the stdexec P2300 execution framework.
3. **Normalization & Block Extraction** — structural (Type-2) and text-preserving normalization; function-level block extraction.
4. **Clone Detection** — five-stage filter cascade (static fingerprint, adaptive fingerprint, length-ratio, bag-of-tokens Dice, threshold-aware bit-parallel LCS), with Union-Find grouping into connected components.
5. **Reporting** — console output with syntax-highlighted diffs, or structured JSON.

For the full algorithm description, including mathematical foundations and performance engineering details,
see [`docs/algorithm.md`](docs/algorithm.md).

## License

Apache-2.0
