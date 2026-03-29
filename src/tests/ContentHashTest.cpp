// SPDX-License-Identifier: Apache-2.0

#include <dude/ContentHash.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ContentHash.EmptyString", "[ContentHash]")
{
    // NIST test vector: SHA-256 of ""
    CHECK(dude::ComputeContentHash("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("ContentHash.Abc", "[ContentHash]")
{
    // NIST test vector: SHA-256 of "abc"
    CHECK(dude::ComputeContentHash("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("ContentHash.TwoBlockMessage", "[ContentHash]")
{
    // NIST test vector: SHA-256 of "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
    CHECK(dude::ComputeContentHash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("ContentHash.Deterministic", "[ContentHash]")
{
    auto const* const input = "Hello, world! This is a test of the content hashing system.";
    auto const hash1 = dude::ComputeContentHash(input);
    auto const hash2 = dude::ComputeContentHash(input);
    CHECK(hash1 == hash2);
    CHECK(hash1.size() == 64); // SHA-256 hex is always 64 characters
}

TEST_CASE("ContentHash.DifferentInputsDifferentHashes", "[ContentHash]")
{
    auto const hash1 = dude::ComputeContentHash("input A");
    auto const hash2 = dude::ComputeContentHash("input B");
    CHECK(hash1 != hash2);
}

TEST_CASE("ContentHash.ExactlyOneBlock", "[ContentHash]")
{
    // 55 bytes is the max payload that fits in a single 64-byte block with padding
    auto const input = std::string(55, 'x');
    auto const hash = dude::ComputeContentHash(input);
    CHECK(hash.size() == 64);
}

TEST_CASE("ContentHash.ExactlyTwoBlocks", "[ContentHash]")
{
    // 56 bytes requires two blocks for padding
    auto const input = std::string(56, 'y');
    auto const hash = dude::ComputeContentHash(input);
    CHECK(hash.size() == 64);
}

TEST_CASE("ContentHash.LargeInput", "[ContentHash]")
{
    // Multiple complete blocks plus partial
    auto const input = std::string(1000, 'z');
    auto const hash = dude::ComputeContentHash(input);
    CHECK(hash.size() == 64);
}
