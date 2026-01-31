# Coding guidelines
- Document new functions, classes, structs, and their members using Doxygen style comments.
- Use const correctness throughout the codebase.
- Prefer C++23 with cnostexpr, std::ranges, std::format, where applicable.
- Use std::views::iota and other views for generating and transforming ranges.
- Use std::span for passing arrays and contiguous sequences.
- Use std::expected for error handling and its functional style methods like and_then, or_else, transform, transform_error, etc.
- Use range based for loop, structured bindings, and algorithms from the standard library.
- Use clang-format after changes to format C++ code according to project style.
- Use auto-type declaration to improve code readability.
- Ensure changes are covered by unit tests and aim always for increased code coverage.
- Reports from clang-tidy should not be cast away via NOLINT comments; instead, address the underlying issues.

# Building
- Use CMake with preset "clang-debug" for building and testing on Linux with Clang in debug mode.
- Use CMake with preset "clang-release-native" for performance testing on Linux with Clang in release mode.

# Testing
- Always run tests if available.

# Workflow
- Always mention the performance impact in detail in the summary, if any.
- Always perform and add risk assessment in the summary, if possible.
- Always report the code coverage results in the summary.
