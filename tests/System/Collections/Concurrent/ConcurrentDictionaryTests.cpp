// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/Concurrent/ConcurrentDictionary.hpp"
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
    auto keys = d.Keys();
    EXPECT_EQ(static_cast<int>(keys.size()), 2);
}
