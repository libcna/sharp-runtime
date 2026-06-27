// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/Generic/SortedSet.hpp"
#include "System/Collections/Generic/SortedDictionary.hpp"
#include "System/Collections/Generic/SortedList.hpp"
#include "System/Collections/Generic/Stack.hpp"
#include <string>
#include <vector>

using namespace System::Collections::Generic;

// ---- SortedSet<int> (new Overlaps + existing coverage) ----
TEST(GenSortedSetTest, OverlapsTrue) {
    SortedSet<int> a{1, 2, 3};
    SortedSet<int> b{3, 4, 5};
    EXPECT_TRUE(a.Overlaps(b));
}

TEST(GenSortedSetTest, OverlapsFalse) {
    SortedSet<int> a{1, 2};
    SortedSet<int> b{3, 4};
    EXPECT_FALSE(a.Overlaps(b));
}

TEST(GenSortedSetTest, AddAndContains) {
    SortedSet<int> s;
    EXPECT_TRUE(s.Add(5));
    EXPECT_FALSE(s.Add(5));
    EXPECT_TRUE(s.Contains(5));
}

TEST(GenSortedSetTest, MinMax) {
    SortedSet<int> s{3, 1, 4, 1, 5};
    EXPECT_EQ(s.getMinProperty(), 1);
    EXPECT_EQ(s.getMaxProperty(), 5);
}

TEST(GenSortedSetTest, GetViewBetween) {
    SortedSet<int> s{1,2,3,4,5};
    auto view = s.GetViewBetween(2, 4);
    EXPECT_EQ(view.getCountProperty(), 3);
    EXPECT_TRUE(view.Contains(2));
    EXPECT_TRUE(view.Contains(4));
    EXPECT_FALSE(view.Contains(1));
}

// ---- SortedDictionary<string,int> ----
TEST(GenSortedDictionaryTest, AddAndContainsKey) {
    SortedDictionary<std::string, int> d;
    d.Add("b", 2); d.Add("a", 1);
    EXPECT_TRUE(d.ContainsKey("a"));
    EXPECT_FALSE(d.ContainsKey("z"));
}

TEST(GenSortedDictionaryTest, KeysAreSorted) {
    SortedDictionary<std::string, int> d;
    d.Add("c", 3); d.Add("a", 1); d.Add("b", 2);
    auto keys = d.getKeysProperty();
    EXPECT_EQ(keys[0], "a");
    EXPECT_EQ(keys[2], "c");
}

TEST(GenSortedDictionaryTest, TryGetValue) {
    SortedDictionary<std::string, int> d;
    d.Add("x", 42);
    int v = 0;
    EXPECT_TRUE(d.TryGetValue("x", v));
    EXPECT_EQ(v, 42);
    EXPECT_FALSE(d.TryGetValue("missing", v));
}

TEST(GenSortedDictionaryTest, TryAdd) {
    SortedDictionary<std::string, int> d;
    EXPECT_TRUE(d.TryAdd("k", 1));
    EXPECT_FALSE(d.TryAdd("k", 2));
}

TEST(GenSortedDictionaryTest, Remove) {
    SortedDictionary<std::string, int> d;
    d.Add("a", 1);
    EXPECT_TRUE(d.Remove("a"));
    EXPECT_FALSE(d.ContainsKey("a"));
}

TEST(GenSortedDictionaryTest, ContainsValue) {
    SortedDictionary<std::string, int> d;
    d.Add("a", 99);
    EXPECT_TRUE(d.ContainsValue(99));
    EXPECT_FALSE(d.ContainsValue(0));
}

TEST(GenSortedDictionaryTest, GetValueOrDefault) {
    SortedDictionary<std::string, int> d;
    d.Add("a", 7);
    EXPECT_EQ(d.GetValueOrDefault("a", 0), 7);
    EXPECT_EQ(d.GetValueOrDefault("missing", -1), -1);
}

TEST(GenSortedDictionaryTest, Clear) {
    SortedDictionary<std::string, int> d;
    d.Add("a", 1);
    d.Clear();
    EXPECT_EQ(d.getCountProperty(), 0);
}

// ---- SortedList<string,int> ----
TEST(GenSortedListTest, AddAndContainsKey) {
    SortedList<std::string, int> sl;
    sl.Add("b", 2); sl.Add("a", 1);
    EXPECT_TRUE(sl.ContainsKey("a"));
    EXPECT_FALSE(sl.ContainsKey("z"));
}

TEST(GenSortedListTest, IndexOfKey) {
    SortedList<std::string, int> sl;
    sl.Add("c", 3); sl.Add("a", 1); sl.Add("b", 2);
    EXPECT_EQ(sl.IndexOfKey("a"), 0);
    EXPECT_EQ(sl.IndexOfKey("c"), 2);
    EXPECT_EQ(sl.IndexOfKey("z"), -1);
}

TEST(GenSortedListTest, IndexOfValue) {
    SortedList<std::string, int> sl;
    sl.Add("a", 10); sl.Add("b", 20);
    EXPECT_EQ(sl.IndexOfValue(10), 0);
    EXPECT_EQ(sl.IndexOfValue(20), 1);
    EXPECT_EQ(sl.IndexOfValue(99), -1);
}

TEST(GenSortedListTest, RemoveAt) {
    SortedList<std::string, int> sl;
    sl.Add("a", 1); sl.Add("b", 2);
    sl.RemoveAt(0);
    EXPECT_EQ(sl.getCountProperty(), 1);
    EXPECT_FALSE(sl.ContainsKey("a"));
}

TEST(GenSortedListTest, TryGetValue) {
    SortedList<std::string, int> sl;
    sl.Add("k", 99);
    int v = 0;
    EXPECT_TRUE(sl.TryGetValue("k", v));
    EXPECT_EQ(v, 99);
}

TEST(GenSortedListTest, ContainsValue) {
    SortedList<std::string, int> sl;
    sl.Add("a", 42);
    EXPECT_TRUE(sl.ContainsValue(42));
    EXPECT_FALSE(sl.ContainsValue(0));
}

TEST(GenSortedListTest, KeysAndValuesSorted) {
    SortedList<std::string, int> sl;
    sl.Add("c", 3); sl.Add("a", 1); sl.Add("b", 2);
    auto keys = sl.getKeysProperty();
    EXPECT_EQ(keys[0], "a");
    EXPECT_EQ(keys[2], "c");
}

// ---- Stack<int> ----
TEST(GenStackTest, PushPop) {
    Stack<int> s;
    s.Push(1); s.Push(2); s.Push(3);
    EXPECT_EQ(s.Pop(), 3);
    EXPECT_EQ(s.Pop(), 2);
    EXPECT_EQ(s.getCountProperty(), 1);
}

TEST(GenStackTest, Peek) {
    Stack<int> s;
    s.Push(42);
    EXPECT_EQ(s.Peek(), 42);
    EXPECT_EQ(s.getCountProperty(), 1);
}

TEST(GenStackTest, TryPop) {
    Stack<int> s;
    s.Push(7);
    int v = 0;
    EXPECT_TRUE(s.TryPop(v));
    EXPECT_EQ(v, 7);
    EXPECT_FALSE(s.TryPop(v));
}

TEST(GenStackTest, TryPeek) {
    Stack<int> s;
    int v = 0;
    EXPECT_FALSE(s.TryPeek(v));
    s.Push(99);
    EXPECT_TRUE(s.TryPeek(v));
    EXPECT_EQ(v, 99);
}

TEST(GenStackTest, Contains) {
    Stack<int> s;
    s.Push(1); s.Push(2);
    EXPECT_TRUE(s.Contains(2));
    EXPECT_FALSE(s.Contains(99));
}

TEST(GenStackTest, ToArray) {
    Stack<int> s;
    s.Push(1); s.Push(2); s.Push(3);
    auto v = s.ToArray();
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 3); // top first
}

TEST(GenStackTest, Clear) {
    Stack<int> s;
    s.Push(1); s.Push(2);
    s.Clear();
    EXPECT_EQ(s.getCountProperty(), 0);
}

TEST(GenStackTest, PopEmptyThrows) {
    Stack<int> s;
    EXPECT_THROW(s.Pop(), std::runtime_error);
}
