// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <dude/ProgressCallback.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>

namespace dude
{

/// @brief Terminal progress bar for pipeline stages.
///
/// Displays a visual progress bar with elapsed time and smoothed ETA on stderr.
/// Thread-safe: Tick() and Update() can be called concurrently from multiple threads.
/// Automatically disables rendering when output is not a TTY (e.g. redirected to a file).
class ProgressBar
{
public:
    /// @brief Constructs a progress bar for a named pipeline stage.
    /// @param stageName Display name for the stage (e.g. "Tokenizing").
    /// @param totalItems Total number of items to process (0 if unknown).
    /// @param output File stream to write progress to (default: stderr).
    ProgressBar(std::string_view stageName, size_t totalItems, FILE* output = stderr);

    ProgressBar(ProgressBar const&) = delete;
    ProgressBar& operator=(ProgressBar const&) = delete;

    ProgressBar(ProgressBar&& other) noexcept
        : _stageName(std::move(other._stageName))
        , _completedItems(other._completedItems.load(std::memory_order_relaxed))
        , _totalItems(other._totalItems.load(std::memory_order_relaxed))
        , _output(other._output)
        , _isTTY(other._isTTY)
        , _startTime(other._startTime)
        , _lastRenderTime(other._lastRenderTime)
        , _smoothedRate(other._smoothedRate)
    {
    }

    ProgressBar& operator=(ProgressBar&&) = delete;
    ~ProgressBar() = default;

    /// @brief Records the start time of this stage.
    void Start();

    /// @brief Thread-safe increment of the completed item counter.
    ///
    /// Increments the atomic counter and triggers a rate-limited render (~50ms).
    void Tick();

    /// @brief Absolute position update for stages where total is discovered late.
    /// @param current Current progress position.
    /// @param total Total number of items (may change between calls).
    void Update(size_t current, size_t total);

    /// @brief Finalizes the progress bar.
    /// @param clearLine If true, clears the bar line; otherwise prints a final 100% state.
    void Finish(bool clearLine = true);

    /// @brief Clears the progress bar, prints a message, and re-renders the bar.
    ///
    /// Useful for verbose diagnostic messages during an active progress bar.
    /// @param message The message to print (a newline is appended).
    void Log(std::string_view message);

    /// @brief Creates a ProgressCallback that calls Tick() on each invocation.
    /// @return A ProgressCallback suitable for stages with known totals.
    [[nodiscard]] auto MakeTickCallback() -> ProgressCallback;

    /// @brief Creates a ProgressCallback that calls Update(current, total).
    /// @return A ProgressCallback suitable for stages where total may be unknown initially.
    [[nodiscard]] auto MakeAbsoluteCallback() -> ProgressCallback;

    /// @brief Returns whether rendering is active (output is a TTY).
    [[nodiscard]] auto IsActive() const -> bool;

    /// @brief Formats a duration into a human-readable string.
    /// @param seconds Duration in seconds.
    /// @return Formatted string (e.g. "< 1s", "5s", "2m 30s").
    [[nodiscard]] static auto FormatDuration(double seconds) -> std::string;

private:
    /// @brief Clears the current progress bar line on the terminal.
    void ClearLine();

    /// @brief Renders the progress bar to the output stream.
    void Render();

    std::string _stageName;                                         ///< Display name of the pipeline stage.
    std::atomic<size_t> _completedItems{0};                         ///< Thread-safe completed item counter.
    std::atomic<size_t> _totalItems{0};                             ///< Total items (may be updated dynamically).
    FILE* _output = stderr;                                         ///< Output stream for rendering.
    bool _isTTY = false;                                            ///< Whether _output is a TTY.
    std::chrono::steady_clock::time_point _startTime{};             ///< Start time of this stage.
    std::chrono::steady_clock::time_point _lastRenderTime{};        ///< Last render timestamp for rate limiting.
    double _smoothedRate = 0.0;                                     ///< EMA-smoothed items/second throughput.
    static constexpr std::chrono::milliseconds kRenderInterval{50}; ///< Minimum interval between renders.
    static constexpr double kSmoothingAlpha = 0.1;                  ///< EMA smoothing factor for throughput.
    static constexpr size_t kBarWidth = 30;                         ///< Width of the visual progress bar in characters.
};

} // namespace dude
