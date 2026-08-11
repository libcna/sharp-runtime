// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "System/HashCode.hpp"
#include "System/Span.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

using System::HashCode;

// A hash code of zero is legal (docs/HashAssertionContractRule.md R6), and System::HashCode makes
// zero genuinely reachable rather than academic: the initial state is `2166136261u ^ GlobalSeed()`
// (HashCode.hpp:34-39), so one process seed in 2^32 leaves an untouched accumulator holding
// exactly 0. What a default HashCode does promise is that it is the same for every instance
// within one process, which is what the seed being per-process rather than per-object means.
TEST(HashCodeTests, ToHashCode_DefaultIsStableWithinTheProcess) {
    HashCode hc;
    EXPECT_EQ(hc.ToHashCode(), hc.ToHashCode());
    HashCode other;
    EXPECT_EQ(hc.ToHashCode(), other.ToHashCode());
}

TEST(HashCodeTests, Add_SameValuesTwice_SameResult) {
    HashCode hc1, hc2;
    hc1.Add(42);
    hc2.Add(42);
    EXPECT_EQ(hc1.ToHashCode(), hc2.ToHashCode());
}

// The designed property is that the accumulator depends on the value added at all; two unequal
// inputs are still permitted to collide. Stated over a family rather than over one pair because
// the per-process seed makes any single pair non-reproducible across runs
// (docs/HashAssertionContractRule.md R4). An Add() that discarded its argument -- or a mix() that
// dropped the xor -- would make every one of these sixteen comparisons agree.
TEST(HashCodeTests, Add_ResultDependsOnTheValueAdded) {
    int differing = 0;
    for (int v = 1; v <= 16; ++v) {
        HashCode zero, other;
        zero.Add(0);
        other.Add(v);
        if (zero.ToHashCode() != other.ToHashCode()) ++differing;
    }
    EXPECT_GT(differing, 0) << "an accumulator that ignored Add()'s argument would give 0 of 16";
}

TEST(HashCodeTests, Combine1_Consistent) {
    EXPECT_EQ(HashCode::Combine(10), HashCode::Combine(10));
}

TEST(HashCodeTests, Combine2_Consistent) {
    EXPECT_EQ(HashCode::Combine(1, 2), HashCode::Combine(1, 2));
}

// Order sensitivity is the designed property of HashCode::Combine: it is a sequential FNV-1a
// fold, not a commutative combiner such as xor or addition. Asserted over a family of pairs
// rather than over Combine(1,2) vs Combine(2,1), because the per-process seed (HashCode.hpp:34-39)
// makes any individual pair a coin flip that no run is guaranteed to win
// (docs/HashAssertionContractRule.md R4). A commutative combiner scores 0 here; the real one
// scores 28.
TEST(HashCodeTests, Combine2_IsOrderSensitiveNotCommutative) {
    int swapsThatChangeTheResult = 0;
    for (int a = 1; a <= 8; ++a)
        for (int b = a + 1; b <= 8; ++b)
            if (HashCode::Combine(a, b) != HashCode::Combine(b, a)) ++swapsThatChangeTheResult;
    EXPECT_GT(swapsThatChangeTheResult, 0)
        << "a commutative combiner would give 0 of 28 order-sensitive pairs";
}

TEST(HashCodeTests, Combine3_Consistent) {
    EXPECT_EQ(HashCode::Combine(1, 2, 3), HashCode::Combine(1, 2, 3));
}

TEST(HashCodeTests, Combine8_DoesNotThrow) {
    EXPECT_NO_THROW(HashCode::Combine(1, 2, 3, 4, 5, 6, 7, 8));
}

TEST(HashCodeTests, AddBytes_SameData_SameHash) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    HashCode hc1, hc2;
    hc1.AddBytes(data);
    hc2.AddBytes(data);
    EXPECT_EQ(hc1.ToHashCode(), hc2.ToHashCode());
}

TEST(HashCodeTests, AddBytes_Span_MatchesVectorOverload) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
    System::ReadOnlySpan<uint8_t> span(data.data(), static_cast<SharpRuntime::intcs>(data.size()));
    HashCode hc1, hc2;
    hc1.AddBytes(data);
    hc2.AddBytes(span);
    EXPECT_EQ(hc1.ToHashCode(), hc2.ToHashCode());
}

// SR-AUD-043a (#1852): HashCode::AddBytes casts the span's signed length straight
// to size_t, so a negative-length ReadOnlySpan<uint8_t> would drive an unbounded
// raw read. That path is closed at the source: once ReadOnlySpan's constructor
// rejects a negative length, such a span can never be built to hand to AddBytes.
TEST(HashCodeTests, AddBytes_NegativeLengthSpan_CannotBeConstructed) {
    std::uint8_t one = 0x42;
    EXPECT_THROW(System::ReadOnlySpan<std::uint8_t>(&one, -1),
                 System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// SR-AUD-043b (#1854, approved 2026-07-31): AddBytes(ReadOnlySpan<uint8_t>)
// rejects a negative length rather than casting it to a huge size_t.
//
// This is the defence-in-depth half of 043: since #1852 no negative-length span
// can be CONSTRUCTED, so the throw is unreachable through the public surface and
// there is nothing to assert about it behaviourally -- what IS assertable is the
// approved contract change itself, the exception specification. The overloads
// taking an unsigned length cannot receive a negative one and keep noexcept.
// ---------------------------------------------------------------------------

TEST(HashCodeTests, AddBytes_ExceptionSpecificationsAreTheApprovedOnes) {
    HashCode hc;
    std::vector<uint8_t> data = {1, 2, 3};
    System::ReadOnlySpan<uint8_t> span(data.data(), 3);
    static_assert(!noexcept(std::declval<HashCode&>().AddBytes(
                      std::declval<const System::ReadOnlySpan<uint8_t>&>())),
                  "#1854: AddBytes(ReadOnlySpan) must be able to throw");
    static_assert(noexcept(std::declval<HashCode&>().AddBytes(
                      std::declval<const std::uint8_t*>(), std::size_t{0})),
                  "#1854: AddBytes(const uint8_t*, size_t) must stay noexcept");
    static_assert(noexcept(std::declval<HashCode&>().AddBytes(
                      std::declval<const std::vector<std::uint8_t>&>())),
                  "#1854: AddBytes(const vector<uint8_t>&) must stay noexcept");
    // The valid path is unchanged: an ordinary span still hashes as before.
    hc.AddBytes(span);
    HashCode reference;
    reference.AddBytes(data);
    EXPECT_EQ(hc.ToHashCode(), reference.ToHashCode());
}

TEST(HashCodeTests, AddBytes_EmptySpan_IsANoOp) {
    std::vector<uint8_t> data;
    System::ReadOnlySpan<uint8_t> empty(data.data(), 0);
    HashCode withEmpty, without;
    withEmpty.AddBytes(empty);
    EXPECT_EQ(withEmpty.ToHashCode(), without.ToHashCode());
}

TEST(HashCodeTests, Seed_DiffersAcrossProcessesButConsistentWithinOne) {
    // The per-process global seed is generated once and shared by every HashCode
    // instance in this run, so two independently-constructed accumulators given the
    // same input still agree (the seed is not per-instance randomness).
    HashCode hc1, hc2;
    hc1.Add(std::string("hello"));
    hc2.Add(std::string("hello"));
    EXPECT_EQ(hc1.ToHashCode(), hc2.ToHashCode());
}
