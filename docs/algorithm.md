# Code Duplication Detection Algorithm

This document describes how CodeDupDetector identifies duplicated code across and within
source files. It covers the full analysis pipeline, the core detection algorithms,
the data structures involved, and the performance engineering that makes the tool
practical on large codebases.

## Table of Contents

- [Pipeline Overview](#pipeline-overview)
- [Phase 1: File Scanning](#phase-1-file-scanning)
- [Phase 2: Tokenization](#phase-2-tokenization)
- [Phase 3: Token Normalization and Block Extraction](#phase-3-token-normalization-and-block-extraction)
  - [Normalization Modes](#normalization-modes)
  - [Block Extraction](#block-extraction)
- [Phase 4: Clone Detection](#phase-4-clone-detection)
  - [Inter-Function Clone Detection](#inter-function-clone-detection)
    - [Step 1: Fingerprint-Based Candidate Generation](#step-1-fingerprint-based-candidate-generation)
    - [Step 2: LCS-Based Similarity Computation](#step-2-lcs-based-similarity-computation)
    - [Step 3: Grouping via Union-Find](#step-3-grouping-via-union-find)
  - [Intra-Function Clone Detection](#intra-function-clone-detection)
  - [Scope Filtering and Diff Filtering](#scope-filtering-and-diff-filtering)
- [Phase 5: Reporting](#phase-5-reporting)
- [Performance Engineering](#performance-engineering)
  - [Rabin-Karp Rolling Hash with Mersenne Prime Arithmetic](#rabin-karp-rolling-hash-with-mersenne-prime-arithmetic)
  - [Bit-Parallel LCS Algorithm](#bit-parallel-lcs-algorithm)
  - [Three-Tier Bitvector Dispatch](#three-tier-bitvector-dispatch)
  - [Length-Ratio Pre-Filter](#length-ratio-pre-filter)
  - [Fingerprint Frequency Capping](#fingerprint-frequency-capping)
  - [Multi-Threaded Similarity Computation](#multi-threaded-similarity-computation)
  - [Parallel Tokenization with stdexec](#parallel-tokenization-with-stdexec)
  - [SIMD-Accelerated Match Extension](#simd-accelerated-match-extension)
  - [Flat PM Table for O(1) Lookups](#flat-pm-table-for-o1-lookups)
  - [Token Memory Reclamation](#token-memory-reclamation)
- [Data Structures Reference](#data-structures-reference)
- [Source File Index](#source-file-index)

---

## Pipeline Overview

The tool processes source code through five sequential phases:

```
Source Directory
      |
      v
  Phase 1: File Scanning
      |  (file paths)
      v
  Phase 2: Tokenization  (parallel)
      |  (token vectors per file)
      v
  Phase 3: Normalization & Block Extraction
      |  (CodeBlock objects with normalized ID sequences)
      v
  Phase 4: Clone Detection
      |--- 4a: Inter-function detection (fingerprint -> LCS -> Union-Find)
      |--- 4b: Intra-function detection (self-join fingerprint -> SIMD extension)
      |--- 4c: Scope filtering / Diff filtering
      |  (CloneGroup / IntraCloneResult vectors)
      v
  Phase 5: Reporting
      |  (console text or JSON)
      v
   Output
```

The pipeline entry point is `main()` in `src/cli/main.cpp`. Each phase is
implemented by a dedicated helper function.

---

## Phase 1: File Scanning

**Entry point:** `ScanFiles()` in `src/cli/main.cpp`

The file scanner recursively walks the target directory and collects source files whose
extensions match a known language. The default extension set covers C++ (`.cpp`, `.hpp`,
`.cc`, `.cxx`, `.h`), C# (`.cs`), and Python (`.py`).

Optional filters are composed together:

- **Gitignore filter** -- Respects `.gitignore` rules by default (can be disabled
  with `--no-gitignore`).
- **Glob filter** -- The `-g` / `--glob` option restricts scanning to files matching
  one or more glob patterns (e.g., `-g '*.cpp' -g '*Controller*'`).

The scanner is implemented in `src/codedup/FileScanner.hpp`.

---

## Phase 2: Tokenization

**Entry point:** `TokenizeFiles()` in `src/cli/main.cpp`

Each source file is converted into a sequence of `Token` objects by a language-specific
tokenizer.

### Language Selection

The `LanguageRegistry` singleton (`src/codedup/LanguageRegistry.hpp`) maps file
extensions to `Language` implementations. Each language provides:

| Method | Purpose |
|---|---|
| `Tokenize(source, fileIndex)` | Tokenize from a string |
| `TokenizeFile(path, fileIndex, encoding)` | Tokenize from a file (with encoding detection) |
| `ExtractBlocks(tokens, normalized, textPreserving, config)` | Extract function-level code blocks |
| `ShouldStripToken(type)` | Decide which tokens to exclude during normalization |

### Token Structure

Defined as `struct Token` in `src/codedup/Token.hpp`:

```cpp
struct Token {
    TokenType type;        // Enum classifying the token (keyword, operator, literal, etc.)
    std::string text;      // Original source text
    SourceLocation location; // File index + line + column
};
```

The `TokenType` enum (same file) covers ~280 token types including shared keywords,
per-language keywords (C#, Python), all operators and punctuation, literals, comments,
and preprocessor directives.

### Source Locations

`SourceLocation` (`src/codedup/SourceLocation.hpp`) uses a compact `uint32_t`
file index rather than storing a full path per token. This index references into the
global file path vector maintained by the pipeline, significantly reducing per-token
memory overhead.

### Parallel Execution

Tokenization is parallelized across all CPU cores using
`stdexec::bulk(stdexec::par, numFiles, ...)` with an `exec::static_thread_pool`
inside `TokenizeFiles()`. Language resolution is done sequentially first (it is
lightweight), then all files are tokenized in parallel.

---

## Phase 3: Token Normalization and Block Extraction

**Entry point:** `ExtractBlocks()` in `src/cli/main.cpp`

### Normalization Modes

The `TokenNormalizer` class (`src/codedup/TokenNormalizer.hpp`) converts raw tokens
into integer ID sequences suitable for algorithmic comparison. It offers two modes:

#### Structural Normalization

`TokenNormalizer::Normalize()` in `src/codedup/TokenNormalizer.cpp`

This mode produces identical sequences for **Type-2 (renamed) clones** -- functions
whose structure is identical but whose variable names or literal values differ:

| Token Category | Normalized ID |
|---|---|
| Keywords/operators | Deterministic ID from the `TokenType` enum value |
| All identifiers | Generic ID `1000` |
| All numeric literals | Generic ID `1001` |
| All string literals | Generic ID `1002` |
| All char literals | Generic ID `1003` |
| Comments, preprocessor, EOF | **Stripped** (excluded from output) |

The generic IDs are defined in the `GenericId` enum (`src/codedup/TokenNormalizer.hpp`).

#### Text-Preserving Normalization

`TokenNormalizer::NormalizeTextPreserving()` in `src/codedup/TokenNormalizer.cpp`

This mode gives each unique identifier and literal text its own distinct ID (starting
at 2000), while keywords and operators keep the same IDs as structural mode. This
enables a textual similarity score that distinguishes renamed identifiers from truly
identical ones.

A `TokenDictionary` (`src/codedup/TokenNormalizer.hpp`) maintains the bijective
mapping between IDs and text strings. IDs below 2000 are reserved; dynamic IDs start
at 2000 and are allocated incrementally.

### Block Extraction

Each language implements `Language::ExtractBlocks()` to identify function-level code
units from the token stream. A `CodeBlock` (`src/codedup/CodeBlock.hpp`) captures:

```cpp
struct CodeBlock {
    std::string name;                                 // Function/method name
    SourceRange sourceRange;                          // Start/end lines in the source file
    size_t tokenStart, tokenEnd;                      // Token index range
    std::vector<NormalizedTokenId> normalizedIds;     // Structural ID sequence
    std::vector<NormalizedTokenId> textPreservingIds; // Text-aware ID sequence
};
```

Blocks smaller than `minTokens` (default: 30) are discarded. The minimum token count
is configurable via `CodeBlockExtractorConfig` (same file).

---

## Phase 4: Clone Detection

### Inter-Function Clone Detection

**Core class:** `CloneDetector` in `src/codedup/CloneDetector.hpp`\
**Implementation:** `CloneDetector::Detect()` in `src/codedup/CloneDetector.cpp`

The algorithm operates in three steps:

#### Step 1: Fingerprint-Based Candidate Generation

Implemented in the first half of `CloneDetector::Detect()`.

This step rapidly narrows the O(n^2) space of all block pairs to a small set of
candidates likely to be similar.

1. **Compute fingerprints.** For each block's normalized ID sequence, compute
   Rabin-Karp rolling hash fingerprints over a sliding window of size `hashWindowSize`
   (default: 10 tokens). Each window position produces one 64-bit fingerprint.

2. **Build an inverted index.** Map each fingerprint to the list of blocks containing
   it. Within a single block, each fingerprint is counted only once (deduplicated via
   a hash set).

3. **Filter over-common fingerprints.** Fingerprints appearing in more than
   `max(50, numBlocks/2)` blocks are skipped. These correspond to extremely common
   code patterns (e.g., `return 0;`) that would generate a combinatorial explosion of
   pairs without being informative.

4. **Count shared fingerprints.** For each pair of blocks sharing at least one
   fingerprint, count how many distinct fingerprints they share. Pairs with fewer than
   `minHashMatches` (default: 3) shared fingerprints are discarded.

5. **Apply length pre-filter.** Before computing the expensive LCS similarity, check
   whether the pair's lengths are compatible using `CloneDetector::LengthsCompatible()`:

   ```
   maxDice = 2 * min(lenA, lenB) / (lenA + lenB)
   ```

   If this upper bound is below the similarity threshold, the pair cannot possibly pass
   and is discarded immediately. This is an O(1) check.

#### Step 2: LCS-Based Similarity Computation

Implemented in the second half of `CloneDetector::Detect()`.

For each surviving candidate pair, compute the exact similarity:

1. **LCS length computation.** The Longest Common Subsequence (LCS) length between
   the two blocks' normalized ID sequences is computed using the bit-parallel
   Allison-Dill / Hyyro-Navarro algorithm (see
   [Bit-Parallel LCS Algorithm](#bit-parallel-lcs-algorithm)).

2. **Dice coefficient.** Similarity is calculated as:

   ```
   similarity = 2 * |LCS| / (|A| + |B|)
   ```

   This ranges from 0.0 (completely different) to 1.0 (identical). The Dice
   coefficient was chosen because it is symmetric and normalizes for sequence length.

3. **Text sensitivity blending.** When `textSensitivity > 0`,
   `CloneDetector::ComputeBlendedSimilarity()` computes both structural and textual
   similarity and blends them:

   ```
   finalSim = (1 - textSensitivity) * structuralSim + textSensitivity * textualSim
   ```

   At `textSensitivity = 0.0` (pure structural), two functions that differ only in
   variable names get similarity 1.0. At `textSensitivity = 1.0` (pure textual), those
   same functions would score lower because their identifiers have different text.

4. **Threshold filtering.** Pairs below `similarityThreshold` (default: 0.80) are
   discarded.

5. **Multi-threaded execution.** Candidate pairs are divided evenly across all
   available CPU cores (`std::thread::hardware_concurrency()`). Each thread writes to
   its own result vector; results are merged after all threads complete. For small
   workloads (<= 100 candidates), the computation runs single-threaded to avoid
   overhead.

#### Step 3: Grouping via Union-Find

Implemented in the final section of `CloneDetector::Detect()`.

Clone pairs are grouped into connected components using a `UnionFind` (disjoint set)
data structure (defined in `src/codedup/CloneDetector.cpp`) with union-by-rank and path
compression:

1. Initialize one element per block.
2. For each clone pair `(A, B)`, call `UnionFind::Unite(A, B)`.
3. Collect blocks by their root representative via `UnionFind::Find()`.
4. For each root with >= 2 blocks, emit a `CloneGroup` with:
   - `blockIndices` -- sorted list of block indices in the group.
   - `avgSimilarity` -- average pairwise similarity among the group's clone pairs.

Groups are sorted by size (largest first), then by similarity.

### Intra-Function Clone Detection

**Core class:** `IntraFunctionDetector` in `src/codedup/IntraFunctionDetector.hpp`\
**Implementation:** `IntraFunctionDetector::Detect()` in `src/codedup/IntraFunctionDetector.cpp`

This detector finds duplicated regions **within** a single function body. For each
code block (via `IntraFunctionDetector::DetectInBlock()`):

1. **Fingerprint self-join.**
   Compute rolling hash fingerprints over the block's normalized ID sequence. Build
   an inverted index mapping each fingerprint to the positions within the block where
   it occurs. Collect all position pairs `(i, j)` that share a fingerprint and whose
   initial windows do not overlap. Over-common fingerprints (appearing > 50 times) are
   skipped.

2. **SIMD-accelerated match extension.**
   For each candidate position pair, extend the match both forward and backward to
   find the maximal region of identical normalized tokens. Extension uses
   `SimdForwardMatch()` and `SimdBackwardMatch()` which compare `kSimdWidth`
   `uint32_t` values per iteration using `std::experimental::native_simd`
   (4 elements on SSE2, 8 on AVX2, 16 on AVX-512).

3. **Similarity scoring.**
   Since the extended regions are exact matches on structural IDs, structural
   similarity is 1.0 by construction. When text sensitivity is enabled, a positional
   text similarity is computed: for each position in the aligned regions, check whether
   the text-preserving IDs also match. This is also SIMD-accelerated
   via `PositionalTextSimilarity()`.

4. **Deduplication.**
   Candidate pairs are sorted by total region length (longest first). A pair is
   considered redundant (via `PairsAreRedundant()`) if both its A and B regions
   overlap by more than 50% with those of an already-kept pair. Only non-redundant
   pairs are emitted.

### Scope Filtering and Diff Filtering

After detection, results are filtered according to the user's requested scope:

- **`ScopeFilter`** (`src/codedup/ScopeFilter.hpp`) restricts inter-function results
  to cross-file clones, within-file clones, or both.
- **`DiffFilter`** (`src/codedup/DiffFilter.hpp`) restricts results to clones
  involving blocks that overlap with lines changed in a git diff. This enables
  CI pipelines to check only newly introduced duplication.

---

## Phase 5: Reporting

**Interface:** `Reporter` in `src/codedup/Reporter.hpp`

The reporter receives the detected groups and intra-function results and formats them
for output. Two concrete implementations exist:

- **`ConsoleReporter`** (`src/codedup/ConsoleReporter.hpp`) -- Human-readable text with
  optional ANSI color codes, syntax highlighting, source code snippets, and
  diff highlighting of mismatched tokens between clones.
- **`JsonReporter`** (`src/codedup/JsonReporter.hpp`) -- Structured JSON for CI tooling
  and machine consumption.

A `SummaryData` struct (`src/codedup/Reporter.hpp`) aggregates statistics
(total files, blocks, groups, duplicated lines) and optional `PerformanceTiming`
data (same file) that records wall-clock duration for each pipeline phase.

---

## Performance Engineering

This section describes the specific techniques used to achieve maximum throughput.

### Rabin-Karp Rolling Hash with Mersenne Prime Arithmetic

**File:** `src/codedup/RollingHash.hpp`

Fingerprints are computed using a polynomial rolling hash:

```
H(t_0, ..., t_{w-1}) = (t_0 * B^{w-1} + t_1 * B^{w-2} + ... + t_{w-1}) mod P
```

where `B = 257` is the hash base (`hashBase`) and `P = 2^61 - 1` is a Mersenne prime
(`hashPrime`), both defined in `src/codedup/RollingHash.hpp`.

The rolling update slides the window by one position in O(1):

```
H' = ((H - t_old * B^{w-1}) * B + t_new) mod P
```

**Mersenne prime optimization** (`Mulmod()`):
Instead of using a general 128-bit modulo operation (which can be expensive), the
implementation exploits the algebraic property of the Mersenne prime `P = 2^61 - 1`:

```
Since 2^61 ≡ 1 (mod P):
  a * b mod P = lo + hi
  where lo = (a * b) & P      (lower 61 bits)
        hi = (a * b) >> 61    (upper bits)
```

The 128-bit product is split with a shift and a mask, followed by a single addition
and a conditional subtraction. This replaces what would otherwise be a library call
for 128-bit division on some platforms with three cheap operations.

### Bit-Parallel LCS Algorithm

**File:** `src/codedup/CloneDetector.cpp`\
**Functions:** `LcsLengthBitParallel64()`, `LcsLengthBitParallel256()`, `LcsLengthBitParallelDynamic()`

The Longest Common Subsequence is the most computationally expensive operation in the
pipeline. The classic dynamic programming algorithm runs in O(m * n) time, where m and
n are the sequence lengths. CodeDupDetector uses the Allison-Dill / Hyyro-Navarro
bit-parallel algorithm, reducing this to **O(m * ceil(n / w))** where `w` is the
machine word size (64 bits).

**Core idea:** Encode an entire column of the DP table as a bitvector `M`, where bit
`j` is set if and only if `DP[i][j] > DP[i][j-1]` (i.e., the LCS length increased at
column j). Each row of the DP table is updated using 5 bitwise operations:

```
For each element a[i]:
  1. PM[a[i]]     -- precomputed bitvector where bit j is set iff b[j] == a[i]
  2. X = M | PM[a[i]]
  3. shifted = (M << 1) | 1
  4. subtracted = X - shifted
  5. M' = X & (subtracted ^ X)
```

After processing all of sequence A, `LCS_length = popcount(M)`.

The subtraction in step 4 is the key insight: it propagates carries through
consecutive set bits, implementing the DP recurrence `max(DP[i-1][j], DP[i][j-1])`
in parallel across all columns simultaneously.

### Three-Tier Bitvector Dispatch

**Function:** `CloneDetector::ComputeSimilarity()` in `src/codedup/CloneDetector.cpp`

The bit-parallel LCS is implemented at three width tiers, selected based on the
shorter sequence's length:

| Tier | Sequence Length | Bitvector Type | Words per Update |
|---|---|---|---|
| **Tier 1** | 1-64 tokens | `uint64_t` | 1 |
| **Tier 2** | 65-256 tokens | `BitVector256` (4 x `uint64_t`) | 4 |
| **Tier 3** | > 256 tokens | `DynamicBitVector` (W x `uint64_t`) | W = ceil(n/64) |

**Tier 1** (`LcsLengthBitParallel64()`) handles most functions since typical function
bodies contain 30-64 normalized tokens after stripping comments. The entire computation
uses a single 64-bit register and 5 native bitwise operations per row -- the inner loop
compiles down to ~5 machine instructions.

**Tier 2** (`LcsLengthBitParallel256()`) uses `BitVector256`, a fixed 256-bit type
implemented as `std::array<uint64_t, 4>`. It provides:

- Bitwise OR, AND, XOR as component-wise operations on 4 words.
- Subtraction with borrow propagation across words (`operator-()`).
- Shift-left-by-1 with carry propagation (`ShiftLeft1()`).
- Population count across all words (`Popcount()`).

**Tier 3** (`LcsLengthBitParallelDynamic()`) uses `DynamicBitVector`, which allocates
`W = ceil(n/64)` words dynamically. The carry and borrow propagation are inlined into
the per-row loop.

`ComputeSimilarity()` always places the **shorter** sequence into the bitvectors and
iterates over the longer sequence row-by-row, ensuring the narrowest possible tier is
used.

**Concrete speedup example:** For two 200-token sequences, classic DP performs 40,000
comparisons. The bit-parallel Tier 2 algorithm performs 200 iterations x 4 word
operations = 800 word ops, each processing 64 columns simultaneously -- a theoretical
**50x reduction** in work.

### Length-Ratio Pre-Filter

**Function:** `CloneDetector::LengthsCompatible()` in `src/codedup/CloneDetector.hpp`

Before computing the expensive LCS, an O(1) check determines whether the pair's
sequence lengths are compatible with the similarity threshold:

```
maxDice = 2 * min(lenA, lenB) / (lenA + lenB)
```

Since `|LCS| <= min(lenA, lenB)`, this is an upper bound on the Dice coefficient.
If `maxDice < threshold`, the pair is immediately discarded. For example, with a
threshold of 0.80, a 100-token block and a 30-token block have
`maxDice = 2*30/130 = 0.46`, so the pair is skipped without any LCS computation.

### Fingerprint Frequency Capping

Inside `CloneDetector::Detect()`, fingerprints appearing in more than
`max(50, numBlocks/2)` blocks are skipped during candidate pair generation. Common
fingerprints correspond to boilerplate patterns (e.g., `if (...) return ...;`) that
appear in many functions. Including them would create O(k^2) pairs for k blocks --
potentially millions of spurious candidates -- without contributing meaningful
duplicate detection. The cap reduces candidate generation from potential O(n^2) to
near-linear in practice.

### Multi-Threaded Similarity Computation

Inside `CloneDetector::Detect()`, Phase 2 (LCS similarity) is the computational
bottleneck. It is parallelized across all available CPU cores:

1. Candidate pairs are divided into equal-sized chunks (one per thread).
2. Each thread writes results to its own `std::vector<ClonePair>`, avoiding
   synchronization on the hot path.
3. An `std::atomic<size_t>` counter tracks progress for the progress callback.
4. Workers are launched as `std::jthread` instances; the main thread processes
   chunk 0.
5. After all threads complete (via automatic `jthread` join), per-thread results
   are merged with move iterators.

For small workloads (<= 100 candidates), single-threaded execution is used to avoid
thread creation overhead.

### Parallel Tokenization with stdexec

Inside `TokenizeFiles()` in `src/cli/main.cpp`, file tokenization is parallelized
using the P2300 `stdexec` execution framework with a static thread pool sized to
`std::thread::hardware_concurrency()`:

```cpp
exec::static_thread_pool pool(std::thread::hardware_concurrency());
auto sched = pool.get_scheduler();
auto work = stdexec::starts_on(sched,
    stdexec::just() | stdexec::bulk(stdexec::par, numFiles, [&](size_t fi) {
        // tokenize file fi
    }));
stdexec::sync_wait(work);
```

Language resolution is done sequentially first (it is a lightweight registry lookup),
and then all files are tokenized concurrently. Each file writes to its pre-allocated
slot in the output vectors, so no synchronization is needed.

### SIMD-Accelerated Match Extension

**File:** `src/codedup/IntraFunctionDetector.cpp`\
**Functions:** `SimdForwardMatch()`, `SimdBackwardMatch()`, `PositionalTextSimilarity()`

Intra-function clone detection extends candidate matches to their maximal length by
comparing consecutive token IDs. This comparison is accelerated using
`std::experimental::native_simd<uint32_t>`, which auto-selects the widest SIMD
register available on the target architecture:

| Architecture | Elements per Iteration |
|---|---|
| SSE2 | 4 |
| AVX2 | 8 |
| AVX-512 | 16 |

**Forward extension** (`SimdForwardMatch()`): Loads `kSimdWidth`
`NormalizedTokenId` values from both regions, compares them with SIMD `!=`, and checks
for any mismatch. On mismatch, `find_first_set()` identifies the exact lane. A scalar
tail loop handles the remainder when `maxLen` is not divisible by `kSimdWidth`.

**Backward extension** (`SimdBackwardMatch()`): Scans backward from the candidate
positions using the same SIMD approach, loading elements in reverse order.

**Positional text similarity** (`PositionalTextSimilarity()`): Uses SIMD to count
matching text-preserving IDs across aligned regions, with `popcount()` on the
comparison mask.

When compiled with `-march=native`, the compiler selects the widest available register
width. The `--info` CLI flag reports the compiled SIMD vector width.

### Flat PM Table for O(1) Lookups

Inside `LcsLengthBitParallel64()`, `LcsLengthBitParallel256()`, and
`LcsLengthBitParallelDynamic()`, the bit-parallel LCS algorithm requires a
**pattern match table** `PM[c]` -- a bitvector indicating all positions in sequence B
where character `c` occurs. A naive implementation using a hash map would add
per-lookup overhead in the inner loop.

Instead, the implementation scans sequence B to find the maximum token ID
(`FindMaxId()`), then allocates a flat vector of size `maxId + 1`. This provides
direct O(1) indexing by token ID:

```cpp
auto const maxId = FindMaxId(b);
std::vector<uint64_t> pm(static_cast<size_t>(maxId) + 1, 0);
for (auto const j : std::views::iota(size_t{0}, n))
    pm[b[j]] |= (uint64_t{1} << j);
```

For normalized token IDs (which are compact integers in the range 0-~2000), this
allocation is small and cache-friendly. The flat table eliminates all hash overhead
from the LCS inner loop.

### Token Memory Reclamation

In `main()` (`src/cli/main.cpp`), after detection completes, the pipeline identifies
which files participate in at least one clone group or intra-function result. Token
vectors for non-participating files are cleared and deallocated (`clear()` +
`shrink_to_fit()`). This reduces peak memory footprint before the reporting phase,
which needs to read source tokens only for participating files.

---

## Data Structures Reference

| Struct / Class | File | Description |
|---|---|---|
| `Token` | `src/codedup/Token.hpp` | A single lexical token (type + text + location) |
| `TokenType` | `src/codedup/Token.hpp` | Enum of ~280 token types across all languages |
| `SourceLocation` | `src/codedup/SourceLocation.hpp` | Compact file:line:column using uint32 file index |
| `SourceRange` | `src/codedup/SourceLocation.hpp` | Start-end pair of `SourceLocation` |
| `NormalizedTokenId` | `src/codedup/TokenNormalizer.hpp` | `uint32_t` alias for normalized IDs |
| `NormalizedToken` | `src/codedup/TokenNormalizer.hpp` | Normalized ID + index into original token vector |
| `TokenDictionary` | `src/codedup/TokenNormalizer.hpp` | Bijective ID-to-name mapping |
| `TokenNormalizer` | `src/codedup/TokenNormalizer.hpp` | Structural and text-preserving normalization |
| `CodeBlock` | `src/codedup/CodeBlock.hpp` | Extracted function body with normalized ID sequences |
| `CodeBlockExtractorConfig` | `src/codedup/CodeBlock.hpp` | Minimum token threshold for blocks |
| `Language` | `src/codedup/Language.hpp` | Abstract base for language tokenizer + extractor |
| `LanguageRegistry` | `src/codedup/LanguageRegistry.hpp` | Singleton mapping extensions to languages |
| `CloneDetectorConfig` | `src/codedup/CloneDetector.hpp` | Detection parameters (threshold, window, etc.) |
| `ClonePair` | `src/codedup/CloneDetector.hpp` | A pair of blocks with similarity score |
| `CloneGroup` | `src/codedup/CloneDetector.hpp` | Connected component of clone blocks |
| `CloneDetector` | `src/codedup/CloneDetector.hpp` | Inter-function detection engine |
| `LcsAlignment` | `src/codedup/CloneDetector.hpp` | Per-position LCS match flags (for reporting) |
| `BitVector256` | `src/codedup/CloneDetector.cpp` | Fixed 256-bit bitvector for Tier 2 LCS |
| `DynamicBitVector` | `src/codedup/CloneDetector.cpp` | Variable-width bitvector for Tier 3 LCS |
| `UnionFind` | `src/codedup/CloneDetector.cpp` | Disjoint-set for grouping clone pairs |
| `IntraCloneRegion` | `src/codedup/IntraFunctionDetector.hpp` | Start + length within a block |
| `IntraClonePair` | `src/codedup/IntraFunctionDetector.hpp` | Two duplicated regions within one block |
| `IntraCloneResult` | `src/codedup/IntraFunctionDetector.hpp` | All intra-function pairs for one block |
| `IntraFunctionDetector` | `src/codedup/IntraFunctionDetector.hpp` | Intra-function detection engine |
| `ScopeFilter` | `src/codedup/ScopeFilter.hpp` | Post-detection inter/intra-file filtering |
| `DiffFilter` | `src/codedup/DiffFilter.hpp` | Post-detection git-diff-based filtering |
| `AnalysisScope` | `src/codedup/AnalysisScope.hpp` | Bitmask enum for scope selection |
| `Reporter` | `src/codedup/Reporter.hpp` | Abstract output formatting interface |
| `PerformanceTiming` | `src/codedup/Reporter.hpp` | Wall-clock timing per pipeline phase |
| `SummaryData` | `src/codedup/Reporter.hpp` | Aggregate statistics for the summary |

---

## Source File Index

| File | Purpose |
|---|---|
| `src/cli/main.cpp` | CLI entry point and pipeline orchestration |
| `src/codedup/CloneDetector.hpp` | Inter-function detection API |
| `src/codedup/CloneDetector.cpp` | Fingerprinting, bit-parallel LCS, Union-Find grouping |
| `src/codedup/IntraFunctionDetector.hpp` | Intra-function detection API |
| `src/codedup/IntraFunctionDetector.cpp` | Self-join, SIMD extension, deduplication |
| `src/codedup/RollingHash.hpp` | Rabin-Karp rolling hash with Mersenne prime |
| `src/codedup/Token.hpp` | Token type enum and Token struct |
| `src/codedup/TokenNormalizer.hpp` | Normalizer API and dictionary types |
| `src/codedup/TokenNormalizer.cpp` | Structural and text-preserving normalization logic |
| `src/codedup/CodeBlock.hpp` | CodeBlock struct and extractor config |
| `src/codedup/Language.hpp` | Abstract language interface |
| `src/codedup/LanguageRegistry.hpp` | Extension-to-language registry |
| `src/codedup/SourceLocation.hpp` | Compact source location types |
| `src/codedup/AnalysisScope.hpp` | Scope bitmask enum and parsing |
| `src/codedup/ScopeFilter.hpp` | Scope-based group filtering |
| `src/codedup/DiffFilter.hpp` | Git-diff-based result filtering |
| `src/codedup/Reporter.hpp` | Reporter interface and summary types |
| `src/codedup/ConsoleReporter.hpp` | Console output with syntax highlighting |
| `src/codedup/JsonReporter.hpp` | JSON structured output |
| `src/codedup/FileScanner.hpp` | Directory scanning for source files |
