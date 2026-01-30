// SPDX-License-Identifier: Apache-2.0
#include <codedup/ProgressBar.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <thread>
#include <vector>

using namespace codedup;

namespace
{

/// @brief RAII wrapper for FILE* opened to /dev/null (non-TTY).
struct NullFile
{
    FILE* file;
    NullFile() : file(std::fopen("/dev/null", "w")) {} // NOLINT(cppcoreguidelines-owning-memory)
    ~NullFile() { std::fclose(file); }                 // NOLINT(cppcoreguidelines-owning-memory)
    NullFile(NullFile const&) = delete;
    NullFile& operator=(NullFile const&) = delete;
    NullFile(NullFile&&) = delete;
    NullFile& operator=(NullFile&&) = delete;
};

} // namespace

TEST_CASE("ProgressBar.Construction", "[ProgressBar]")
{
    NullFile nf;
    REQUIRE(nf.file != nullptr);
    ProgressBar bar("Testing", 100, nf.file);
    CHECK_FALSE(bar.IsActive());
}

TEST_CASE("ProgressBar.TickNoTTY", "[ProgressBar]")
{
    NullFile nf;
    REQUIRE(nf.file != nullptr);
    ProgressBar bar("Testing", 10, nf.file);
    bar.Start();
    for (size_t i = 0; i < 10; ++i)
        bar.Tick();
    bar.Finish();
}

TEST_CASE("ProgressBar.ThreadSafeTick", "[ProgressBar]")
{
    NullFile nf;
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
    NullFile nf;
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
    NullFile nf;
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
    NullFile nf;
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
    NullFile nf;
    REQUIRE(nf.file != nullptr);
    ProgressBar bar("NoClear", 10, nf.file);
    bar.Start();
    for (size_t i = 0; i < 10; ++i)
        bar.Tick();
    bar.Finish(false); // non-clearing finish path
}

TEST_CASE("ProgressBar.FinishWithoutStart", "[ProgressBar]")
{
    NullFile nf;
    REQUIRE(nf.file != nullptr);
    ProgressBar bar("NoStart", 10, nf.file);
    // Finish without calling Start — should not crash on non-TTY
    bar.Finish();
    bar.Finish(false);
}
