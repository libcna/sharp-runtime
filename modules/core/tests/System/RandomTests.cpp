// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "gtest/gtest.h"

#include <atomic>
#include <set>
#include <thread>
#include <vector>

#include "System/Random.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

using SharpRuntime::intcs;
using SharpRuntime::longcs;
TEST(RandomTests, NextWithMaxValue) {
    System::Random rng;
    const intcs max = 100;
    bool maxFound = false;
    bool maxMinusOneFound = false;
    for (int i = 0; i < 10000; ++i) {
        intcs value = rng.Next(max);
        //std::cout << value << std::endl;
        ASSERT_GE(value, 0);
        ASSERT_LT(value, max);
        maxFound = maxFound || value == max;
        maxMinusOneFound = maxMinusOneFound || value == (max - 1);
    }

    if (maxFound) { FAIL() << "Max value " << max << " found!";}
    if (!maxMinusOneFound) { FAIL() << "Max value minus one " << (max - 1) << " not found!";}
}

TEST(RandomTests, NextWithMinAndMaxValue) {
    System::Random rng;
    const intcs min = 10;
    const intcs max = 20;
    bool minFound = false;
    bool maxFound = false;
    bool maxMinusOneFound = false;
    for (int i = 0; i < 1000; ++i) {
        //std::cout << "Run " << i << std::endl;
        intcs value = rng.Next(min, max);
        //std::cout << value << std::endl;
        ASSERT_GE(value, min);
        ASSERT_LT(value, max);
        maxFound = maxFound || value == max;
        maxMinusOneFound = maxMinusOneFound || value == (max - 1);
        minFound = minFound || value == min;
    }
    if (maxFound) { FAIL() << "Max value " << max << " found!";}
    if (!maxMinusOneFound) { FAIL() << "Max value minus one " << (max - 1) << " not found!";}
    if (!minFound) { FAIL() << "Min value " << min << " not found!";}
}

TEST(RandomTests, NextWithoutArguments) {
    System::Random rng;
    for (int i = 0; i < 1000; ++i) {
        intcs value = rng.Next();
        ASSERT_GE(value, 0);
        ASSERT_LT(value, SharpRuntime::INTCS_MAX);
    }
}

TEST(RandomTests, SeededConstructorIsDeterministic) {
    System::Random rng1(42);
    System::Random rng2(42);
    for (int i = 0; i < 20; ++i)
        EXPECT_EQ(rng1.Next(1000), rng2.Next(1000));
}

TEST(RandomTests, DifferentSeedsDifferentSequences) {
    System::Random rng1(1);
    System::Random rng2(2);
    bool anyDifferent = false;
    for (int i = 0; i < 10; ++i)
        if (rng1.Next(10000) != rng2.Next(10000)) { anyDifferent = true; break; }
    EXPECT_TRUE(anyDifferent);
}

TEST(RandomTests, NextDoubleInRange) {
    System::Random rng(0);
    for (int i = 0; i < 1000; ++i) {
        double v = rng.NextDouble();
        ASSERT_GE(v, 0.0);
        ASSERT_LT(v, 1.0);
    }
}

TEST(RandomTests, NextDoubleCoversBothEnds) {
    System::Random rng(123);
    bool nearZero = false, nearOne = false;
    for (int i = 0; i < 10000; ++i) {
        double v = rng.NextDouble();
        if (v < 0.01) nearZero = true;
        if (v > 0.99) nearOne = true;
    }
    EXPECT_TRUE(nearZero);
    EXPECT_TRUE(nearOne);
}

TEST(RandomTests, NextBytesFilledCorrectly) {
    System::Random rng(7);
    std::vector<uint8_t> buf(16, 0);
    rng.NextBytes(buf);
    bool anyNonZero = false;
    for (auto b : buf) if (b != 0) { anyNonZero = true; break; }
    EXPECT_TRUE(anyNonZero);
    EXPECT_EQ(buf.size(), 16u);
}

TEST(RandomTests, NextBytesAllValuesInRange) {
    System::Random rng(99);
    std::vector<uint8_t> buf(1000);
    rng.NextBytes(buf);
    for (auto b : buf)
        ASSERT_LE(b, 255);
}

TEST(RandomTests, NextSingleInRange) {
    System::Random rng(42);
    for (int i = 0; i < 100; ++i) {
        float v = rng.NextSingle();
        EXPECT_GE(v, 0.0f);
        EXPECT_LT(v, 1.0f);
    }
}
TEST(RandomTests, NextSingleSeededIsDeterministic) {
    System::Random a(7), b(7);
    EXPECT_EQ(a.NextSingle(), b.NextSingle());
}

// --- NextInt64 ---
TEST(RandomTests, NextInt64_NoArgs_InRange) {
    System::Random rng(1);
    for (int i = 0; i < 100; ++i) {
        longcs v = rng.NextInt64();
        EXPECT_GE(v, 0LL);
        EXPECT_LT(v, SharpRuntime::LONGCS_MAX);
    }
}
TEST(RandomTests, NextInt64_MaxValue_InRange) {
    System::Random rng(2);
    const longcs max = 1000000000LL;
    for (int i = 0; i < 100; ++i) {
        longcs v = rng.NextInt64(max);
        EXPECT_GE(v, 0LL);
        EXPECT_LT(v, max);
    }
}
TEST(RandomTests, NextInt64_MaxZero_ReturnsZero) {
    System::Random rng(3);
    EXPECT_EQ(rng.NextInt64(0LL), 0LL);
}
TEST(RandomTests, NextInt64_Range_InRange) {
    System::Random rng(4);
    const longcs lo = 500000000000LL, hi = 600000000000LL;
    for (int i = 0; i < 100; ++i) {
        longcs v = rng.NextInt64(lo, hi);
        EXPECT_GE(v, lo);
        EXPECT_LT(v, hi);
    }
}
TEST(RandomTests, NextInt64_EqualMinMax_ReturnsMin) {
    System::Random rng(5);
    EXPECT_EQ(rng.NextInt64(42LL, 42LL), 42LL);
}
TEST(RandomTests, NextInt64_Seeded_Deterministic) {
    System::Random a(99), b(99);
    EXPECT_EQ(a.NextInt64(1000000LL), b.NextInt64(1000000LL));
}
TEST(RandomTests, NextInt64_MaxValueThrows) {
    System::Random rng(6);
    EXPECT_THROW(rng.NextInt64(-1LL), System::ArgumentOutOfRangeException);
}
TEST(RandomTests, NextInt64_InvalidRangeThrows) {
    System::Random rng(7);
    EXPECT_THROW(rng.NextInt64(10LL, 5LL), System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// Shared
// ---------------------------------------------------------------------------

TEST(RandomTests, Shared_ReturnsSameReference) {
    System::Random& a = System::Random::getSharedProperty();
    System::Random& b = System::Random::getSharedProperty();
    EXPECT_EQ(&a, &b);
}

TEST(RandomTests, Shared_ProducesValues) {
    int v = System::Random::getSharedProperty().Next(1000);
    EXPECT_GE(v, 0);
    EXPECT_LT(v, 1000);
}

// ---------------------------------------------------------------------------
// Shared — concurrency ownership boundary (CCF-009 / SR-AUD-010, ticket #1902)
//
// getSharedProperty() used to return one process-wide Random whose
// seedArray_/inext_/inextp_ every Next() mutated with no synchronisation;
// ThreadSanitizer reported the race. The shared object now owns no mutable
// state: generator calls made on it route to the calling thread's own
// generator.
//
// The tempting one-word repair -- making the instance itself thread_local --
// is the one .NET rejects explicitly (Random.cs, lines 755-759), because it
// silently gives a caller that obtained Shared on one thread a *different*
// generator when it uses the reference on another. Shared_ReferenceIs*
// and Shared_ReferenceObtainedOnOneThread* below are what make that wrong
// repair fail rather than pass quietly.
// ---------------------------------------------------------------------------

namespace {

    constexpr int kSharedThreads = 8;

    /** @brief Runs @p body on @p threadCount threads, passing each its index. */
    template<typename Body>
    void runOnThreads(int threadCount, Body body) {
        std::vector<std::thread> threads;
        threads.reserve(static_cast<std::size_t>(threadCount));
        for (int t = 0; t < threadCount; ++t) threads.emplace_back(body, t);
        for (auto& th: threads) th.join();
    }

} // namespace

TEST(RandomTests, Shared_ReferenceIsTheSameAddressOnEveryThread) {
    const System::Random* mainAddress = &System::Random::getSharedProperty();

    std::vector<const System::Random*> seen(static_cast<std::size_t>(kSharedThreads), nullptr);
    runOnThreads(kSharedThreads, [&seen](int t) {
        seen[static_cast<std::size_t>(t)] = &System::Random::getSharedProperty();
    });

    for (int t = 0; t < kSharedThreads; ++t)
        EXPECT_EQ(seen[static_cast<std::size_t>(t)], mainAddress) << "thread " << t;
}

TEST(RandomTests, Shared_ReferenceObtainedOnOneThreadStaysUsableOnAnother) {
    // The cross-thread handoff .NET's comment is about: acquire on this thread,
    // draw on another. It must stay valid and produce in-range values.
    System::Random& shared = System::Random::getSharedProperty();

    std::vector<intcs> drawn;
    runOnThreads(1, [&shared, &drawn](int) {
        for (int i = 0; i < 1000; ++i) drawn.push_back(shared.Next(1000));
    });

    ASSERT_EQ(drawn.size(), 1000u);
    for (intcs v: drawn) {
        ASSERT_GE(v, 0);
        ASSERT_LT(v, 1000);
    }
    EXPECT_GT(std::set<intcs>(drawn.begin(), drawn.end()).size(), 1u);
}

TEST(RandomTests, Shared_DistinctThreadsDoNotShareAStream) {
    // Two threads seeded identically would emit identical sequences -- the
    // failure mode a per-thread generator introduces if its seed is not made
    // distinct per thread.
    constexpr int draws = 1000;
    std::vector<std::vector<intcs>> perThread(2);
    runOnThreads(2, [&perThread](int t) {
        auto& out = perThread[static_cast<std::size_t>(t)];
        out.reserve(static_cast<std::size_t>(draws));
        for (int i = 0; i < draws; ++i) out.push_back(System::Random::getSharedProperty().Next());
    });

    ASSERT_EQ(perThread[0].size(), static_cast<std::size_t>(draws));
    ASSERT_EQ(perThread[1].size(), static_cast<std::size_t>(draws));
    EXPECT_NE(perThread[0], perThread[1]);
}

TEST(RandomTests, Shared_ConcurrentDrawsAreInRangeAndVaried) {
    constexpr int draws = 4000;
    std::vector<std::vector<intcs>> perThread(static_cast<std::size_t>(kSharedThreads));
    runOnThreads(kSharedThreads, [&perThread](int t) {
        auto& out = perThread[static_cast<std::size_t>(t)];
        out.reserve(static_cast<std::size_t>(draws));
        for (int i = 0; i < draws; ++i) out.push_back(System::Random::getSharedProperty().Next(1000000));
    });

    std::set<intcs> distinct;
    for (const auto& chunk: perThread)
        for (intcs v: chunk) {
            ASSERT_GE(v, 0);
            ASSERT_LT(v, 1000000);
            distinct.insert(v);
        }
    // Comfortably below the ~2% birthday-collision loss expected for 32,000
    // draws from a million-wide range, but far above what a stuck or shared
    // stream would reach.
    EXPECT_GT(distinct.size(), 20000u);
}

TEST(RandomTests, Shared_ConcurrentNextBytesFillsEveryBuffer) {
    // NextBytes drives internalSample() directly rather than through Next(),
    // so it exercises the ownership boundary on its own path.
    //
    // `std::vector<char>`, NOT `std::vector<bool>` (#2145). This bookkeeping vector used to be
    // `std::vector<bool>`, which is a bit-packed proxy container: eight threads writing
    // `allZero[t] = false` for eight DIFFERENT `t` all write the same underlying word, and
    // [container.requirements.dataraces] exempts `vector<bool>` from the guarantee that distinct
    // elements may be modified concurrently. A lost update then left one thread's entry `true`
    // and failed the assertion -- measured at 2 failures in 30 standalone runs, with the
    // alternative explanation (a thread genuinely receiving 200 x 64 zero bytes from the shared
    // PRNG) astronomically improbable. The defect was in the test's own bookkeeping, not in
    // System::Random. `char` elements are independently addressable, so the race is gone.
    std::vector<char> allZero(static_cast<std::size_t>(kSharedThreads), 1);
    runOnThreads(kSharedThreads, [&allZero](int t) {
        for (int round = 0; round < 200; ++round) {
            std::vector<uint8_t> buffer(64, 0);
            System::Random::getSharedProperty().NextBytes(buffer);
            for (uint8_t b: buffer)
                if (b != 0) { allZero[static_cast<std::size_t>(t)] = 0; break; }
        }
    });
    for (int t = 0; t < kSharedThreads; ++t)
        EXPECT_EQ(allZero[static_cast<std::size_t>(t)], 0) << "thread " << t;
}

TEST(RandomTests, Shared_ConcurrentUseLeavesSeededInstancesUntouched) {
    // The boundary keys on object identity, so a seeded instance must keep its
    // exact stream even while other threads are hammering the shared one. The
    // expected values are the .NET-verified seed-42 stream pinned below.
    std::atomic<bool> stop{false};
    std::vector<std::thread> noise;
    for (int t = 0; t < 4; ++t)
        noise.emplace_back([&stop] {
            while (!stop.load(std::memory_order_relaxed)) System::Random::getSharedProperty().Next();
        });

    for (int repeat = 0; repeat < 100; ++repeat) {
        System::Random rng(42);
        ASSERT_EQ(rng.Next(), 1434747710);
        ASSERT_EQ(rng.Next(), 302596119);
        ASSERT_EQ(rng.Next(), 269548474);
    }

    stop.store(true, std::memory_order_relaxed);
    for (auto& th: noise) th.join();
}

TEST(RandomTests, Shared_DerivedInstancesAreNotAffectedByTheBoundary) {
    // A derived Random can never be the shared instance, so it keeps drawing
    // from its own state and its Sample() override keeps seeing the values it
    // created -- the protected-seam concern
    // docs/SharedPrngConcurrencyPlan.md section 9 flags.
    class CountingRandom : public System::Random {
    public:
        explicit CountingRandom(intcs seed) : System::Random(seed) {}
        int samples = 0;

        /** @brief Reaches the protected seam from the test body. */
        double CallSample() { return Sample(); }

    protected:
        double Sample() override {
            ++samples;
            return System::Random::Sample();
        }
    };

    CountingRandom a(42);
    CountingRandom b(42);
    EXPECT_NE(&a, &System::Random::getSharedProperty());
    for (int i = 0; i < 50; ++i) EXPECT_EQ(a.Next(), b.Next());

    EXPECT_GE(a.CallSample(), 0.0);
    EXPECT_LT(a.CallSample(), 1.0);
    EXPECT_EQ(a.samples, 2);
}

TEST(RandomTests, LayoutIsUnchangedByTheConcurrencyRepair) {
    // Ticket #1902's repair is a pointer comparison inside Random.cpp, not a
    // member: Random's state lives in the public header, so a flag would have
    // been an object-layout change.
    static_assert(sizeof(System::Random) == 240, "Random must stay 240 bytes");
    static_assert(alignof(System::Random) == 8, "Random must stay 8-byte aligned");
    EXPECT_EQ(sizeof(System::Random), 240u);
    EXPECT_EQ(alignof(System::Random), 8u);
}

// ---------------------------------------------------------------------------
// NextBytes(Span)
// ---------------------------------------------------------------------------

TEST(RandomTests, NextBytes_Span_FillsBuffer) {
    System::Random rng(11);
    std::vector<uint8_t> vec(16, 0);
    System::Span<uint8_t> s(vec);
    rng.NextBytes(s);
    bool anyNonZero = false;
    for (auto b : vec) if (b) { anyNonZero = true; break; }
    EXPECT_TRUE(anyNonZero);
}

TEST(RandomTests, NextBytes_Span_LengthUnchanged) {
    System::Random rng(22);
    std::vector<uint8_t> vec(32);
    System::Span<uint8_t> s(vec);
    rng.NextBytes(s);
    EXPECT_EQ(s.getLengthProperty(), 32);
}

// ---------------------------------------------------------------------------
// NextInteger<T>
// ---------------------------------------------------------------------------

TEST(RandomTests, NextInteger_Int_InRange) {
    System::Random rng(33);
    for (int i = 0; i < 100; ++i) {
        int v = rng.NextInteger<int>();
        EXPECT_GE(v, 0);
        EXPECT_LE(v, std::numeric_limits<int>::max());
    }
}

TEST(RandomTests, NextInteger_MaxValue_InRange) {
    System::Random rng(44);
    for (int i = 0; i < 100; ++i) {
        int v = rng.NextInteger<int>(50);
        EXPECT_GE(v, 0);
        EXPECT_LT(v, 50);
    }
}

TEST(RandomTests, NextInteger_Range_InRange) {
    System::Random rng(55);
    for (int i = 0; i < 100; ++i) {
        int v = rng.NextInteger<int>(10, 20);
        EXPECT_GE(v, 10);
        EXPECT_LT(v, 20);
    }
}

TEST(RandomTests, NextInteger_EqualMinMax_ReturnsMin) {
    System::Random rng(66);
    EXPECT_EQ(rng.NextInteger<int>(7, 7), 7);
}

TEST(RandomTests, NextInteger_MaxValueNegative_Throws) {
    System::Random rng(77);
    EXPECT_THROW((rng.NextInteger<int>(-1)), System::ArgumentOutOfRangeException);
}

TEST(RandomTests, NextInteger_InvalidRange_Throws) {
    System::Random rng(88);
    EXPECT_THROW((rng.NextInteger<int>(10, 5)), System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// Shuffle<T>
// ---------------------------------------------------------------------------

TEST(RandomTests, Shuffle_ChangesOrder) {
    System::Random rng(99);
    std::vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> original = v;
    rng.Shuffle(v);
    // All elements must still be present
    std::vector<int> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted, original);
}

TEST(RandomTests, Shuffle_EmptyVector_NoThrow) {
    System::Random rng(100);
    std::vector<int> v;
    EXPECT_NO_THROW(rng.Shuffle(v));
}

TEST(RandomTests, Shuffle_VectorAndSpan_ProduceIdenticalSequenceForSameSeed) {
    // Real .NET's Shuffle(T[]) delegates entirely to Shuffle(Span<T>) -- there is only one
    // shuffle algorithm. Shuffle(std::vector<T>&) previously implemented an independent,
    // opposite-direction loop that produced a genuinely different permutation than
    // Shuffle(Span<T>) for the same seed, breaking this file's seeded-determinism guarantee.
    System::Random rngVec(42);
    std::vector<int> v = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    rngVec.Shuffle(v);

    System::Random rngSpan(42);
    std::vector<int> s = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    rngSpan.Shuffle(System::Span<int>(s));

    EXPECT_EQ(v, s);
}

// ---------------------------------------------------------------------------
// GetItems<T>
// ---------------------------------------------------------------------------

TEST(RandomTests, GetItems_CorrectLength) {
    System::Random rng(111);
    std::vector<int> choices = {10, 20, 30, 40};
    auto result = rng.GetItems(choices, 6);
    EXPECT_EQ(result.size(), 6u);
}

TEST(RandomTests, GetItems_EmptyChoices_Throws) {
    // .NET's GetItems throws ArgumentException for empty choices, even when
    // length == 0 - verified against Random.cs.
    System::Random rng(111);
    std::vector<int> choices;
    EXPECT_THROW(rng.GetItems(choices, 0), System::ArgumentException);
}

TEST(RandomTests, GetItems_NegativeLength_Throws) {
    System::Random rng(111);
    std::vector<int> choices = {1, 2, 3};
    EXPECT_THROW(rng.GetItems(choices, -1), System::ArgumentOutOfRangeException);
}

TEST(RandomTests, GetItems_SpanDestination_EmptyChoices_Throws) {
    System::Random rng(111);
    std::vector<int> empty;
    std::vector<int> dest(3);
    EXPECT_THROW(rng.GetItems(System::ReadOnlySpan<int>(empty.data(), 0), System::Span<int>(dest.data(), 3)), System::ArgumentException);
}

TEST(RandomTests, GetItems_OnlyFromChoices) {
    System::Random rng(222);
    std::vector<int> choices = {10, 20, 30};
    auto result = rng.GetItems(choices, 50);
    for (int v : result)
        EXPECT_TRUE(v == 10 || v == 20 || v == 30);
}

// ---------------------------------------------------------------------------
// GetString
// ---------------------------------------------------------------------------

TEST(RandomTests, GetString_CorrectLength) {
    System::Random rng(333);
    std::string result = rng.GetString("abc", 8);
    EXPECT_EQ(result.size(), 8u);
}

TEST(RandomTests, GetString_OnlyFromChoices) {
    System::Random rng(444);
    std::string result = rng.GetString("xyz", 20);
    for (char c : result)
        EXPECT_TRUE(c == 'x' || c == 'y' || c == 'z');
}

// ---------------------------------------------------------------------------
// GetHexString
// ---------------------------------------------------------------------------

TEST(RandomTests, GetHexString_CorrectLength) {
    System::Random rng(555);
    EXPECT_EQ(rng.GetHexString(8).size(), 8u);
}

TEST(RandomTests, GetHexString_UppercaseByDefault) {
    System::Random rng(666);
    std::string s = rng.GetHexString(100);
    for (char c : s) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'));
    }
}

TEST(RandomTests, GetHexString_Lowercase) {
    System::Random rng(777);
    std::string s = rng.GetHexString(100, true);
    for (char c : s) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

TEST(RandomTests, GetHexString_ZeroLength_Empty) {
    System::Random rng(888);
    EXPECT_TRUE(rng.GetHexString(0).empty());
}

// ---------------------------------------------------------------------------
// NextBinaryFloat<T>
// ---------------------------------------------------------------------------

TEST(RandomTests, NextBinaryFloat_Float_InRange) {
    System::Random rng(10);
    for (int i = 0; i < 100; ++i) {
        float v = rng.NextBinaryFloat<float>();
        EXPECT_GE(v, 0.0f);
        EXPECT_LT(v, 1.0f);
    }
}

TEST(RandomTests, NextBinaryFloat_Double_InRange) {
    System::Random rng(20);
    for (int i = 0; i < 100; ++i) {
        double v = rng.NextBinaryFloat<double>();
        EXPECT_GE(v, 0.0);
        EXPECT_LT(v, 1.0);
    }
}

TEST(RandomTests, NextBinaryFloat_Float_Seeded_Deterministic) {
    System::Random a(30), b(30);
    EXPECT_EQ(a.NextBinaryFloat<float>(), b.NextBinaryFloat<float>());
}

// ---------------------------------------------------------------------------
// GetItems<T>(ReadOnlySpan, Span) — destination overload
// ---------------------------------------------------------------------------

TEST(RandomTests, GetItems_SpanDestination_FillsCorrectly) {
    System::Random rng(40);
    std::vector<int> choices = {1, 2, 3, 4};
    std::vector<int> dest(6, 0);
    System::ReadOnlySpan<int> src(choices);
    System::Span<int> dst(dest);
    rng.GetItems(src, dst);
    for (int v : dest)
        EXPECT_TRUE(v >= 1 && v <= 4);
}

TEST(RandomTests, GetItems_SpanDestination_LengthUnchanged) {
    System::Random rng(50);
    std::vector<int> choices = {10, 20};
    std::vector<int> dest(8);
    rng.GetItems(System::ReadOnlySpan<int>(choices), System::Span<int>(dest));
    EXPECT_EQ(dest.size(), 8u);
}

// ---------------------------------------------------------------------------
// Shuffle<T>(Span<T>)
// ---------------------------------------------------------------------------

TEST(RandomTests, Shuffle_Span_PreservesElements) {
    System::Random rng(60);
    std::vector<int> v = {1, 2, 3, 4, 5};
    std::vector<int> original = v;
    System::Span<int> s(v);
    rng.Shuffle(s);
    std::vector<int> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted, original);
}

TEST(RandomTests, Shuffle_Span_EmptyNoThrow) {
    System::Random rng(70);
    std::vector<int> v;
    EXPECT_NO_THROW(rng.Shuffle(System::Span<int>(v)));
}

// ---------------------------------------------------------------------------
// GetHexString(Span<char>)
// ---------------------------------------------------------------------------

TEST(RandomTests, GetHexString_SpanDestination_FillsUppercase) {
    System::Random rng(80);
    std::vector<char> buf(12, 0);
    System::Span<char> s(buf);
    rng.GetHexString(s);
    for (char c : buf)
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'));
}

TEST(RandomTests, GetHexString_SpanDestination_FillsLowercase) {
    System::Random rng(90);
    std::vector<char> buf(12, 0);
    System::Span<char> s(buf);
    rng.GetHexString(s, true);
    for (char c : buf)
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
}

TEST(RandomTests, GetHexString_SpanDestination_LengthUnchanged) {
    System::Random rng(91);
    std::vector<char> buf(8);
    System::Span<char> s(buf);
    rng.GetHexString(s);
    EXPECT_EQ(s.getLengthProperty(), 8);
}

// ---------------------------------------------------------------------------
// Next — edge cases and throws
// ---------------------------------------------------------------------------

TEST(RandomTests, Next_MaxValueZero_ReturnsZero) {
    System::Random rng(1);
    EXPECT_EQ(rng.Next(0), 0);
}

TEST(RandomTests, Next_MaxValueNegative_Throws) {
    System::Random rng(2);
    EXPECT_THROW(rng.Next(-1), System::ArgumentOutOfRangeException);
}

TEST(RandomTests, Next_EqualMinMax_ReturnsMin) {
    System::Random rng(3);
    EXPECT_EQ(rng.Next(7, 7), 7);
}

TEST(RandomTests, Next_InvalidRange_Throws) {
    System::Random rng(4);
    EXPECT_THROW(rng.Next(10, 5), System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// NextBytes — edge cases
// ---------------------------------------------------------------------------

TEST(RandomTests, NextBytes_EmptyVector_NoThrow) {
    System::Random rng(5);
    std::vector<uint8_t> buf;
    EXPECT_NO_THROW(rng.NextBytes(buf));
}

// ---------------------------------------------------------------------------
// NextDouble — determinism
// ---------------------------------------------------------------------------

TEST(RandomTests, NextDouble_Seeded_Deterministic) {
    System::Random a(12345), b(12345);
    EXPECT_EQ(a.NextDouble(), b.NextDouble());
}

// ---------------------------------------------------------------------------
// GetString — edge cases
// ---------------------------------------------------------------------------

TEST(RandomTests, GetString_ZeroLength_Empty) {
    System::Random rng(6);
    EXPECT_TRUE(rng.GetString("abc", 0).empty());
}

TEST(RandomTests, GetString_EmptyChoices_Throws) {
    // .NET's GetString throws ArgumentException for empty choices, even when
    // length == 0 - verified against Random.cs.
    System::Random rng(6);
    EXPECT_THROW(rng.GetString("", 0), System::ArgumentException);
}

TEST(RandomTests, GetString_NegativeLength_Throws) {
    System::Random rng(6);
    EXPECT_THROW(rng.GetString("abc", -1), System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// Shuffle — single element
// ---------------------------------------------------------------------------

TEST(RandomTests, Shuffle_SingleElement_NoThrow) {
    System::Random rng(7);
    std::vector<int> v = {42};
    EXPECT_NO_THROW(rng.Shuffle(v));
    EXPECT_EQ(v[0], 42);
}

// ---------------------------------------------------------------------------
// GetItems — ReadOnlySpan overload
// ---------------------------------------------------------------------------

TEST(RandomTests, GetItems_ReadOnlySpan_CorrectLength) {
    System::Random rng(111);
    std::vector<int> choices = {10, 20, 30, 40};
    System::ReadOnlySpan<int> src(choices);
    auto result = rng.GetItems(src, 6);
    EXPECT_EQ(result.size(), 6u);
}

TEST(RandomTests, GetItems_ReadOnlySpan_OnlyFromChoices) {
    System::Random rng(222);
    std::vector<int> choices = {10, 20, 30};
    System::ReadOnlySpan<int> src(choices);
    auto result = rng.GetItems(src, 50);
    for (int v : result)
        EXPECT_TRUE(v == 10 || v == 20 || v == 30);
}

// ---------------------------------------------------------------------------
// .NET parity (ticket 64): Random(seed) is now a byte-for-byte port of real
// .NET's own seeded algorithm (the Knuth "subtractive" generator .NET has kept
// bit-stable since .NET Framework 1.0). Every expected value below was captured
// from a live Mono run (System.Private.CoreLib-compatible mscorlib, which uses
// the identical unchanged seeded algorithm) with the exact same seeds -- these
// are not just "same seed -> same output" self-consistency checks like the
// tests above, but actual cross-runtime parity checks against real .NET output.
// ---------------------------------------------------------------------------

TEST(RandomTests, Parity_Next_Seed42) {
    System::Random rng(42);
    EXPECT_EQ(rng.Next(), 1434747710);
    EXPECT_EQ(rng.Next(), 302596119);
    EXPECT_EQ(rng.Next(), 269548474);
    EXPECT_EQ(rng.Next(), 1122627734);
    EXPECT_EQ(rng.Next(), 361709742);
}

TEST(RandomTests, Parity_NextMaxValue_Seed42) {
    System::Random rng(42);
    EXPECT_EQ(rng.Next(100), 66);
    EXPECT_EQ(rng.Next(100), 14);
    EXPECT_EQ(rng.Next(100), 12);
    EXPECT_EQ(rng.Next(100), 52);
    EXPECT_EQ(rng.Next(100), 16);
}

TEST(RandomTests, Parity_NextMinMax_Seed42) {
    System::Random rng(42);
    EXPECT_EQ(rng.Next(10, 20), 16);
    EXPECT_EQ(rng.Next(10, 20), 11);
    EXPECT_EQ(rng.Next(10, 20), 11);
    EXPECT_EQ(rng.Next(10, 20), 15);
    EXPECT_EQ(rng.Next(10, 20), 11);
}

TEST(RandomTests, Parity_NextDouble_Seed42) {
    System::Random rng(42);
    EXPECT_DOUBLE_EQ(rng.NextDouble(), 0.66810646591154232);
    EXPECT_DOUBLE_EQ(rng.NextDouble(), 0.14090729837348093);
    EXPECT_DOUBLE_EQ(rng.NextDouble(), 0.12551828945312568);
}

TEST(RandomTests, Parity_NextBytes_Seed42) {
    System::Random rng(42);
    std::vector<SharpRuntime::bytecs> buf(8);
    rng.NextBytes(buf);
    std::vector<SharpRuntime::bytecs> expected = {0x3E, 0x17, 0xBA, 0x96, 0xAE, 0x04, 0xCD, 0x3B};
    EXPECT_EQ(buf, expected);
}

TEST(RandomTests, Parity_Next_Seed12345) {
    System::Random rng(12345);
    EXPECT_EQ(rng.Next(), 143337951);
    EXPECT_EQ(rng.Next(), 150666398);
    EXPECT_EQ(rng.Next(), 1663795458);
    EXPECT_EQ(rng.Next(), 1097663221);
    EXPECT_EQ(rng.Next(), 1712597933);
}

TEST(RandomTests, Parity_Next_SeedZero) {
    System::Random rng(0);
    EXPECT_EQ(rng.Next(), 1559595546);
    EXPECT_EQ(rng.Next(), 1755192844);
    EXPECT_EQ(rng.Next(), 1649316166);
}

// Verified against real .NET's own seed-normalization: Math.Abs(seed) is used for every
// seed except int.MinValue (where Math.Abs would overflow), which is special-cased to
// int.MaxValue instead -- so this is deliberately a different effective seed from 0, and
// legitimately produces a different sequence from SeedZero above.
TEST(RandomTests, Parity_Next_SeedIntMinValue) {
    System::Random rng(std::numeric_limits<intcs>::min());
    EXPECT_EQ(rng.Next(), 1559595546);
    EXPECT_EQ(rng.Next(), 1755192844);
    EXPECT_EQ(rng.Next(), 1649316172);
}

// Verified against real .NET's Math.Abs(seed) normalization: a negative seed produces the
// exact same sequence as its positive absolute value.
TEST(RandomTests, Parity_Next_NegativeSeed_MatchesAbsoluteValue) {
    System::Random negative(-42);
    System::Random positive(42);
    EXPECT_EQ(negative.Next(), positive.Next());
    EXPECT_EQ(negative.Next(), positive.Next());
    EXPECT_EQ(negative.Next(), positive.Next());
}
