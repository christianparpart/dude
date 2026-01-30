// SPDX-License-Identifier: Apache-2.0
#include <dude/ProgressBar.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
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
