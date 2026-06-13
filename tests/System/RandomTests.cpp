#include <iostream>

#include "gtest/gtest.h"
#include "System/Random.hpp"

using SharpRuntime::intcs;
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
