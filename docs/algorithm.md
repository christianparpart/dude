# Code Duplication Detection Algorithm

This document describes how dude identifies duplicated code across and within
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
    - [Step 2: Multi-Stage Candidate Filtering](#step-2-multi-stage-candidate-filtering)
    - [Step 3: LCS-Based Similarity Computation](#step-3-lcs-based-similarity-computation)
    - [Step 4: Grouping via Union-Find](#step-4-grouping-via-union-find)
  - [Intra-Function Clone Detection](#intra-function-clone-detection)
  - [Scope Filtering and Diff Filtering](#scope-filtering-and-diff-filtering)
- [Phase 5: Reporting](#phase-5-reporting)
- [Mathematical Foundations](#mathematical-foundations)
  - [Dice Coefficient as Similarity Metric](#dice-coefficient-as-similarity-metric)
  - [Longest Common Subsequence (LCS)](#longest-common-subsequence-lcs)
  - [Rabin-Karp Polynomial Hashing](#rabin-karp-polynomial-hashing)
  - [Mersenne Prime Modular Arithmetic](#mersenne-prime-modular-arithmetic)
  - [Upper Bound Derivations for Pre-Filters](#upper-bound-derivations-for-pre-filters)
  - [Blended Similarity Model](#blended-similarity-model)
- [Performance Engineering](#performance-engineering)
  - [Rabin-Karp Rolling Hash with Mersenne Prime Arithmetic](#rabin-karp-rolling-hash-with-mersenne-prime-arithmetic)
  - [Bit-Parallel LCS Algorithm](#bit-parallel-lcs-algorithm)
  - [Three-Tier Bitvector Dispatch](#three-tier-bitvector-dispatch)
  - [Threshold-Aware LCS with Early Termination](#threshold-aware-lcs-with-early-termination)
  - [Five-Stage Filter Cascade](#five-stage-filter-cascade)
  - [Adaptive Fingerprint Threshold](#adaptive-fingerprint-threshold)
  - [Bag-of-Tokens Dice Pre-Filter](#bag-of-tokens-dice-pre-filter)
  - [Length-Ratio Pre-Filter](#length-ratio-pre-filter)
  - [Fingerprint Frequency Capping](#fingerprint-frequency-capping)
  - [Partitioned MapReduce Candidate Gathering](#partitioned-mapreduce-candidate-gathering)
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
  Phase 2: Tokenization  (parallel via stdexec)
      |  (token vectors per file)
      v
  Phase 3: Normalization & Block Extraction
      |  (CodeBlock objects with normalized ID sequences)
      v
  Phase 4: Clone Detection
      |--- 4a: Inter-function detection
      |         Fingerprinting (parallel)
      |              |
      |         Candidate gathering (partitioned MapReduce)
      |              |
      |         Five-stage filter cascade:
      |           1. Static minHashMatches
      |           2. Adaptive minHashMatches
      |           3. Length-ratio pre-filter         O(1)
      |           4. Bag-of-tokens Dice pre-filter   O(V)
      |           5. Threshold-aware bit-parallel LCS O(m * ceil(n/64))
      |              |
      |         Union-Find grouping
      |
      |--- 4b: Intra-function detection (self-join fingerprint -> SIMD extension)
      |--- 4c: Scope filtering / Diff filtering
      |--- 4d: Token memory reclamation
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

By collapsing all identifiers to a single ID and all literals of each kind to a single
ID, two code fragments that differ only in variable names and literal values produce
identical normalized sequences, enabling Type-2 clone detection.

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

The algorithm operates in four steps:

#### Step 1: Fingerprint-Based Candidate Generation

Implemented in the first section of `CloneDetector::Detect()`.

This step rapidly narrows the O(N^2) space of all block pairs to a small set of
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

4. **Precompute block histograms.** A `BlockHistogram` is built for every block: a
   flat array indexed by `NormalizedTokenId` where `counts[t]` holds the number of
   occurrences of token ID `t` in that block. The array is sized to
   `globalMaxId + 1`, where `globalMaxId` is the largest token ID across all blocks.
   These histograms enable the O(V) bag-of-tokens Dice pre-filter in Step 2.

5. **Gather candidate pairs (parallel MapReduce).** Shared fingerprint counts between
   block pairs are tallied using a partitioned MapReduce scheme (see
   [Partitioned MapReduce Candidate Gathering](#partitioned-mapreduce-candidate-gathering)).
   Multiple worker threads process fingerprints in a striped (interleaved) pattern,
   each writing to partitioned maps. Partitions are then merged independently in
   parallel.

#### Step 2: Multi-Stage Candidate Filtering

After candidate gathering, each candidate pair passes through a **five-stage filter
cascade**. The stages are ordered from cheapest to most expensive, so that most
non-matching pairs are eliminated before reaching the costly LCS computation:

```
Candidate pair (blockA, blockB, sharedFingerprints)
      |
      v
  Stage 1: Static minHashMatches           O(1)   -- constant threshold
      |
      v
  Stage 2: Adaptive minHashMatches         O(1)   -- threshold-dependent bound
      |
      v
  Stage 3: Length-ratio pre-filter         O(1)   -- geometric upper bound
      |
      v
  Stage 4: Bag-of-tokens Dice pre-filter   O(V)   -- multiset upper bound
      |
      v
  Stage 5: Threshold-aware LCS             O(m * ceil(n/64))  -- exact with early exit
      |
      v
  Accepted ClonePair
```

**Stage 1 -- Static minHashMatches.** Reject the pair if the number of shared
fingerprints is less than `minHashMatches` (default: 3). This is the original baseline
filter.

**Stage 2 -- Adaptive minHashMatches.** For high similarity thresholds (e.g., 0.97),
dynamically raise the minimum shared fingerprint count. The derivation is based on
the observation that each changed token can disrupt up to `hashWindowSize` consecutive
rolling-hash windows (see [Adaptive Fingerprint Threshold](#adaptive-fingerprint-threshold)).

**Stage 3 -- Length-ratio pre-filter.** An O(1) check based on the mathematical
upper bound of the Dice coefficient given only the sequence lengths (see
[Length-Ratio Pre-Filter](#length-ratio-pre-filter)).

**Stage 4 -- Bag-of-tokens Dice pre-filter.** An O(V) check computing the multiset
intersection of token histograms to upper-bound the LCS-based Dice coefficient (see
[Bag-of-Tokens Dice Pre-Filter](#bag-of-tokens-dice-pre-filter)).

**Stage 5 -- Threshold-aware bit-parallel LCS.** The exact LCS-based similarity is
computed using threshold-aware variants of the bit-parallel algorithm that can
terminate early when the threshold becomes unreachable (see
[Threshold-Aware LCS with Early Termination](#threshold-aware-lcs-with-early-termination)).

#### Step 3: LCS-Based Similarity Computation

Implemented in the parallel similarity computation section of `CloneDetector::Detect()`.

For each candidate pair surviving Stages 1-4, the exact similarity is computed:

1. **LCS length computation.** The Longest Common Subsequence (LCS) length between
   the two blocks' normalized ID sequences is computed using the bit-parallel
   Allison-Dill / Hyyro-Navarro algorithm (see
   [Bit-Parallel LCS Algorithm](#bit-parallel-lcs-algorithm)).

2. **Dice coefficient.** Similarity is calculated as:

   ```
   similarity = 2 * |LCS(A, B)| / (|A| + |B|)
   ```

   This ranges from 0.0 (completely different) to 1.0 (identical). See
   [Dice Coefficient as Similarity Metric](#dice-coefficient-as-similarity-metric) for
   the mathematical properties motivating this choice.

3. **Text sensitivity blending.** When `textSensitivity > 0`,
   `CloneDetector::ComputeBlendedSimilarityWithThreshold()` computes both structural
   and textual similarity and blends them:

   ```
   finalSim = (1 - textSensitivity) * structuralSim + textSensitivity * textualSim
   ```

   The threshold-aware variant adds an additional optimization: if the structural
   similarity alone (weighted by `1 - textSensitivity`) plus the best-case textual
   contribution (assuming `textualSim = 1.0`) cannot reach the threshold, the textual
   LCS computation is skipped entirely.

4. **Threshold filtering.** Pairs below `similarityThreshold` (default: 0.80) are
   discarded.

5. **Multi-threaded execution.** Candidate pairs are divided evenly across all
   available CPU cores. Each thread writes to its own result vector; results are merged
   after all threads complete (see
   [Multi-Threaded Similarity Computation](#multi-threaded-similarity-computation)).

#### Step 4: Grouping via Union-Find

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

The Union-Find data structure achieves nearly O(1) amortized time per operation via
two standard optimizations:

- **Path compression** in `Find()`: Flattens the tree so all nodes point directly to
  the root, speeding up future queries.
- **Union by rank** in `Unite()`: Always attaches the shorter tree under the taller
  one, keeping the tree depth bounded by O(alpha(n)) where alpha is the inverse
  Ackermann function (effectively constant for all practical inputs).

### Intra-Function Clone Detection

**Core class:** `IntraFunctionDetector` in `src/codedup/IntraFunctionDetector.hpp`\
**Implementation:** `IntraFunctionDetector::Detect()` in `src/codedup/IntraFunctionDetector.cpp`

This detector finds duplicated regions **within** a single function body. The algorithm
is conceptually similar to the seed-and-extend paradigm used in bioinformatics sequence
alignment (e.g., BLAST): short exact-match seeds are identified via fingerprinting,
then extended to maximal length using SIMD-accelerated comparison.

For each code block (via `IntraFunctionDetector::DetectInBlock()`):

1. **Fingerprint self-join.**
   Compute rolling hash fingerprints over the block's normalized ID sequence. Build
   an inverted index (using `ankerl::unordered_dense::map` for cache-friendly open
   addressing) mapping each fingerprint to the positions within the block where it
   occurs. Collect all position pairs `(i, j)` that share a fingerprint and whose
   initial windows do not overlap (`j - i >= hashWindowSize`). Over-common fingerprints
   (appearing > 50 times) are skipped.

   Early exit: blocks with fewer than `2 * minRegionTokens` tokens are skipped since
   they cannot contain two non-overlapping regions of sufficient size.

2. **SIMD-accelerated match extension.**
   For each candidate position pair, extend the match both forward and backward to
   find the maximal region of identical normalized tokens. Extension uses
   `ForwardMatch()` and `BackwardMatch()` which compare `kSimdWidth`
   `uint32_t` values per iteration using `std::experimental::native_simd`
   (4 elements on SSE2, 8 on AVX2, 16 on AVX-512).

3. **Similarity scoring.**
   Since the extended regions are exact matches on structural IDs, structural
   similarity is 1.0 by construction. When text sensitivity is enabled, a positional
   text similarity is computed: for each position in the aligned regions, check whether
   the text-preserving IDs also match. This is also SIMD-accelerated
   via `PositionalTextSimilarity()`:

   ```
   textSim = |{k : textIdsA[k] == textIdsB[k]}| / regionLength
   ```

   The final similarity is blended:

   ```
   similarity = (1 - textSensitivity) * 1.0 + textSensitivity * textSim
   ```

4. **Deduplication.**
   Candidate pairs are sorted by total region length (longest first), then by
   similarity (highest first). A pair is considered redundant
   (via `PairsAreRedundant()`) if both its A and B regions overlap by more than 50%
   with those of an already-kept pair:

   ```
   redundant iff overlap(regionA_p, regionA_q) * 2 > min(|regionA_p|, |regionA_q|)
          AND overlap(regionB_p, regionB_q) * 2 > min(|regionB_p|, |regionB_q|)
   ```

   where interval overlap is computed as:

   ```
   overlap([s1, s1+l1), [s2, s2+l2)) = max(0, min(s1+l1, s2+l2) - max(s1, s2))
   ```

   Only non-redundant pairs are emitted. The greedy longest-first ordering ensures
   that shorter, subsumed duplications are suppressed in favor of their maximal
   enclosing match.

### Scope Filtering and Diff Filtering

After detection, results are filtered according to the user's requested scope:

- **`ScopeFilter`** (`src/codedup/ScopeFilter.hpp`) restricts inter-function results
  to cross-file clones, within-file clones, or both. For `IntraFile` mode, it splits
  multi-file groups into per-file sub-groups containing 2+ blocks each.
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

## Mathematical Foundations

This section develops the mathematical theory behind the algorithms. Understanding
these foundations clarifies why each pre-filter is sound (never produces false
negatives) and how the optimizations preserve correctness.

### Dice Coefficient as Similarity Metric

The **Dice coefficient** (also known as the Sorensen-Dice index) measures similarity
between two sets or, in this context, two sequences via their LCS:

```
Dice(A, B) = 2 * |LCS(A, B)| / (|A| + |B|)
```

**Properties:**

- **Symmetry:** `Dice(A, B) = Dice(B, A)` -- the formula is symmetric in `|A|` and
  `|B|`.
- **Bounds:** `0 <= Dice(A, B) <= 1`. The lower bound is achieved when the sequences
  share no common subsequence; the upper bound when they are identical
  (`|LCS| = |A| = |B|`).
- **Length normalization:** Unlike raw LCS length, the Dice coefficient accounts for
  differing sequence lengths. Two sequences of length 100 sharing 80 tokens score
  `2*80/200 = 0.80`, while a 100-token and 50-token sequence sharing 50 tokens score
  `2*50/150 = 0.67`, correctly reflecting the asymmetry.
- **Relationship to Jaccard:** For sets, `Dice = 2J/(1+J)` where `J` is the Jaccard
  index. The Dice coefficient gives more weight to shared elements.

### Longest Common Subsequence (LCS)

The **Longest Common Subsequence** of two sequences `A = (a_1, ..., a_m)` and
`B = (b_1, ..., b_n)` is the longest sequence that appears as a subsequence of both
(preserving order but not necessarily contiguity).

**Recurrence (classic DP):**

```
DP[0][j] = 0                                   for all j
DP[i][0] = 0                                   for all i
DP[i][j] = DP[i-1][j-1] + 1                    if a[i] == b[j]
DP[i][j] = max(DP[i-1][j], DP[i][j-1])         otherwise
```

**Complexity:** O(m*n) time, O(m*n) space (or O(n) space with two-row optimization).

**Bit-parallel reformulation:** The DP table can be encoded column-wise into
bitvectors, reducing the time complexity to O(m * ceil(n/w)) where w = 64 is the
machine word size. See [Bit-Parallel LCS Algorithm](#bit-parallel-lcs-algorithm) for
the full derivation.

**Key bound used for pre-filtering:**

```
|LCS(A, B)| <= min(|A|, |B|)
```

This is because a subsequence of both A and B cannot be longer than the shorter
sequence. This bound enables the O(1) length-ratio pre-filter.

### Rabin-Karp Polynomial Hashing

A **Rabin-Karp hash** represents a sequence window as a polynomial evaluated modulo a
prime:

```
H(t_0, ..., t_{W-1}) = (sum_{k=0}^{W-1} t_k * B^{W-1-k}) mod P
```

where `B = 257` is the hash base and `P` is the modulus.

**Rolling update.** When the window slides from position `i` to `i+1`, the hash is
updated in O(1):

```
H_{i+1} = ((H_i - t_i * B^{W-1}) * B + t_{i+W}) mod P
```

This removes the contribution of the outgoing element `t_i` and incorporates the
incoming element `t_{i+W}`. The factor `B^{W-1} mod P` is precomputed once.

**Choice of base:** `B = 257` exceeds the byte value range (0-255), ensuring distinct
single-token inputs hash to distinct values. For normalized token IDs in the range
0-2000, this provides good distribution.

### Mersenne Prime Modular Arithmetic

The hash modulus is the **Mersenne prime** `P = 2^61 - 1 = 2305843009213693951`.

**Key algebraic property:** Since `2^61 = 1 (mod P)`:

```
(a * b) mod P = lo + hi
where lo = (a * b) & P      (lower 61 bits of the 128-bit product)
      hi = (a * b) >> 61    (upper bits, which represent multiples of 2^61 ≡ 1)
```

**Proof sketch:** Write the 128-bit product as `a * b = hi * 2^61 + lo`. Then
`a * b mod P = hi * 2^61 + lo mod P = hi * 1 + lo mod P = hi + lo mod P`. Since
`hi < 2^67` and `lo < 2^61`, their sum is at most `2P`, so a single conditional
subtraction produces the final result.

This replaces a general 128-bit modulo (expensive division) with:
1. One widened multiply (128-bit)
2. One 61-bit mask
3. One 61-bit right shift
4. One addition
5. One conditional subtraction

### Upper Bound Derivations for Pre-Filters

The filter cascade relies on a chain of progressively tighter upper bounds on the Dice
coefficient. Each bound is **sound**: if a pair fails the bound, it provably cannot
meet the similarity threshold.

**Length-ratio bound.** From `|LCS| <= min(|A|, |B|)`:

```
Dice(A, B) = 2|LCS|/(|A|+|B|) <= 2*min(|A|,|B|)/(|A|+|B|) = maxDice_length
```

This is O(1) to evaluate.

**Bag-of-tokens bound.** Let `count_A(t)` and `count_B(t)` denote the number of
occurrences of token ID `t` in sequences A and B respectively. The **multiset
intersection** is:

```
bagIntersection(A, B) = sum_t min(count_A(t), count_B(t))
```

**Claim:** `|LCS(A, B)| <= bagIntersection(A, B)`.

**Proof:** The LCS is a subsequence of both A and B. For each token ID `t`, the LCS
can contain at most `min(count_A(t), count_B(t))` copies of `t` (since it must be a
subsequence of both). Summing over all token IDs gives the bound.

Therefore:

```
Dice(A, B) <= 2 * bagIntersection(A, B) / (|A| + |B|) = bagDice
```

This is O(V) to evaluate where V is the vocabulary size (~1003 for structural
normalization).

**Bound ordering:** `Dice <= bagDice <= maxDice_length`, so each successive filter is
at least as tight as the previous one.

### Blended Similarity Model

When `textSensitivity > 0`, the final similarity is a convex combination:

```
finalSim = (1 - ts) * structuralSim + ts * textualSim
```

where `ts = textSensitivity in [0, 1]`.

**Properties:**

- At `ts = 0` (pure structural): Two functions differing only in variable names score
  1.0, enabling Type-2 clone detection.
- At `ts = 1` (pure textual): Similarity depends entirely on whether the actual
  identifier and literal texts match.
- The blended score is bounded: `min(structSim, textSim) <= finalSim <= max(structSim, textSim)`.
- Since `textualSim <= structuralSim` always holds (text-preserving normalization is
  finer-grained), the blended score decreases monotonically with `ts`.

**Threshold-aware optimization:** If the structural similarity `s` is known and
`textualSim` is not yet computed, the best-case blended score is:

```
bestCase = (1 - ts) * s + ts * 1.0
```

If `bestCase < threshold`, the textual LCS computation is skipped.

---

## Performance Engineering

This section describes the specific techniques used to achieve maximum throughput.

### Rabin-Karp Rolling Hash with Mersenne Prime Arithmetic

**File:** `src/codedup/RollingHash.hpp`

Fingerprints are computed using a polynomial rolling hash:

```
H(t_0, ..., t_{W-1}) = (t_0 * B^{W-1} + t_1 * B^{W-2} + ... + t_{W-1}) mod P
```

where `B = 257` is the hash base (`hashBase`) and `P = 2^61 - 1` is a Mersenne prime
(`hashPrime`), both defined in `src/codedup/RollingHash.hpp`.

The rolling update slides the window by one position in O(1):

```
H' = ((H - t_old * B^{W-1}) * B + t_new) mod P
```

The subtraction uses `hash + P - remove` to avoid unsigned underflow (since all values
are less than `P`, adding `P` first guarantees a non-negative intermediate).

**Mersenne prime optimization** (`Mulmod()`):
Instead of using a general 128-bit modulo operation (which can be expensive), the
implementation exploits the algebraic property of the Mersenne prime `P = 2^61 - 1`
(see [Mersenne Prime Modular Arithmetic](#mersenne-prime-modular-arithmetic)):

```
Since 2^61 ≡ 1 (mod P):
  a * b mod P = lo + hi
  where lo = (a * b) & P      (lower 61 bits)
        hi = (a * b) >> 61    (upper bits)
```

The 128-bit product is split with a shift and a mask, followed by a single addition
and a conditional subtraction. This replaces what would otherwise be a library call
for 128-bit division on some platforms with three cheap operations.

Platform-specific 128-bit multiplication is used: `_umul128` intrinsic on MSVC,
`__int128` native type on GCC/Clang.

### Bit-Parallel LCS Algorithm

**File:** `src/codedup/CloneDetector.cpp`\
**Functions:** `LcsLengthBitParallel64()`, `LcsLengthBitParallel256()`, `LcsLengthBitParallelDynamic()`

The Longest Common Subsequence is the most computationally expensive operation in the
pipeline. The classic dynamic programming algorithm runs in O(m * n) time, where m and
n are the sequence lengths. dude uses the Allison-Dill / Hyyro-Navarro
bit-parallel algorithm, reducing this to **O(m * ceil(n / w))** where `w` is the
machine word size (64 bits).

> Reference: Hyyro, H. & Navarro, G. (2004). "A Practical O(m * log(sigma) * ceil(n/w))
> Time Bit-Parallel Algorithm for Computing the LCS."

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

**Why this works:** The bitvector `M` encodes the "difference" representation of a DP
column: bit `j` is set when the LCS length increments at position `j`. The OR in
step 2 marks all candidate positions (either previously matched or newly matching).
The subtraction in step 4 propagates carries through consecutive set bits -- this is
the key insight, as carry propagation implements the `max(DP[i-1][j], DP[i][j-1])`
recurrence across all columns simultaneously. The XOR and AND in step 5 isolate
exactly the positions where a new match is committed.

**Complexity per row:** 5 bitwise operations on w-bit words, where w is the bitvector
width. For sequences of length n, this requires ceil(n/64) words per operation, giving
O(ceil(n/64)) work per row and O(m * ceil(n/64)) total.

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

### Threshold-Aware LCS with Early Termination

**File:** `src/codedup/CloneDetector.cpp`\
**Functions:** `LcsLengthBitParallel64WithThreshold()`, `LcsLengthBitParallel256WithThreshold()`, `LcsLengthBitParallelDynamicWithThreshold()`\
**Public API:** `CloneDetector::ComputeSimilarityWithThreshold()`, `CloneDetector::ComputeBlendedSimilarityWithThreshold()`

When comparing candidate pairs against a known similarity threshold, the full LCS
computation can often be short-circuited. The threshold-aware variants periodically
check whether the target LCS length is still reachable and terminate early if not.

**Minimum LCS derivation.** From the Dice coefficient formula:

```
threshold <= 2 * |LCS| / (|A| + |B|)
```

Solving for the minimum LCS length:

```
minLcs = ceil(threshold * (|A| + |B|) / 2)
```

**Early termination check.** At row `i` of the bit-parallel computation, the current
LCS length is `popcount(M)` and the remaining rows are `m - (i + 1)`. Each remaining
row can increment the LCS by at most 1, but never beyond the number of unmatched
positions in B:

```
maxAdditional = min(m - (i + 1), n - popcount(M))
```

If `popcount(M) + maxAdditional < minLcs`, the threshold is unreachable and the
function returns 0 immediately.

**Check frequency.** The `popcount` operation has a small but non-zero cost. The check
frequency is tuned per tier to balance early-termination savings against overhead on
pairs that do pass:

| Tier | Check Interval | Rationale |
|---|---|---|
| Tier 1 (n <= 64) | Every 4 rows | Cheap popcount on single word; rows are also cheap |
| Tier 2 (n <= 256) | Every 8 rows | Popcount over 4 words; rows are moderately expensive |
| Tier 3 (n > 256) | Every 16 rows | Popcount over W words; rows are expensive |

**Performance impact:** Saves 50-90% of LCS computation time on failing pairs (which
are the majority of candidates), with less than 5% overhead on passing pairs due to
the amortized popcount checks.

### Five-Stage Filter Cascade

The complete candidate filtering pipeline is designed as a cascade of progressively
more expensive tests. Each stage is a **sound** upper bound: if a pair fails any stage,
it provably cannot reach the similarity threshold.

**Cascade summary (from cheapest to most expensive):**

| Stage | Test | Cost | Typical Rejection Rate |
|---|---|---|---|
| 1 | Shared fingerprints >= `minHashMatches` | O(1) | Baseline |
| 2 | Shared fingerprints >= adaptive minimum | O(1) | 50-80% of remaining |
| 3 | Length ratio compatible with threshold | O(1) | Variable |
| 4 | Bag-of-tokens Dice >= threshold | O(V) | 70-95% of remaining |
| 5 | Threshold-aware LCS Dice >= threshold | O(m * ceil(n/64)) | Final exact check |

The cascade ordering ensures that the vast majority of non-matching pairs are rejected
by O(1) or O(V) checks before the O(m * ceil(n/64)) LCS computation is reached. This
is particularly impactful at high thresholds (e.g., 0.97) where the candidate set can
be large but the pass rate is low.

### Adaptive Fingerprint Threshold

**Location:** Inside `CloneDetector::Detect()` (candidate filtering loop)

For high similarity thresholds, the static `minHashMatches` (default: 3) is too
permissive, admitting many candidates that cannot possibly reach the threshold. The
adaptive filter dynamically raises this minimum based on the following reasoning:

**Derivation.** Each token change disrupts up to `hashWindowSize` consecutive
rolling-hash windows. At similarity threshold `T`, at most a fraction `(1 - T)` of
tokens differ. Therefore, the fraction of fingerprints that survive unchanged is at
least:

```
changeRate = 1 - T
minSurvivalRate = max(0, 1 - changeRate * hashWindowSize)
```

For a block with `L` tokens, the number of fingerprint positions is
`fps = L - hashWindowSize + 1`. The minimum expected shared fingerprints between two
blocks at threshold `T` is:

```
expectedShared = minSurvivalRate * min(fpsA, fpsB)
```

A safety factor of 0.5 is applied to account for hash collisions, edge effects at
block boundaries, and the fact that changes may cluster rather than distribute
uniformly:

```
adaptiveMin = max(minHashMatches, floor(minSurvivalRate * 0.5 * min(fpsA, fpsB)))
```

**Self-disabling property:** At lower thresholds (e.g., T <= 0.90 with
`hashWindowSize = 10`), `changeRate * hashWindowSize >= 1`, so `minSurvivalRate = 0`
and the adaptive filter has no effect, falling back to the static minimum.

**Example:** With `T = 0.97`, `hashWindowSize = 10`, and two 100-token blocks:
- `changeRate = 0.03`
- `minSurvivalRate = max(0, 1 - 0.03 * 10) = 0.70`
- `fps = 100 - 10 + 1 = 91`
- `adaptiveMin = max(3, floor(0.70 * 0.5 * 91)) = max(3, 31) = 31`

This means the pair needs at least 31 shared fingerprints instead of 3, eliminating
a large fraction of noise candidates.

### Bag-of-Tokens Dice Pre-Filter

**Function:** `CloneDetector::BagDiceCompatible()` in `src/codedup/CloneDetector.cpp`\
**Data structure:** `BlockHistogram` in `src/codedup/CloneDetector.hpp`

This filter computes an upper bound on the Dice coefficient using token frequency
histograms, without considering token order.

**Algorithm:**

1. For each block, a `BlockHistogram` is precomputed: a flat array where
   `counts[t] = number of occurrences of token ID t`.
2. For a candidate pair (A, B), compute the multiset intersection:
   ```
   bagIntersection = sum_{t=0}^{maxId} min(histA.counts[t], histB.counts[t])
   ```
3. Compute the bag Dice upper bound:
   ```
   bagDice = 2 * bagIntersection / (|A| + |B|)
   ```
4. Reject if `bagDice < threshold`.

**Soundness:** See [Upper Bound Derivations for Pre-Filters](#upper-bound-derivations-for-pre-filters).

**Complexity:** O(V) where V = `globalMaxId + 1`. For structural normalization, the
vocabulary is compact (~1003 distinct IDs), making this effectively O(1) with a small
constant. The histogram precomputation is O(sum of block lengths) and is done once.

**Tightness compared to length-ratio bound:** The bag-of-tokens bound accounts for the
actual token composition, not just lengths. Two blocks of similar length but
completely different token distributions (e.g., one arithmetic-heavy, one
control-flow-heavy) will have a low bag Dice despite passing the length-ratio filter.

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

### Partitioned MapReduce Candidate Gathering

**Location:** Inside `CloneDetector::Detect()` (parallel candidate gathering phase)

Counting shared fingerprints between block pairs is parallelized using a partitioned
MapReduce scheme that avoids contention between threads:

1. **Partitioned map phase.** `numWorkers` threads each process fingerprints in a
   striped (interleaved) pattern. Each worker maintains `numWorkers` partition maps,
   where each partition is indexed by `PairHash(blockA, blockB) % numWorkers`. This
   ensures that any given block pair is always assigned to the same partition across
   all workers.

2. **Parallel merge phase.** Each partition is independently merged across all workers.
   The merge starts by moving (not copying) the largest contributor's map to seed the
   merged result, then iterates remaining workers' maps, adding their counts. Worker
   maps are freed eagerly after merge to reduce peak memory.

3. **Candidate extraction.** After merging, candidate pairs are extracted from each
   partition in parallel, applying the five-stage filter cascade.

The partitioning ensures that the merge for each partition is independent, enabling
full parallelism without locks. The striped work assignment avoids tail-end stall
from expensive (high-frequency) fingerprints.

The `PairHash` functor uses golden-ratio bit mixing for decorrelation:

```
hash(a, b) = h(a) ^ (h(b) * 0x9E3779B97F4A7C15 + (h(a) << 6) + (h(a) >> 2))
```

The constant `0x9E3779B97F4A7C15` is the 64-bit golden ratio fractional part
(`floor(2^64 / phi)`), which provides near-optimal bit distribution.

### Multi-Threaded Similarity Computation

Inside `CloneDetector::Detect()`, the LCS similarity computation (Step 3) is the
computational bottleneck. It is parallelized across all available CPU cores using
`stdexec::bulk` on an `exec::static_thread_pool`:

1. Candidate pairs are divided using striped (interleaved) assignment across workers
   for automatic load balancing.
2. Each worker writes results to its own `std::vector<ClonePair>`, avoiding
   synchronization on the hot path.
3. An `std::atomic<size_t>` counter tracks progress for the progress callback.
4. After all workers complete, per-worker results are merged with move iterators.

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
**Functions:** `ForwardMatch()`, `BackwardMatch()`, `PositionalTextSimilarity()`

Intra-function clone detection extends candidate matches to their maximal length by
comparing consecutive token IDs. This comparison is accelerated using
`std::experimental::native_simd<uint32_t>`, which auto-selects the widest SIMD
register available on the target architecture:

| Architecture | Elements per Iteration |
|---|---|
| SSE2 | 4 |
| AVX2 | 8 |
| AVX-512 | 16 |

**Forward extension** (`ForwardMatch()`): Loads `kSimdWidth`
`NormalizedTokenId` values from both regions, compares them with SIMD `!=`, and checks
for any mismatch. On mismatch, `find_first_set()` identifies the exact lane. A scalar
tail loop handles the remainder when `maxLen` is not divisible by `kSimdWidth`.

**Backward extension** (`BackwardMatch()`): Scans backward from the candidate
positions using the same SIMD approach, loading elements in reverse order. When a
mismatch is found in a SIMD group, a manual reverse scan of that group determines the
exact boundary.

**Positional text similarity** (`PositionalTextSimilarity()`): Uses SIMD to count
matching text-preserving IDs across aligned regions, with `popcount()` on the
comparison mask to count matching lanes per iteration.

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
| `BlockHistogram` | `src/codedup/CloneDetector.hpp` | Token frequency histogram for bag-of-tokens pre-filter |
| `BitVector256` | `src/codedup/CloneDetector.cpp` | Fixed 256-bit bitvector for Tier 2 LCS |
| `DynamicBitVector` | `src/codedup/CloneDetector.cpp` | Variable-width bitvector for Tier 3 LCS |
| `UnionFind` | `src/codedup/CloneDetector.cpp` | Disjoint-set for grouping clone pairs |
| `PairHash` | `src/codedup/CloneDetector.cpp` | Golden-ratio hash functor for block pair keys |
| `CandidatePair` | `src/codedup/CloneDetector.cpp` | Lightweight block pair pending similarity check |
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
| `src/codedup/CloneDetector.hpp` | Inter-function detection API and data types |
| `src/codedup/CloneDetector.cpp` | Fingerprinting, bit-parallel LCS, threshold-aware LCS, bag-of-tokens filter, Union-Find grouping |
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
