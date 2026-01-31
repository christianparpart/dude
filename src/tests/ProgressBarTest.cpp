// SPDX-License-Identifier: Apache-2.0
#include <dude/ProgressBar.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace dude;

namespace
{

/// @brief RAII wrapper for a temporary FILE* that is guaranteed non-TTY on all platforms.
///
/// Uses tmpfile() instead of the null device because Windows _isatty() reports NUL
/// as a TTY (it's a character device), which defeats the purpose of testing non-TTY behavior.
struct NonTtyFile
{
    FILE* file;
    NonTtyFile() : file(std::tmpfile()) {} // NOLINT(cppcoreguidelines-owning-memory)
    ~NonTtyFile() { std::fclose(file); }   // NOLINT(cppcoreguidelines-owning-memory)
    NonTtyFile(NonTtyFile const&) = delete;
    NonTtyFile& operator=(NonTtyFile const&) = delete;
    NonTtyFile(NonTtyFile&&) = delete;
    NonTtyFile& operator=(NonTtyFile&&) = delete;

    /// @brief Reads back all content written to the temporary file.
    [[nodiscard]] auto ReadContent() const -> std::string
    {
        std::fflush(file);
        std::rewind(file);
        std::string content;
        char buf[1024];
        while (auto const n = std::fread(buf, 1, sizeof(buf), file))
            content.append(buf, n);
        return content;
    }
};

} // namespace

TEST_CASE("ProgressBar.Construction", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);
    ProgressBar bar("Testing", 100, nf.file);
    CHECK_FALSE(bar.IsActive());
}

TEST_CASE("ProgressBar.TickNoTTY", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);
    ProgressBar bar("Testing", 10, nf.file);
    bar.Start();
    for (size_t i = 0; i < 10; ++i)
        bar.Tick();
    bar.Finish();
}

TEST_CASE("ProgressBar.ThreadSafeTick", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);
    ProgressBar bar("Threaded", 8000, nf.file);
    bar.Start();

    std::vector<std::jthread> threads;
    threads.reserve(8);
    for (size_t t = 0; t < 8; ++t)
        threads.emplace_back(
            [&]
            {
                for (size_t i = 0; i < 1000; ++i)
                    bar.Tick();
            });

    threads.clear(); // join all
    bar.Finish();
}

TEST_CASE("ProgressBar.FormatDuration", "[ProgressBar]")
{
    CHECK(ProgressBar::FormatDuration(0.0) == "< 1s");
    CHECK(ProgressBar::FormatDuration(0.5) == "< 1s");
    CHECK(ProgressBar::FormatDuration(5.0) == "5s");
    CHECK(ProgressBar::FormatDuration(59.0) == "59s");
    CHECK(ProgressBar::FormatDuration(65.0) == "1m 5s");
    CHECK(ProgressBar::FormatDuration(3661.0) == "61m 1s");
}

TEST_CASE("ProgressBar.UpdateAbsolute", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);
    ProgressBar bar("Detecting", 0, nf.file);
    bar.Start();
    bar.Update(50, 100);
    bar.Update(75, 100);
    bar.Update(100, 200);
    bar.Finish();
}

TEST_CASE("ProgressBar.LogDuringProgress", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);
    ProgressBar bar("Logging", 10, nf.file);
    bar.Start();
    bar.Tick();
    bar.Log("Some verbose message");
    bar.Tick();
    bar.Finish();
}

TEST_CASE("ProgressBar.MakeCallbacks", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);
    ProgressBar bar("Callbacks", 100, nf.file);
    bar.Start();

    auto tickCb = bar.MakeTickCallback();
    REQUIRE(tickCb);
    tickCb(1, 100);
    tickCb(2, 100);

    auto absCb = bar.MakeAbsoluteCallback();
    REQUIRE(absCb);
    absCb(50, 100);
    absCb(100, 100);

    bar.Finish();
}

TEST_CASE("ProgressBar.FinishNoClear", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);
    ProgressBar bar("NoClear", 10, nf.file);
    bar.Start();
    for (size_t i = 0; i < 10; ++i)
        bar.Tick();
    bar.Finish(false); // non-clearing finish path
}

TEST_CASE("ProgressBar.FinishWithoutStart", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);
    ProgressBar bar("NoStart", 10, nf.file);
    // Finish without calling Start — should not crash on non-TTY
    bar.Finish();
    bar.Finish(false);
}

// ---------------------------------------------------------------------------
// Tests using forceTTY=true to exercise the rendering code paths
// ---------------------------------------------------------------------------

TEST_CASE("ProgressBar.ForceTTY.RenderOutput", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);

    ProgressBar bar("Scanning", 10, nf.file, /*forceTTY=*/true);
    CHECK(bar.IsActive());

    bar.Start();
    // Tick enough to trigger at least one render (renders happen on time interval,
    // but the first Tick after Start should render since lastRenderTime == startTime)
    for (size_t i = 0; i < 10; ++i)
        bar.Tick();

    bar.Finish(/*clearLine=*/false);

    auto const content = nf.ReadContent();
    // The output should contain the stage name and a progress bar
    CHECK(content.find("Scanning") != std::string::npos);
    // Should contain percentage
    CHECK(content.find('%') != std::string::npos);
}

TEST_CASE("ProgressBar.ForceTTY.FinishClearLine", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);

    ProgressBar bar("Clearing", 5, nf.file, /*forceTTY=*/true);
    bar.Start();
    for (size_t i = 0; i < 5; ++i)
        bar.Tick();

    bar.Finish(/*clearLine=*/true);

    auto const content = nf.ReadContent();
    // ClearLine writes "\r\033[K"
    CHECK(content.find("\033[K") != std::string::npos);
}

TEST_CASE("ProgressBar.ForceTTY.FinishNoClearShowsDone", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);

    ProgressBar bar("Done", 10, nf.file, /*forceTTY=*/true);
    bar.Start();
    for (size_t i = 0; i < 10; ++i)
        bar.Tick();

    bar.Finish(/*clearLine=*/false);

    auto const content = nf.ReadContent();
    // Finish(false) renders final state + newline; should show 100% or "done"
    CHECK(content.find("100%") != std::string::npos);
}

TEST_CASE("ProgressBar.ForceTTY.LogClearsAndReRenders", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);

    ProgressBar bar("Logging", 10, nf.file, /*forceTTY=*/true);
    bar.Start();
    bar.Tick();

    bar.Log("diagnostic output");

    auto const content = nf.ReadContent();
    // Log should produce the diagnostic message followed by a newline
    CHECK(content.find("diagnostic output") != std::string::npos);
}

TEST_CASE("ProgressBar.ForceTTY.UpdateAbsoluteRenders", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);

    ProgressBar bar("Detect", 0, nf.file, /*forceTTY=*/true);
    bar.Start();

    // Use absolute update to simulate late-discovered total
    bar.Update(50, 100);
    // Wait just enough to pass the render interval
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    bar.Update(100, 100);

    bar.Finish(/*clearLine=*/false);

    auto const content = nf.ReadContent();
    CHECK(content.find("Detect") != std::string::npos);
}

TEST_CASE("ProgressBar.MoveConstructor", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);

    ProgressBar bar("Moving", 10, nf.file, /*forceTTY=*/true);
    bar.Start();
    bar.Tick();

    // Move-construct a new bar from the old one
    ProgressBar bar2(std::move(bar));
    CHECK(bar2.IsActive());
    bar2.Tick();
    bar2.Finish(/*clearLine=*/true);
}

TEST_CASE("ProgressBar.ForceTTY.TickRendersOnInterval", "[ProgressBar]")
{
    NonTtyFile nf;
    REQUIRE(nf.file != nullptr);

    ProgressBar bar("Ticking", 100, nf.file, /*forceTTY=*/true);
    bar.Start();

    // First tick — might not render if interval hasn't passed
    bar.Tick();

    // Sleep past the render interval (50ms) and tick again — should trigger render
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    bar.Tick();

    bar.Finish(/*clearLine=*/true);

    auto const content = nf.ReadContent();
    // Should have rendered at least once with the stage name
    CHECK(content.find("Ticking") != std::string::npos);
}
