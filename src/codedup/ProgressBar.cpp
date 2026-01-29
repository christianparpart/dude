// SPDX-License-Identifier: Apache-2.0
#include <codedup/ProgressBar.hpp>

#include <algorithm>
#include <cstdio>
#include <format>
#include <string>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define CODEDUP_ISATTY(fd) _isatty(fd)
#define CODEDUP_FILENO(f) _fileno(f)
#else
#include <unistd.h>
#define CODEDUP_ISATTY(fd) isatty(fd)
#define CODEDUP_FILENO(f) fileno(f)
#endif

namespace codedup
{

namespace
{

#ifdef _WIN32
/// @brief Enables VT100 escape sequence processing on Windows consoles.
///
/// Windows consoles do not process VT100 escape sequences by default.
/// This function enables the ENABLE_VIRTUAL_TERMINAL_PROCESSING flag
/// so that sequences like \033[K (erase to end of line) work correctly.
void EnableVTProcessing(FILE* output)
{
    auto const handle = reinterpret_cast<HANDLE>(_get_osfhandle(CODEDUP_FILENO(output)));
    if (handle == INVALID_HANDLE_VALUE)
        return;
    DWORD mode = 0;
    if (GetConsoleMode(handle, &mode))
        SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#endif

} // namespace

ProgressBar::ProgressBar(std::string_view stageName, size_t totalItems, FILE* output)
    : _stageName(stageName), _totalItems(totalItems), _output(output), _isTTY(CODEDUP_ISATTY(CODEDUP_FILENO(output)))
{
#ifdef _WIN32
    if (_isTTY)
        EnableVTProcessing(_output);
#endif
}

void ProgressBar::Start()
{
    _startTime = std::chrono::steady_clock::now();
    _lastRenderTime = _startTime;
    _completedItems.store(0, std::memory_order_relaxed);
    _smoothedRate = 0.0;
}

void ProgressBar::Tick()
{
    auto const completed = _completedItems.fetch_add(1, std::memory_order_relaxed) + 1;
    (void)completed;

    if (!_isTTY)
        return;

    auto const now = std::chrono::steady_clock::now();
    if (now - _lastRenderTime >= kRenderInterval)
        Render();
}

void ProgressBar::Update(size_t current, size_t total)
{
    _totalItems.store(total, std::memory_order_relaxed);
    _completedItems.store(current, std::memory_order_relaxed);

    if (!_isTTY)
        return;

    auto const now = std::chrono::steady_clock::now();
    if (now - _lastRenderTime >= kRenderInterval)
        Render();
}

void ProgressBar::Finish(bool clearLine)
{
    if (!_isTTY)
        return;

    if (clearLine)
    {
        ClearLine();
    }
    else
    {
        // Force 100% render
        auto const total = _totalItems.load(std::memory_order_relaxed);
        if (total > 0)
            _completedItems.store(total, std::memory_order_relaxed);
        Render();
        std::fputc('\n', _output);
    }
}

void ProgressBar::Log(std::string_view message)
{
    if (!_isTTY)
    {
        std::fputs(std::string(message).c_str(), _output);
        std::fputc('\n', _output);
        return;
    }

    ClearLine();
    std::fputs(std::string(message).c_str(), _output);
    std::fputc('\n', _output);
    Render();
}

auto ProgressBar::MakeTickCallback() -> ProgressCallback
{
    return [this](size_t /*current*/, size_t /*total*/) { Tick(); };
}

auto ProgressBar::MakeAbsoluteCallback() -> ProgressCallback
{
    return [this](size_t current, size_t total) { Update(current, total); };
}

auto ProgressBar::IsActive() const -> bool
{
    return _isTTY;
}

auto ProgressBar::FormatDuration(double seconds) -> std::string
{
    if (seconds < 1.0)
        return "< 1s";

    auto const totalSeconds = static_cast<int>(seconds);
    if (totalSeconds < 60)
        return std::format("{}s", totalSeconds);

    auto const minutes = totalSeconds / 60;
    auto const secs = totalSeconds % 60;
    return std::format("{}m {}s", minutes, secs);
}

void ProgressBar::ClearLine()
{
    std::fputs("\r\033[K", _output);
    std::fflush(_output);
}

void ProgressBar::Render()
{
    _lastRenderTime = std::chrono::steady_clock::now();

    auto const completed = _completedItems.load(std::memory_order_relaxed);
    auto const total = _totalItems.load(std::memory_order_relaxed);

    auto const elapsed = std::chrono::duration<double>(_lastRenderTime - _startTime).count();

    // Compute progress fraction
    double fraction = 0.0;
    if (total > 0)
        fraction = std::min(1.0, static_cast<double>(completed) / static_cast<double>(total));

    // Update smoothed rate (EMA)
    if (elapsed > 0.0)
    {
        auto const currentRate = static_cast<double>(completed) / elapsed;
        if (_smoothedRate <= 0.0)
            _smoothedRate = currentRate;
        else
            _smoothedRate = kSmoothingAlpha * currentRate + (1.0 - kSmoothingAlpha) * _smoothedRate;
    }

    // Compute ETA
    std::string etaStr = "?";
    if (total > 0 && _smoothedRate > 0.0 && completed < total)
    {
        auto const remaining = static_cast<double>(total - completed) / _smoothedRate;
        etaStr = std::format("~{}", FormatDuration(remaining));
    }
    else if (total > 0 && completed >= total)
    {
        etaStr = "done";
    }

    // Build progress bar
    auto const filledCount = static_cast<size_t>(fraction * static_cast<double>(kBarWidth));
    auto const emptyCount = kBarWidth - filledCount;

    std::string bar(filledCount, '=');
    if (filledCount < kBarWidth)
    {
        bar += '>';
        if (emptyCount > 1)
            bar.append(emptyCount - 1, '-');
    }

    auto const percentage = static_cast<int>(fraction * 100.0);

    // Pad stage name to 14 chars for alignment
    auto const paddedName = std::format("{:<14}", _stageName);

    auto const line = std::format("\r\033[K{} [{}] {:3}%  elapsed: {}  ETA: {}", paddedName, bar, percentage,
                                  FormatDuration(elapsed), etaStr);

    std::fputs(line.c_str(), _output);
    std::fflush(_output);
}

} // namespace codedup
