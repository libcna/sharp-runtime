#include <iostream>

#include "gtest/gtest.h"
#include "System/Random.hpp"
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
