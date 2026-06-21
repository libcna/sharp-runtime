// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Lazy.hpp"

using System::Lazy;
using System::LazyThreadSafetyMode;

TEST(LazyTests, DefaultCtor_NotCreated) {
    Lazy<int> lz;
    EXPECT_FALSE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, DefaultCtor_AccessCreatesDefault) {
    Lazy<int> lz;
    EXPECT_EQ(lz.getValueProperty(), 0);
    EXPECT_TRUE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, PreInitCtor_IsValueCreatedTrue) {
    Lazy<int> lz(42);
    EXPECT_TRUE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, PreInitCtor_ValueCorrect) {
    Lazy<int> lz(42);
    EXPECT_EQ(lz.getValueProperty(), 42);
}

TEST(LazyTests, FactoryCtor_NotCreatedBeforeAccess) {
    int calls = 0;
    Lazy<int> lz([&] { ++calls; return 99; });
    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, FactoryCtor_ValueCorrectOnAccess) {
    Lazy<int> lz([] { return 7; });
    EXPECT_EQ(lz.getValueProperty(), 7);
}

TEST(LazyTests, FactoryCtor_FactoryCalledOnce) {
    int calls = 0;
    Lazy<int> lz([&] { ++calls; return 1; });
    lz.getValueProperty();
    lz.getValueProperty();
    EXPECT_EQ(calls, 1);
}

TEST(LazyTests, BoolCtor_ThreadSafe_DefaultConstruct) {
    Lazy<std::string> lz(true);
    EXPECT_FALSE(lz.getIsValueCreatedProperty());
    EXPECT_EQ(lz.getValueProperty(), "");
}

TEST(LazyTests, ModeCtor_DefaultConstruct) {
    Lazy<double> lz(LazyThreadSafetyMode::None);
    EXPECT_EQ(lz.getValueProperty(), 0.0);
}

TEST(LazyTests, FactoryBoolCtor_WorksCorrectly) {
    Lazy<int> lz([] { return 55; }, false);
    EXPECT_EQ(lz.getValueProperty(), 55);
}

TEST(LazyTests, FactoryModeCtor_WorksCorrectly) {
    Lazy<int> lz([] { return 33; }, LazyThreadSafetyMode::None);
    EXPECT_EQ(lz.getValueProperty(), 33);
}

TEST(LazyTests, IsValueCreated_AfterAccess_True) {
    Lazy<int> lz([] { return 1; });
    lz.getValueProperty();
    EXPECT_TRUE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, Value_Alias_SameAsGetValueProperty) {
    Lazy<int> lz([] { return 21; });
    EXPECT_EQ(lz.Value(), lz.getValueProperty());
}

TEST(LazyTests, ToString_BeforeAccess) {
    Lazy<int> lz([] { return 0; });
    EXPECT_NE(lz.ToString().find("not"), std::string::npos);
}

TEST(LazyTests, ToString_AfterAccess) {
    Lazy<int> lz([] { return 0; });
    lz.getValueProperty();
    EXPECT_EQ(lz.ToString().find("not"), std::string::npos);
}

TEST(LazyTests, GetMode_DefaultIsSafe) {
    Lazy<int> lz;
    EXPECT_EQ(lz.getModeProperty(), LazyThreadSafetyMode::ExecutionAndPublication);
}
