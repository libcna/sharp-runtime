// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/Generic/Dictionary.hpp"
#include <string>

using System::Collections::Generic::Dictionary;

TEST(DictionaryTest, DefaultEmpty) {
    Dictionary<std::string, int> d;
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(DictionaryTest, AddAndContainsKey) {
    Dictionary<std::string, int> d;
    d.Add("a", 1);
    EXPECT_TRUE(d.ContainsKey("a"));
    EXPECT_EQ(d.getCountProperty(), 1);
}

TEST(DictionaryTest, AddDuplicateThrows) {
    Dictionary<std::string, int> d;
    d.Add("k", 1);
    EXPECT_THROW(d.Add("k", 2), std::invalid_argument);
}

TEST(DictionaryTest, TryGetValueFound) {
    Dictionary<std::string, int> d;
    d.Add("x", 42);
    int v = 0;
    EXPECT_TRUE(d.TryGetValue("x", v));
    EXPECT_EQ(v, 42);
}

TEST(DictionaryTest, TryGetValueNotFound) {
    Dictionary<std::string, int> d;
    int v = 99;
    EXPECT_FALSE(d.TryGetValue("missing", v));
    EXPECT_EQ(v, 99);
}

TEST(DictionaryTest, Remove) {
    Dictionary<std::string, int> d;
    d.Add("r", 5);
    EXPECT_TRUE(d.Remove("r"));
    EXPECT_FALSE(d.ContainsKey("r"));
}

TEST(DictionaryTest, RemoveWithValue) {
    Dictionary<std::string, int> d;
    d.Add("r", 77);
    int v = 0;
    EXPECT_TRUE(d.Remove("r", v));
    EXPECT_EQ(v, 77);
}

TEST(DictionaryTest, Clear) {
    Dictionary<std::string, int> d;
    d.Add("a", 1);
    d.Add("b", 2);
    d.Clear();
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(DictionaryTest, OperatorBracketReadWrite) {
    Dictionary<std::string, int> d;
    d["key"] = 100;
    EXPECT_EQ(d["key"], 100);
}

TEST(DictionaryTest, OperatorBracketConstThrowsIfMissing) {
    const Dictionary<std::string, int> d;
    EXPECT_THROW(d["missing"], std::out_of_range);
}

TEST(DictionaryTest, TryAdd) {
    Dictionary<std::string, int> d;
    EXPECT_TRUE(d.TryAdd("k", 1));
    EXPECT_FALSE(d.TryAdd("k", 2));
    EXPECT_EQ(d["k"], 1);
}

TEST(DictionaryTest, ContainsValue) {
    Dictionary<std::string, int> d;
    d.Add("a", 10);
    EXPECT_TRUE(d.ContainsValue(10));
    EXPECT_FALSE(d.ContainsValue(99));
}

TEST(DictionaryTest, GetValueOrDefault) {
    Dictionary<std::string, int> d;
    d.Add("x", 5);
    EXPECT_EQ(d.GetValueOrDefault("x"), 5);
    EXPECT_EQ(d.GetValueOrDefault("missing", 42), 42);
}

TEST(DictionaryTest, GetKeysAndValues) {
    Dictionary<std::string, int> d;
    d.Add("a", 1);
    d.Add("b", 2);
    EXPECT_EQ(static_cast<int>(d.getKeysProperty().size()), 2);
    EXPECT_EQ(static_cast<int>(d.getValuesProperty().size()), 2);
}

TEST(DictionaryTest, RangeBasedFor) {
    Dictionary<std::string, int> d;
    d.Add("a", 1);
    d.Add("b", 2);
    int sum = 0;
    for (const auto& kv : d) sum += kv.second;
    EXPECT_EQ(sum, 3);
}

TEST(DictionaryTest, EnsureCapacityAndTrimExcess) {
    Dictionary<std::string, int> d;
    d.EnsureCapacity(100);
    d.Add("x", 1);
    d.TrimExcess();
    EXPECT_EQ(d.getCountProperty(), 1);
}
