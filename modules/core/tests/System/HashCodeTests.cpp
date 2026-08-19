// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "System/HashCode.hpp"
#include "System/Span.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#if defined(__linux__)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#endif

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

// ===========================================================================================
// #2402 -- the seed is per-PROCESS, and both halves of that are now asserted.
//
// This case used to assert only the within-one-process half, so THE FIRST CLAUSE OF ITS OWN NAME
// WAS UNTESTED: a constant seed satisfies every assertion it made. The across-processes half is
// the one the property exists for -- `HashCode.hpp`'s doc-comment states it in terms ("consistent
// within a run but differ across runs (by design, to discourage persisting hash codes and to
// resist hash-flooding)"), and it is .NET's, whose `s_seed` is `GenerateGlobalSeed()`
// (`HashCode.cs:58,70-75`).
//
// A PLAIN fork() CANNOT TEST IT, and finding that out is worth recording: the seed is a
// function-local static initialised on first use, so a forked child INHERITS it and reports the
// same hash whatever the source. The child must re-exec, which is the idiom #1979 established for
// PosixSignalTests and for the same class of reason -- a fresh process is the only place the
// property is observable.
//
// NOT A CSPRNG, AND THAT IS PARITY RATHER THAN A GAP (#2402). .NET has two entropy entry points
// and chooses between them deliberately (`Interop.GetRandomBytes.cs:18-27`); `GenerateGlobalSeed`
// calls the NON-cryptographic one. `std::random_device` here is the counterpart. Upgrading it
// would be a divergence, and would put a cryptography component under every consumer of
// `Core.Base`.
// ===========================================================================================

TEST(HashCodeTests, Seed_IsSharedByEveryInstanceWithinOneProcess) {
    // The per-process global seed is generated once and shared by every HashCode
    // instance in this run, so two independently-constructed accumulators given the
    // same input still agree (the seed is not per-instance randomness).
    HashCode hc1, hc2;
    hc1.Add(std::string("hello"));
    hc2.Add(std::string("hello"));
    EXPECT_EQ(hc1.ToHashCode(), hc2.ToHashCode());
}

#if defined(__linux__)
namespace {

constexpr const char* kHashSeedChildEnv = "SHARP_RUNTIME_HASHCODE_SEED_CHILD";
constexpr const char* kProbeInput = "the same input in every process";

/// The hash this process computes for a fixed input. Shared by the parent and the re-exec'd child
/// so that the two are literally computing the same thing.
uint32_t probeHash() {
    HashCode hc;
    hc.Add(std::string(kProbeInput));
    return static_cast<uint32_t>(hc.ToHashCode());
}

/// Re-executes this binary so that only HashCodeSeedChildBody runs, and returns the hash it wrote
/// to the pipe. Returns std::nullopt if the child could not report one.
std::optional<uint32_t> hashFromAFreshProcess() {
    int pipeFds[2] = {-1, -1};
    if (::pipe(pipeFds) != 0) return std::nullopt;

    const pid_t child = ::fork();
    if (child < 0) { ::close(pipeFds[0]); ::close(pipeFds[1]); return std::nullopt; }
    if (child == 0) {
        ::close(pipeFds[0]);
        // The child writes to fd 3, not stdout: gtest's own output would otherwise be
        // indistinguishable from the payload.
        ::dup2(pipeFds[1], 3);
        ::close(pipeFds[1]);
        ::setenv(kHashSeedChildEnv, "1", 1);
        ::execl("/proc/self/exe", "SharpRuntimeTests_Core_Base",
                "--gtest_filter=HashCodeSeedChildBody.Run", "--gtest_brief=1",
                static_cast<char*>(nullptr));
        ::_exit(90); // exec failed
    }

    ::close(pipeFds[1]);
    std::string received;
    char buffer[64];
    ssize_t n = 0;
    while ((n = ::read(pipeFds[0], buffer, sizeof(buffer))) > 0)
        received.append(buffer, static_cast<size_t>(n));
    ::close(pipeFds[0]);

    int status = 0;
    if (::waitpid(child, &status, 0) != child) return std::nullopt;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return std::nullopt;
    if (received.empty()) return std::nullopt;
    return static_cast<uint32_t>(std::stoul(received));
}

} // namespace

// The child half. It is a test only because re-execing this binary is how a fresh process is
// obtained; in an ordinary run the environment variable is absent and it does nothing.
TEST(HashCodeSeedChildBody, Run) {
    if (::getenv(kHashSeedChildEnv) == nullptr) {
        // SUCCEED rather than GTEST_SKIP, deliberately and following #1979's
        // PosixSignalChildBody: this driver runs in EVERY ordinary run, and a skip here would
        // move the repository's gate off "0 skipped" permanently -- a documented property of the
        // floor in CLAUDE.md rule 2, not an incidental one.
        SUCCEED() << "child-body driver; runs only when re-executed by the #2402 parent case";
        return;
    }
    const std::string payload = std::to_string(probeHash());
    const ssize_t written = ::write(3, payload.data(), payload.size());
    ASSERT_EQ(written, static_cast<ssize_t>(payload.size()));
}

TEST(HashCodeTests, Seed_DiffersAcrossProcesses) {
    const uint32_t here = probeHash();

    // Two fresh processes, not one: a single disagreement could in principle be the 1-in-2^32
    // coincidence, and more usefully, two children make the failure mode legible -- a constant
    // seed makes BOTH agree with the parent, where a flake would not.
    const std::optional<uint32_t> first = hashFromAFreshProcess();
    const std::optional<uint32_t> second = hashFromAFreshProcess();
    ASSERT_TRUE(first.has_value()) << "the first child could not report its hash";
    ASSERT_TRUE(second.has_value()) << "the second child could not report its hash";

    EXPECT_FALSE(*first == here && *second == here)
        << "two fresh processes both computed the parent's hash (" << here
        << ") for the same input, so the global seed is not per-process";
}
#endif // __linux__
