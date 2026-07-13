// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/Generic/Dictionary.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"
#include <string>

using System::Collections::Generic::Dictionary;
using System::Collections::Generic::KeyNotFoundException;

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
    EXPECT_THROW(d.Add("k", 2), System::ArgumentException);
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
    EXPECT_THROW(d["missing"], KeyNotFoundException);
}

// Regression test for POST_STABILIZATION_AUDIT.md finding #3 / ticket 1712: the non-const
// operator[] getter previously silently inserted a default-constructed value on a missing key
// (std::unordered_map::operator[]'s convention) instead of throwing, unlike real .NET's
// Dictionary<TKey,TValue> indexer, which throws KeyNotFoundException on the getter regardless
// of const/non-const.
TEST(DictionaryTest, OperatorBracketNonConstThrowsIfMissing) {
    Dictionary<std::string, int> d;
    EXPECT_THROW((void)static_cast<int>(d["missing"]), KeyNotFoundException);
    // Confirm reading the missing key did NOT silently insert it.
    EXPECT_FALSE(d.ContainsKey("missing"));
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(DictionaryTest, OperatorBracketNonConstGetExistingReturnsValue) {
    Dictionary<std::string, int> d;
    d.Add("present", 7);
    int v = d["present"];
    EXPECT_EQ(v, 7);
}

TEST(DictionaryTest, OperatorBracketNonConstSetMissingStillInserts) {
    Dictionary<std::string, int> d;
    d["newkey"] = 42;
    EXPECT_TRUE(d.ContainsKey("newkey"));
    EXPECT_EQ(static_cast<int>(d["newkey"]), 42);
}

TEST(DictionaryTest, OperatorBracketNonConstSetExistingUpdates) {
    Dictionary<std::string, int> d;
    d.Add("k", 1);
    d["k"] = 2;
    EXPECT_EQ(static_cast<int>(d["k"]), 2);
    EXPECT_EQ(d.getCountProperty(), 1);
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

// Regression for ticket 341: EnsureCapacity previously returned void, but real .NET's
// Dictionary<TKey,TValue>.EnsureCapacity(int) returns the resulting capacity -- added to match
// that signature (return value ported code may capture).
TEST(DictionaryTest, EnsureCapacity_ReturnsResultingCapacity) {
    Dictionary<std::string, int> d;
    SharpRuntime::intcs cap = d.EnsureCapacity(100);
    EXPECT_GE(cap, 100);
}

TEST(DictionaryTest, EnsureCapacity_NegativeThrows) {
    Dictionary<std::string, int> d;
    EXPECT_THROW(d.EnsureCapacity(-1), System::ArgumentOutOfRangeException);
}
