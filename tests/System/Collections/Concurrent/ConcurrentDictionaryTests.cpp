// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/Concurrent/ConcurrentDictionary.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"
#include <string>

using System::Collections::Concurrent::ConcurrentDictionary;

TEST(ConcurrentDictionaryTest, TryAddAndCount) {
    ConcurrentDictionary<std::string, int> d;
    EXPECT_TRUE(d.TryAdd("a", 1));
    EXPECT_EQ(d.getCountProperty(), 1);
}

TEST(ConcurrentDictionaryTest, TryAddDuplicateReturnsFalse) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("x", 1);
    EXPECT_FALSE(d.TryAdd("x", 2));
}

TEST(ConcurrentDictionaryTest, TryGetValue) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("k", 42);
    int v = 0;
    EXPECT_TRUE(d.TryGetValue("k", v));
    EXPECT_EQ(v, 42);
}

TEST(ConcurrentDictionaryTest, TryGetValueMissing) {
    ConcurrentDictionary<std::string, int> d;
    int v = 0;
    EXPECT_FALSE(d.TryGetValue("missing", v));
}

TEST(ConcurrentDictionaryTest, TryRemove) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("r", 99);
    int v = 0;
    EXPECT_TRUE(d.TryRemove("r", v));
    EXPECT_EQ(v, 99);
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(ConcurrentDictionaryTest, ContainsKey) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("yes", 1);
    EXPECT_TRUE(d.ContainsKey("yes"));
    EXPECT_FALSE(d.ContainsKey("no"));
}

TEST(ConcurrentDictionaryTest, GetOrAdd) {
    ConcurrentDictionary<std::string, int> d;
    int v = d.GetOrAdd("key", 7);
    EXPECT_EQ(v, 7);
    int v2 = d.GetOrAdd("key", 99);
    EXPECT_EQ(v2, 7);
}

TEST(ConcurrentDictionaryTest, TryUpdate) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("u", 10);
    EXPECT_TRUE(d.TryUpdate("u", 20, 10));
    int v = 0;
    d.TryGetValue("u", v);
    EXPECT_EQ(v, 20);
}

TEST(ConcurrentDictionaryTest, AddOrUpdate) {
    ConcurrentDictionary<std::string, int> d;
    d.AddOrUpdate("au", 1, [](const std::string&, int old){ return old + 1; });
    d.AddOrUpdate("au", 1, [](const std::string&, int old){ return old + 1; });
    int v = 0;
    d.TryGetValue("au", v);
    EXPECT_EQ(v, 2);
}

TEST(ConcurrentDictionaryTest, Clear) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("a", 1);
    d.Clear();
    EXPECT_TRUE(d.getIsEmptyProperty());
}

TEST(ConcurrentDictionaryTest, Keys) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("a", 1);
    d.TryAdd("b", 2);
    auto keys = d.getKeysProperty();
    EXPECT_EQ(static_cast<int>(keys.size()), 2);
}

TEST(ConcurrentDictionaryTest, AddOrUpdate_WithAddFactory_NewKey_Adds) {
    ConcurrentDictionary<std::string, int> d;
    int result = d.AddOrUpdate("a", [](const std::string&) { return 5; },
                                [](const std::string&, int v) { return v + 1; });
    EXPECT_EQ(result, 5);
}

TEST(ConcurrentDictionaryTest, AddOrUpdate_WithAddFactory_ExistingKey_Updates) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("a", 5);
    int result = d.AddOrUpdate("a", [](const std::string&) { return 100; },
                                [](const std::string&, int v) { return v + 1; });
    EXPECT_EQ(result, 6);
}

// .NET's ConcurrentDictionary<TKey,TValue> indexer getter throws KeyNotFoundException for
// an absent key (ConcurrentDictionary.cs:1069-1072) -- it does NOT insert a default value,
// unlike std::unordered_map::operator[] or this type's own prior (buggy) implementation.
TEST(ConcurrentDictionaryTest, Indexer_MissingKey_Throws) {
    ConcurrentDictionary<std::string, int> d;
    EXPECT_THROW({ int v = d["missing"]; (void)v; }, System::Collections::Generic::KeyNotFoundException);
    EXPECT_TRUE(d.getIsEmptyProperty());
}

TEST(ConcurrentDictionaryTest, Indexer_Get_ExistingKey_ReturnsValue) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("a", 42);
    int v = d["a"];
    EXPECT_EQ(v, 42);
}

TEST(ConcurrentDictionaryTest, Indexer_Set_NewKey_Inserts) {
    ConcurrentDictionary<std::string, int> d;
    d["a"] = 7;
    EXPECT_EQ(d.getCountProperty(), 1);
    EXPECT_EQ(static_cast<int>(d["a"]), 7);
}

TEST(ConcurrentDictionaryTest, Indexer_Set_ExistingKey_Overwrites) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("a", 1);
    d["a"] = 2;
    EXPECT_EQ(d.getCountProperty(), 1);
    EXPECT_EQ(static_cast<int>(d["a"]), 2);
}
