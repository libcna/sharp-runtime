// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include "System/Collections/Immutable/ImmutableArray.hpp"
#include "System/Collections/Immutable/ImmutableDictionary.hpp"
#include "System/Collections/Immutable/ImmutableList.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"
#include "System/IndexOutOfRangeException.hpp"

using System::Collections::Immutable::ImmutableArray;
using System::Collections::Immutable::ImmutableDictionary;
using System::Collections::Immutable::ImmutableList;
using System::Collections::Generic::KeyNotFoundException;

// ---------------------------------------------------------------------------
// ImmutableArray<int>
// ---------------------------------------------------------------------------

TEST(ImmutableCollectionTests, ArrayEmptyIsEmpty) {
    auto arr = ImmutableArray<int>::Empty();
    EXPECT_EQ(arr.getLengthProperty(), 0);
    EXPECT_TRUE(arr.getIsEmptyProperty());
}

TEST(ImmutableCollectionTests, ArrayCreateFromInitList) {
    auto arr = ImmutableArray<int>::Create({1, 2, 3});
    EXPECT_EQ(arr.getLengthProperty(), 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
}

TEST(ImmutableCollectionTests, ArrayAddReturnsNewInstance) {
    auto a = ImmutableArray<int>::Create({1, 2});
    auto b = a.Add(3);

    // original unchanged
    EXPECT_EQ(a.getLengthProperty(), 2);
    // new instance has the added element
    EXPECT_EQ(b.getLengthProperty(), 3);
    EXPECT_EQ(b[2], 3);
}

TEST(ImmutableCollectionTests, ArrayAddRangeReturnsNewInstance) {
    auto a = ImmutableArray<int>::Create({1});
    auto b = a.AddRange({2, 3, 4});

    EXPECT_EQ(a.getLengthProperty(), 1);
    EXPECT_EQ(b.getLengthProperty(), 4);
    EXPECT_EQ(b[3], 4);
}

TEST(ImmutableCollectionTests, ArrayRemoveReturnsNewInstance) {
    auto a = ImmutableArray<int>::Create({1, 2, 3});
    auto b = a.Remove(2);

    EXPECT_EQ(a.getLengthProperty(), 3);
    EXPECT_EQ(b.getLengthProperty(), 2);
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[1], 3);
}

TEST(ImmutableCollectionTests, ArrayRemoveAtReturnsNewInstance) {
    auto a = ImmutableArray<int>::Create({10, 20, 30});
    auto b = a.RemoveAt(1);

    EXPECT_EQ(a.getLengthProperty(), 3);
    EXPECT_EQ(a[1], 20); // original unchanged
    EXPECT_EQ(b.getLengthProperty(), 2);
    EXPECT_EQ(b[0], 10);
    EXPECT_EQ(b[1], 30);
}

TEST(ImmutableCollectionTests, ArraySetItemReturnsNewInstance) {
    auto a = ImmutableArray<int>::Create({1, 2, 3});
    auto b = a.SetItem(1, 99);

    EXPECT_EQ(a[1], 2);  // original unchanged
    EXPECT_EQ(b[1], 99);
    EXPECT_EQ(b.getLengthProperty(), 3);
}

TEST(ImmutableCollectionTests, ArrayInsertReturnsNewInstance) {
    auto a = ImmutableArray<int>::Create({1, 3});
    auto b = a.Insert(1, 2);

    EXPECT_EQ(a.getLengthProperty(), 2);
    EXPECT_EQ(b.getLengthProperty(), 3);
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[1], 2);
    EXPECT_EQ(b[2], 3);
}

TEST(ImmutableCollectionTests, ArraySort) {
    auto a = ImmutableArray<int>::Create({3, 1, 2});
    auto b = a.Sort();

    EXPECT_EQ(a[0], 3); // original unchanged
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[1], 2);
    EXPECT_EQ(b[2], 3);
}

TEST(ImmutableCollectionTests, ArrayContains) {
    auto arr = ImmutableArray<int>::Create({10, 20, 30});
    EXPECT_TRUE(arr.Contains(20));
    EXPECT_FALSE(arr.Contains(99));
}

TEST(ImmutableCollectionTests, ArrayIndexOf) {
    auto arr = ImmutableArray<int>::Create({10, 20, 30});
    EXPECT_EQ(arr.IndexOf(20), 1);
    EXPECT_EQ(arr.IndexOf(99), -1);
}

TEST(ImmutableCollectionTests, ArrayRangeFor) {
    auto arr = ImmutableArray<int>::Create({1, 2, 3});
    int sum = 0;
    for (int v : arr) sum += v;
    EXPECT_EQ(sum, 6);
}

TEST(ImmutableCollectionTests, ArrayOutOfRangeThrows) {
    auto arr = ImmutableArray<int>::Create({1, 2});
    EXPECT_THROW(arr[5], System::IndexOutOfRangeException);
}

// ---------------------------------------------------------------------------
// ImmutableList<int>
// ---------------------------------------------------------------------------

TEST(ImmutableCollectionTests, ListEmptyIsEmpty) {
    auto lst = ImmutableList<int>::Empty();
    EXPECT_EQ(lst.getCountProperty(), 0);
    EXPECT_TRUE(lst.getIsEmptyProperty());
}

TEST(ImmutableCollectionTests, ListCreateFromInitList) {
    auto lst = ImmutableList<int>::Create({4, 5, 6});
    EXPECT_EQ(lst.getCountProperty(), 3);
    EXPECT_EQ(lst[0], 4);
    EXPECT_EQ(lst[2], 6);
}

TEST(ImmutableCollectionTests, ListAddReturnsNewInstance) {
    auto a = ImmutableList<int>::Create({1, 2});
    auto b = a.Add(3);

    EXPECT_EQ(a.getCountProperty(), 2);
    EXPECT_EQ(b.getCountProperty(), 3);
    EXPECT_EQ(b[2], 3);
}

TEST(ImmutableCollectionTests, ListRemoveReturnsNewInstance) {
    auto a = ImmutableList<int>::Create({1, 2, 3});
    auto b = a.Remove(2);

    EXPECT_EQ(a.getCountProperty(), 3);
    EXPECT_EQ(b.getCountProperty(), 2);
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[1], 3);
}

TEST(ImmutableCollectionTests, ListRemoveAtReturnsNewInstance) {
    auto a = ImmutableList<int>::Create({10, 20, 30});
    auto b = a.RemoveAt(0);

    EXPECT_EQ(a.getCountProperty(), 3);
    EXPECT_EQ(b.getCountProperty(), 2);
    EXPECT_EQ(b[0], 20);
}

TEST(ImmutableCollectionTests, ListSetItemReturnsNewInstance) {
    auto a = ImmutableList<int>::Create({1, 2, 3});
    auto b = a.SetItem(2, 99);

    EXPECT_EQ(a[2], 3);  // original unchanged
    EXPECT_EQ(b[2], 99);
}

TEST(ImmutableCollectionTests, ListInsertReturnsNewInstance) {
    auto a = ImmutableList<int>::Create({1, 3});
    auto b = a.Insert(1, 2);

    EXPECT_EQ(a.getCountProperty(), 2);
    EXPECT_EQ(b.getCountProperty(), 3);
    EXPECT_EQ(b[1], 2);
}

TEST(ImmutableCollectionTests, ListContainsAndIndexOf) {
    auto lst = ImmutableList<int>::Create({10, 20, 30});
    EXPECT_TRUE(lst.Contains(10));
    EXPECT_FALSE(lst.Contains(99));
    EXPECT_EQ(lst.IndexOf(30), 2);
    EXPECT_EQ(lst.IndexOf(99), -1);
}

TEST(ImmutableCollectionTests, ListRangeFor) {
    auto lst = ImmutableList<int>::Create({1, 2, 3, 4});
    int sum = 0;
    for (int v : lst) sum += v;
    EXPECT_EQ(sum, 10);
}

TEST(ImmutableCollectionTests, ListIndexerOutOfRangeThrows) {
    auto lst = ImmutableList<int>::Create({1, 2});
    EXPECT_THROW(lst[5], System::ArgumentOutOfRangeException);
    EXPECT_THROW(lst[-1], System::ArgumentOutOfRangeException);
}

TEST(ImmutableCollectionTests, ListInsertOutOfRangeThrows) {
    auto lst = ImmutableList<int>::Create({1, 2});
    EXPECT_THROW(lst.Insert(5, 99), System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW(lst.Insert(2, 99)); // append at Count is valid
}

TEST(ImmutableCollectionTests, ListRemoveAtOutOfRangeThrows) {
    auto lst = ImmutableList<int>::Create({1, 2});
    EXPECT_THROW(lst.RemoveAt(5), System::ArgumentOutOfRangeException);
}

TEST(ImmutableCollectionTests, ListRemoveRangeInvalidThrows) {
    auto lst = ImmutableList<int>::Create({1, 2, 3});
    EXPECT_THROW(lst.RemoveRange(2, 5), System::ArgumentException);
}

// ---------------------------------------------------------------------------
// ImmutableDictionary<string, int>
// ---------------------------------------------------------------------------

TEST(ImmutableCollectionTests, DictEmptyIsEmpty) {
    auto d = ImmutableDictionary<std::string, int>::Empty();
    EXPECT_EQ(d.getCountProperty(), 0);
    EXPECT_TRUE(d.getIsEmptyProperty());
}

TEST(ImmutableCollectionTests, DictAddReturnsNewInstance) {
    auto a = ImmutableDictionary<std::string, int>::Empty();
    auto b = a.Add("x", 1);

    EXPECT_EQ(a.getCountProperty(), 0);  // original unchanged
    EXPECT_EQ(b.getCountProperty(), 1);
    EXPECT_TRUE(b.ContainsKey("x"));
    EXPECT_EQ(b["x"], 1);
}

TEST(ImmutableCollectionTests, DictAddDuplicateKeyThrows) {
    auto d = ImmutableDictionary<std::string, int>::Empty().Add("k", 1);
    EXPECT_THROW(d.Add("k", 2), System::ArgumentException);
}

TEST(ImmutableCollectionTests, DictSetItemReturnsNewInstance) {
    auto a = ImmutableDictionary<std::string, int>::Empty().Add("k", 1);
    auto b = a.SetItem("k", 99);

    EXPECT_EQ(a["k"], 1);   // original unchanged
    EXPECT_EQ(b["k"], 99);
    EXPECT_EQ(b.getCountProperty(), 1);
}

TEST(ImmutableCollectionTests, DictSetItemAddsIfAbsent) {
    auto a = ImmutableDictionary<std::string, int>::Empty();
    auto b = a.SetItem("new", 42);

    EXPECT_EQ(a.getCountProperty(), 0);
    EXPECT_EQ(b.getCountProperty(), 1);
    EXPECT_EQ(b["new"], 42);
}

TEST(ImmutableCollectionTests, DictRemoveReturnsNewInstance) {
    auto a = ImmutableDictionary<std::string, int>::Empty()
                 .Add("a", 1).Add("b", 2);
    auto b = a.Remove("a");

    EXPECT_EQ(a.getCountProperty(), 2);  // original unchanged
    EXPECT_EQ(b.getCountProperty(), 1);
    EXPECT_FALSE(b.ContainsKey("a"));
    EXPECT_TRUE(b.ContainsKey("b"));
}

TEST(ImmutableCollectionTests, DictRemoveMissingKeyIsNoOp) {
    auto a = ImmutableDictionary<std::string, int>::Empty().Add("k", 1);
    auto b = a.Remove("nope");

    EXPECT_EQ(b.getCountProperty(), 1);
    EXPECT_TRUE(b.ContainsKey("k"));
}

TEST(ImmutableCollectionTests, DictTryGetValue) {
    auto d = ImmutableDictionary<std::string, int>::Empty().Add("k", 42);
    int val = 0;
    EXPECT_TRUE(d.TryGetValue("k", val));
    EXPECT_EQ(val, 42);
    EXPECT_FALSE(d.TryGetValue("missing", val));
}

TEST(ImmutableCollectionTests, DictMissingKeyThrows) {
    auto d = ImmutableDictionary<std::string, int>::Empty();
    EXPECT_THROW(d["nope"], KeyNotFoundException);
}

TEST(ImmutableCollectionTests, DictClearReturnsEmpty) {
    auto a = ImmutableDictionary<std::string, int>::Empty()
                 .Add("x", 1).Add("y", 2);
    auto b = a.Clear();

    EXPECT_EQ(a.getCountProperty(), 2);  // original unchanged
    EXPECT_EQ(b.getCountProperty(), 0);
    EXPECT_TRUE(b.getIsEmptyProperty());
}

TEST(ImmutableCollectionTests, DictMultipleAdds) {
    auto d = ImmutableDictionary<std::string, int>::Empty()
                 .Add("one", 1).Add("two", 2).Add("three", 3);

    EXPECT_EQ(d.getCountProperty(), 3);
    EXPECT_EQ(d["one"], 1);
    EXPECT_EQ(d["two"], 2);
    EXPECT_EQ(d["three"], 3);
}
