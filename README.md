# CodeDupDetector

A platform-independent C++ CLI tool that detects code duplication in C++ projects using Type-2 clone detection (identifier-normalized).

## Features

- **Type-2 Clone Detection**: Finds duplicated code blocks even when variable, type, or function names differ.
- **Function-Level Analysis**: Extracts and compares function bodies for meaningful results.
- **Intra-Function Clone Detection**: Finds duplicated regions within large functions using fingerprint self-join and maximal match extension.
- **Syntax-Highlighted Output**: Truecolor (24-bit RGB) ANSI output with dark and light theme support.
- **No External Dependencies**: Custom tokenizer, hand-written argument parser, no heavyweight libraries required.
- **Fast Fingerprint Filtering**: Rabin-Karp rolling hash fingerprints avoid expensive O(n^2) all-pairs comparison.

## Building

### Prerequisites

- C++23 compatible compiler (Clang 17+ or GCC 13+)
- CMake 3.25+
- Catch2 v3 (optional, fetched automatically if not installed)

### Build Commands

```bash
# Configure and build (debug)
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug

# Run tests
ctest --preset linux-clang-debug

# Release build
cmake --preset linux-clang-release
cmake --build --preset linux-clang-release
```

## Usage

```
codedupdetector [OPTIONS] <directory>

Options:
  -t, --threshold <N>         Similarity threshold 0.0-1.0 (default: 0.80)
  -m, --min-tokens <N>        Minimum block size in tokens (default: 30)
  --no-color                  Disable ANSI color output
  --no-source                 Don't print source code snippets
  --theme <dark|light|auto>   Color theme (default: auto)
  -e, --extensions <list>     Comma-separated extensions (default: .cpp,.cxx,.cc,.c,.h,.hpp,.hxx)
  --intra                     Enable intra-function clone detection (default)
  --no-intra                  Disable intra-function clone detection
  -v, --verbose               Show progress during scanning
  -h, --help                  Show help
  --version                   Show version
```

### Examples

```bash
# Scan a project directory
codedupdetector /path/to/project/src/

# Higher threshold (only report near-identical clones)
codedupdetector -t 0.95 /path/to/project/

# Machine-readable output (no colors, no source)
codedupdetector --no-color --no-source /path/to/project/

# Verbose mode with custom extensions
codedupdetector -v -e .cpp,.hpp /path/to/project/
```

### Exit Codes

- `0`: No clones found.
- `1`: Clones detected.
- `2`: Error (invalid arguments, directory not found, etc.).

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

Edit `~/.gemini/settings.json` or the MCP config (open via **Manage MCP Servers** →
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

1. **File Scanning**: Recursively find C++ source files by extension.
2. **Tokenization**: Hand-written lexer produces token streams with source location tracking.
3. **Normalization**: Tokens are mapped to integer IDs. All identifiers map to a single generic ID, all numeric/string/char literals to their respective generic IDs. Comments and preprocessor directives are stripped. This enables Type-2 clone detection.
4. **Block Extraction**: Function bodies are identified by heuristic pattern matching (parameter list followed by brace-delimited body).
5. **Inter-Function Clone Detection** (two phases):
   - **Phase 1 - Fingerprinting**: Rabin-Karp rolling hash over sliding windows of normalized token IDs builds an inverted index. Candidate pairs share a minimum number of fingerprints.
   - **Phase 2 - LCS Similarity**: Longest Common Subsequence with Dice coefficient scoring on candidate pairs only.
6. **Grouping**: Union-Find merges clone pairs into connected components.
7. **Intra-Function Clone Detection** (three phases):
   - **Phase 1 - Fingerprint Self-Join**: Rolling hash fingerprints within a single block build an inverted index to find candidate position pairs.
   - **Phase 2 - Match Extension**: Candidate pairs are extended to maximal regions by matching tokens forward and backward.
   - **Phase 3 - Deduplication**: Redundant pairs (both regions substantially overlap) are merged, keeping all distinct clone pairs.
8. **Reporting**: Results are printed with optional syntax-highlighted source snippets.

### Key Design Decisions

- **Custom tokenizer** (no libclang): No heavyweight dependency, works on incomplete code, fast.
- **Function-level blocks**: More meaningful results than sliding-window approaches.
- **Dice coefficient**: `2 * |LCS| / (|A| + |B|)` handles different-length blocks well.
- **Fingerprint pre-filtering**: O(total_tokens) to build the index, LCS only computed for candidates.

## Project Structure

```
CodeDupDetector/
├── CMakeLists.txt          # Root build configuration
├── CMakePresets.json        # Build presets
├── cmake/                   # CMake modules
├── src/
│   ├── codedup/             # Static library (libcodedup) — analysis engine
│   │   ├── Api.hpp                        # DLL export macro
│   │   ├── SourceLocation.hpp             # Source location and range types
│   │   ├── Token.hpp/cpp                  # Token types and helpers
│   │   ├── Tokenizer.hpp/cpp              # C++ lexer
│   │   ├── TokenNormalizer.hpp/cpp        # Type-2 normalization
│   │   ├── CodeBlock.hpp/cpp              # Function extraction
│   │   ├── CloneDetector.hpp/cpp          # Inter-function detection algorithm
│   │   ├── IntraFunctionDetector.hpp/cpp  # Intra-function detection algorithm
│   │   ├── RollingHash.hpp                # Shared Rabin-Karp hash utilities
│   │   ├── FileScanner.hpp/cpp            # Directory scanning
│   │   ├── SyntaxHighlighter.hpp          # ANSI color output
│   │   └── Reporter.hpp/cpp              # Result formatting
│   ├── mcpprotocol/         # Static library (libmcpprotocol) — JSON-RPC 2.0 + MCP spec
│   │   ├── JsonRpc.hpp/cpp                # JSON-RPC 2.0 parsing/serialization
│   │   ├── McpProtocol.hpp/cpp            # MCP protocol helpers and constants
│   │   └── McpServer.hpp/cpp              # MCP server state machine and stdio loop
│   ├── mcp/                 # Static library (libmcp) — MCP tools and analysis session
│   │   ├── AnalysisSession.hpp/cpp        # Cached analysis pipeline for MCP access
│   │   └── McpTooling.hpp/cpp             # Tool and prompt registration
│   ├── cli/                 # CLI executable
│   │   └── main.cpp
│   └── tests/               # Catch2 unit tests
└── .clang-format
```

## License

Apache-2.0
