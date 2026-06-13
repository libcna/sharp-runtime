// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

#include "System/Collections/Generic/List.hpp"
#include "System/Collections/Generic/Dictionary.hpp"
#include "System/Collections/Generic/HashSet.hpp"

using System::Collections::Generic::List;
using System::Collections::Generic::Dictionary;
using System::Collections::Generic::HashSet;

// ---------------------------------------------------------------------------
// List<T>
// ---------------------------------------------------------------------------

TEST(ListTests, DefaultCtorIsEmpty) {
    List<int> lst;
    EXPECT_EQ(lst.getCountProperty(), 0);
}

TEST(ListTests, AddIncreasesCount) {
    List<int> lst;
    lst.Add(1);
    lst.Add(2);
    lst.Add(3);
    EXPECT_EQ(lst.getCountProperty(), 3);
}

TEST(ListTests, ContainsAfterAdd) {
    List<std::string> lst;
    lst.Add(std::string("hello"));
    EXPECT_TRUE(lst.Contains(std::string("hello")));
}

TEST(ListTests, ContainsReturnsFalseIfAbsent) {
    List<int> lst;
    lst.Add(1);
    EXPECT_FALSE(lst.Contains(99));
}

TEST(ListTests, OperatorBracketRead) {
    List<int> lst;
    lst.Add(10); lst.Add(20); lst.Add(30);
    EXPECT_EQ(lst[0], 10);
    EXPECT_EQ(lst[1], 20);
    EXPECT_EQ(lst[2], 30);
}

TEST(ListTests, OperatorBracketMutation) {
    List<int> lst;
    lst.Add(0);
    lst[0] = 42;
    EXPECT_EQ(lst[0], 42);
}

TEST(ListTests, RemoveReturnsTrueIfFound) {
    List<int> lst;
    lst.Add(5); lst.Add(10);
    EXPECT_TRUE(lst.Remove(5));
    EXPECT_EQ(lst.getCountProperty(), 1);
    EXPECT_EQ(lst[0], 10);
}

TEST(ListTests, RemoveReturnsFalseIfAbsent) {
    List<int> lst;
    lst.Add(1);
    EXPECT_FALSE(lst.Remove(99));
    EXPECT_EQ(lst.getCountProperty(), 1);
}

TEST(ListTests, RemoveFirstOccurrenceOnly) {
    List<int> lst;
    lst.Add(7); lst.Add(7); lst.Add(7);
    lst.Remove(7);
    EXPECT_EQ(lst.getCountProperty(), 2);
}

TEST(ListTests, IndexOfFound) {
    List<int> lst;
    lst.Add(10); lst.Add(20); lst.Add(30);
    EXPECT_EQ(lst.IndexOf(20), 1);
}

TEST(ListTests, IndexOfNotFound) {
    List<int> lst;
    lst.Add(1); lst.Add(2);
    EXPECT_EQ(lst.IndexOf(99), -1);
}

TEST(ListTests, Insert) {
    List<int> lst;
    lst.Add(1); lst.Add(3);
    lst.Insert(1, 2); // insert 2 between 1 and 3
    EXPECT_EQ(lst.getCountProperty(), 3);
    EXPECT_EQ(lst[0], 1);
    EXPECT_EQ(lst[1], 2);
    EXPECT_EQ(lst[2], 3);
}

TEST(ListTests, RemoveAt) {
    List<int> lst;
    lst.Add(10); lst.Add(20); lst.Add(30);
    lst.RemoveAt(1); // remove 20
    EXPECT_EQ(lst.getCountProperty(), 2);
    EXPECT_EQ(lst[0], 10);
    EXPECT_EQ(lst[1], 30);
}

TEST(ListTests, ClearResetsCount) {
    List<int> lst;
    lst.Add(1); lst.Add(2); lst.Add(3);
    lst.Clear();
    EXPECT_EQ(lst.getCountProperty(), 0);
    EXPECT_FALSE(lst.Contains(1));
}

TEST(ListTests, ToVector) {
    List<int> lst;
    lst.Add(4); lst.Add(5); lst.Add(6);
    const std::vector<int>& v = lst.ToVector();
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 4);
    EXPECT_EQ(v[2], 6);
}

TEST(ListTests, CtorFromVector) {
    std::vector<int> src = {7, 8, 9};
    List<int> lst(src);
    EXPECT_EQ(lst.getCountProperty(), 3);
    EXPECT_EQ(lst[0], 7);
    EXPECT_EQ(lst[2], 9);
}

TEST(ListTests, RangeForIteration) {
    List<int> lst;
    lst.Add(1); lst.Add(2); lst.Add(3);
    int sum = 0;
    for (int x : lst) sum += x;
    EXPECT_EQ(sum, 6);
}

TEST(ListTests, RangeForIterationOrder) {
    List<std::string> lst;
    lst.Add(std::string("a")); lst.Add(std::string("b")); lst.Add(std::string("c"));
    std::vector<std::string> collected;
    for (const auto& s : lst) collected.push_back(s);
    EXPECT_EQ(collected[0], "a");
    EXPECT_EQ(collected[1], "b");
    EXPECT_EQ(collected[2], "c");
}

TEST(ListTests, StringList) {
    List<std::string> lst;
    lst.Add(std::string("foo"));
    lst.Add(std::string("bar"));
    EXPECT_EQ(lst.getCountProperty(), 2);
    EXPECT_EQ(lst[0], "foo");
    EXPECT_EQ(lst[1], "bar");
    EXPECT_TRUE(lst.Contains(std::string("bar")));
    EXPECT_FALSE(lst.Contains(std::string("baz")));
}

// ---------------------------------------------------------------------------
// Dictionary<TKey, TValue>
// ---------------------------------------------------------------------------

TEST(DictionaryTests, DefaultCtorIsEmpty) {
    Dictionary<std::string, int> d;
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(DictionaryTests, AddAndContainsKey) {
    Dictionary<std::string, int> d;
    d.Add(std::string("a"), 1);
    EXPECT_TRUE(d.ContainsKey(std::string("a")));
    EXPECT_EQ(d.getCountProperty(), 1);
}

TEST(DictionaryTests, ContainsKeyReturnsFalseIfAbsent) {
    Dictionary<std::string, int> d;
    d.Add(std::string("x"), 0);
    EXPECT_FALSE(d.ContainsKey(std::string("y")));
}

TEST(DictionaryTests, AddDuplicateThrows) {
    Dictionary<std::string, int> d;
    d.Add(std::string("k"), 1);
    EXPECT_THROW(d.Add(std::string("k"), 2), std::invalid_argument);
}

TEST(DictionaryTests, TryGetValueFound) {
    Dictionary<int, std::string> d;
    d.Add(42, std::string("forty-two"));
    std::string out;
    bool found = d.TryGetValue(42, out);
    EXPECT_TRUE(found);
    EXPECT_EQ(out, "forty-two");
}

TEST(DictionaryTests, TryGetValueNotFound) {
    Dictionary<int, std::string> d;
    std::string out = "unchanged";
    bool found = d.TryGetValue(99, out);
    EXPECT_FALSE(found);
}

TEST(DictionaryTests, OperatorBracketWrite) {
    Dictionary<std::string, int> d;
    d[std::string("key")] = 100;
    EXPECT_TRUE(d.ContainsKey(std::string("key")));
    EXPECT_EQ(d[std::string("key")], 100);
}

TEST(DictionaryTests, OperatorBracketOverwrite) {
    Dictionary<std::string, int> d;
    d.Add(std::string("k"), 1);
    d[std::string("k")] = 2;
    EXPECT_EQ(d[std::string("k")], 2);
    EXPECT_EQ(d.getCountProperty(), 1);
}

TEST(DictionaryTests, ConstOperatorBracketThrowsOnMissing) {
    const Dictionary<std::string, int> d;
    EXPECT_THROW((void)d[std::string("missing")], std::out_of_range);
}

TEST(DictionaryTests, RemoveReturnsTrueIfFound) {
    Dictionary<std::string, int> d;
    d.Add(std::string("k"), 5);
    EXPECT_TRUE(d.Remove(std::string("k")));
    EXPECT_EQ(d.getCountProperty(), 0);
    EXPECT_FALSE(d.ContainsKey(std::string("k")));
}

TEST(DictionaryTests, RemoveReturnsFalseIfAbsent) {
    Dictionary<std::string, int> d;
    EXPECT_FALSE(d.Remove(std::string("nope")));
}

TEST(DictionaryTests, CountAfterMultipleAdds) {
    Dictionary<int, int> d;
    for (int i = 0; i < 10; ++i) d.Add(i, i * i);
    EXPECT_EQ(d.getCountProperty(), 10);
}

TEST(DictionaryTests, ClearResetsCount) {
    Dictionary<int, int> d;
    d.Add(1, 1); d.Add(2, 4); d.Add(3, 9);
    d.Clear();
    EXPECT_EQ(d.getCountProperty(), 0);
    EXPECT_FALSE(d.ContainsKey(1));
}

TEST(DictionaryTests, RangeForIterationCoversAllEntries) {
    Dictionary<int, int> d;
    d.Add(1, 10); d.Add(2, 20); d.Add(3, 30);
    int keySum = 0, valSum = 0;
    for (const auto& [k, v] : d) { keySum += k; valSum += v; }
    EXPECT_EQ(keySum, 6);
    EXPECT_EQ(valSum, 60);
}

TEST(DictionaryTests, IntToStringMap) {
    Dictionary<int, std::string> d;
    d.Add(1, std::string("one"));
    d.Add(2, std::string("two"));
    std::string out;
    EXPECT_TRUE(d.TryGetValue(1, out));
    EXPECT_EQ(out, "one");
    EXPECT_TRUE(d.TryGetValue(2, out));
    EXPECT_EQ(out, "two");
}

// ---------------------------------------------------------------------------
// HashSet<T>
// ---------------------------------------------------------------------------

TEST(HashSetTests, DefaultCtorIsEmpty) {
    HashSet<int> hs;
    EXPECT_EQ(hs.getCountProperty(), 0);
}

TEST(HashSetTests, AddReturnsTrueFirstTime) {
    HashSet<int> hs;
    EXPECT_TRUE(hs.Add(1));
    EXPECT_EQ(hs.getCountProperty(), 1);
}

TEST(HashSetTests, AddReturnsFalseIfDuplicate) {
    HashSet<int> hs;
    hs.Add(1);
    EXPECT_FALSE(hs.Add(1));
    EXPECT_EQ(hs.getCountProperty(), 1);
}

TEST(HashSetTests, ContainsAfterAdd) {
    HashSet<int> hs;
    hs.Add(42);
    EXPECT_TRUE(hs.Contains(42));
}

TEST(HashSetTests, ContainsReturnsFalseIfAbsent) {
    HashSet<int> hs;
    hs.Add(1);
    EXPECT_FALSE(hs.Contains(99));
}

TEST(HashSetTests, RemoveReturnsTrueIfFound) {
    HashSet<int> hs;
    hs.Add(5);
    EXPECT_TRUE(hs.Remove(5));
    EXPECT_EQ(hs.getCountProperty(), 0);
    EXPECT_FALSE(hs.Contains(5));
}

TEST(HashSetTests, RemoveReturnsFalseIfAbsent) {
    HashSet<int> hs;
    EXPECT_FALSE(hs.Remove(99));
}

TEST(HashSetTests, CountAfterAddRemove) {
    HashSet<int> hs;
    hs.Add(1); hs.Add(2); hs.Add(3);
    hs.Remove(2);
    EXPECT_EQ(hs.getCountProperty(), 2);
    EXPECT_TRUE(hs.Contains(1));
    EXPECT_FALSE(hs.Contains(2));
    EXPECT_TRUE(hs.Contains(3));
}

TEST(HashSetTests, ClearResetsCount) {
    HashSet<int> hs;
    hs.Add(1); hs.Add(2); hs.Add(3);
    hs.Clear();
    EXPECT_EQ(hs.getCountProperty(), 0);
    EXPECT_FALSE(hs.Contains(1));
}

TEST(HashSetTests, UnionWith) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2);
    b.Add(2); b.Add(3);
    a.UnionWith(b);
    EXPECT_EQ(a.getCountProperty(), 3);
    EXPECT_TRUE(a.Contains(1));
    EXPECT_TRUE(a.Contains(2));
    EXPECT_TRUE(a.Contains(3));
}

TEST(HashSetTests, UnionWithDisjoint) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2);
    b.Add(3); b.Add(4);
    a.UnionWith(b);
    EXPECT_EQ(a.getCountProperty(), 4);
}

TEST(HashSetTests, IntersectWith) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2); a.Add(3);
    b.Add(2); b.Add(3); b.Add(4);
    a.IntersectWith(b);
    EXPECT_EQ(a.getCountProperty(), 2);
    EXPECT_FALSE(a.Contains(1));
    EXPECT_TRUE(a.Contains(2));
    EXPECT_TRUE(a.Contains(3));
    EXPECT_FALSE(a.Contains(4));
}

TEST(HashSetTests, IntersectWithDisjoint) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2);
    b.Add(3); b.Add(4);
    a.IntersectWith(b);
    EXPECT_EQ(a.getCountProperty(), 0);
}

TEST(HashSetTests, ExceptWith) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2); a.Add(3);
    b.Add(2); b.Add(3);
    a.ExceptWith(b);
    EXPECT_EQ(a.getCountProperty(), 1);
    EXPECT_TRUE(a.Contains(1));
    EXPECT_FALSE(a.Contains(2));
    EXPECT_FALSE(a.Contains(3));
}

TEST(HashSetTests, ExceptWithNoOverlap) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2);
    b.Add(3);
    a.ExceptWith(b);
    EXPECT_EQ(a.getCountProperty(), 2);
}

TEST(HashSetTests, ToArray) {
    HashSet<int> hs;
    hs.Add(10); hs.Add(20); hs.Add(30);
    std::vector<int> arr = hs.ToArray();
    EXPECT_EQ(arr.size(), 3u);
    // order is unspecified; just verify all elements are present
    std::sort(arr.begin(), arr.end());
    EXPECT_EQ(arr[0], 10);
    EXPECT_EQ(arr[1], 20);
    EXPECT_EQ(arr[2], 30);
}

TEST(HashSetTests, RangeForIteration) {
    HashSet<int> hs;
    hs.Add(1); hs.Add(2); hs.Add(3);
    int sum = 0;
    for (int x : hs) sum += x;
    EXPECT_EQ(sum, 6);
}

TEST(HashSetTests, StringHashSet) {
    HashSet<std::string> hs;
    EXPECT_TRUE(hs.Add(std::string("alpha")));
    EXPECT_TRUE(hs.Add(std::string("beta")));
    EXPECT_FALSE(hs.Add(std::string("alpha")));
    EXPECT_EQ(hs.getCountProperty(), 2);
    EXPECT_TRUE(hs.Contains(std::string("beta")));
    hs.Remove(std::string("beta"));
    EXPECT_FALSE(hs.Contains(std::string("beta")));
}

// ---------------------------------------------------------------------------
// List<T> — new methods (session 57)
// ---------------------------------------------------------------------------

TEST(ListTests, Sort_DefaultOrder) {
    List<int> lst;
    lst.Add(3); lst.Add(1); lst.Add(4); lst.Add(1); lst.Add(5);
    lst.Sort();
    EXPECT_EQ(lst[0], 1);
    EXPECT_EQ(lst[4], 5);
}

TEST(ListTests, Sort_WithComparison_Descending) {
    List<int> lst;
    lst.Add(3); lst.Add(1); lst.Add(2);
    lst.Sort([](const int& a, const int& b) { return b - a; });
    EXPECT_EQ(lst[0], 3);
    EXPECT_EQ(lst[2], 1);
}

TEST(ListTests, Reverse_ReversesOrder) {
    List<int> lst;
    lst.Add(1); lst.Add(2); lst.Add(3);
    lst.Reverse();
    EXPECT_EQ(lst[0], 3);
    EXPECT_EQ(lst[2], 1);
}

TEST(ListTests, AddRange_FromVector) {
    List<int> lst;
    lst.Add(1);
    lst.AddRange(std::vector<int>{2, 3, 4});
    EXPECT_EQ(lst.getCountProperty(), 4);
    EXPECT_EQ(lst[3], 4);
}

TEST(ListTests, AddRange_FromList) {
    List<int> a, b;
    a.Add(1); a.Add(2);
    b.Add(3); b.Add(4);
    a.AddRange(b);
    EXPECT_EQ(a.getCountProperty(), 4);
    EXPECT_EQ(a[3], 4);
}

TEST(ListTests, InsertRange) {
    List<int> lst;
    lst.Add(1); lst.Add(4);
    lst.InsertRange(1, std::vector<int>{2, 3});
    EXPECT_EQ(lst.getCountProperty(), 4);
    EXPECT_EQ(lst[1], 2);
    EXPECT_EQ(lst[2], 3);
}

TEST(ListTests, GetRange) {
    List<int> lst;
    for (int i = 0; i < 5; ++i) lst.Add(i);
    auto sub = lst.GetRange(1, 3);
    EXPECT_EQ(sub.getCountProperty(), 3);
    EXPECT_EQ(sub[0], 1);
    EXPECT_EQ(sub[2], 3);
}

TEST(ListTests, ToArray_ReturnsCopy) {
    List<int> lst;
    lst.Add(10); lst.Add(20);
    auto arr = lst.ToArray();
    EXPECT_EQ(arr.size(), 2u);
    EXPECT_EQ(arr[1], 20);
}

TEST(ListTests, Find_Found) {
    List<int> lst;
    lst.Add(1); lst.Add(2); lst.Add(3);
    EXPECT_EQ(lst.Find([](const int& x) { return x > 1; }), 2);
}

TEST(ListTests, Find_NotFound_ReturnsDefault) {
    List<int> lst;
    lst.Add(1); lst.Add(2);
    EXPECT_EQ(lst.Find([](const int& x) { return x > 10; }), 0);
}

TEST(ListTests, FindAll) {
    List<int> lst;
    lst.Add(1); lst.Add(2); lst.Add(3); lst.Add(4);
    auto evens = lst.FindAll([](const int& x) { return x % 2 == 0; });
    EXPECT_EQ(evens.getCountProperty(), 2);
    EXPECT_EQ(evens[0], 2);
    EXPECT_EQ(evens[1], 4);
}

TEST(ListTests, FindIndex_Found) {
    List<int> lst;
    lst.Add(10); lst.Add(20); lst.Add(30);
    EXPECT_EQ(lst.FindIndex([](const int& x) { return x == 20; }), 1);
}

TEST(ListTests, FindIndex_NotFound) {
    List<int> lst;
    lst.Add(1); lst.Add(2);
    EXPECT_EQ(lst.FindIndex([](const int& x) { return x == 99; }), -1);
}

TEST(ListTests, FindLastIndex) {
    List<int> lst;
    lst.Add(1); lst.Add(2); lst.Add(2); lst.Add(3);
    EXPECT_EQ(lst.FindLastIndex([](const int& x) { return x == 2; }), 2);
}

TEST(ListTests, RemoveAll_RemovesMatching) {
    List<int> lst;
    lst.Add(1); lst.Add(2); lst.Add(3); lst.Add(4);
    int removed = lst.RemoveAll([](const int& x) { return x % 2 == 0; });
    EXPECT_EQ(removed, 2);
    EXPECT_EQ(lst.getCountProperty(), 2);
    EXPECT_EQ(lst[0], 1);
    EXPECT_EQ(lst[1], 3);
}

TEST(ListTests, ForEach_VisitsAll) {
    List<int> lst;
    lst.Add(1); lst.Add(2); lst.Add(3);
    int sum = 0;
    lst.ForEach([&sum](const int& x) { sum += x; });
    EXPECT_EQ(sum, 6);
}

TEST(ListTests, Exists_True) {
    List<int> lst;
    lst.Add(1); lst.Add(5); lst.Add(3);
    EXPECT_TRUE(lst.Exists([](const int& x) { return x == 5; }));
}

TEST(ListTests, Exists_False) {
    List<int> lst;
    lst.Add(1); lst.Add(2);
    EXPECT_FALSE(lst.Exists([](const int& x) { return x == 99; }));
}

TEST(ListTests, TrueForAll_True) {
    List<int> lst;
    lst.Add(2); lst.Add(4); lst.Add(6);
    EXPECT_TRUE(lst.TrueForAll([](const int& x) { return x % 2 == 0; }));
}

TEST(ListTests, TrueForAll_False) {
    List<int> lst;
    lst.Add(2); lst.Add(3);
    EXPECT_FALSE(lst.TrueForAll([](const int& x) { return x % 2 == 0; }));
}

TEST(ListTests, BinarySearch_Found) {
    List<int> lst;
    lst.Add(1); lst.Add(2); lst.Add(3); lst.Add(4); lst.Add(5);
    EXPECT_EQ(lst.BinarySearch(3), 2);
}

TEST(ListTests, BinarySearch_NotFound_NegativeResult) {
    List<int> lst;
    lst.Add(1); lst.Add(3); lst.Add(5);
    EXPECT_LT(lst.BinarySearch(2), 0);
}

// ---------------------------------------------------------------------------
// Dictionary<K,V> — new methods (session 58)
// ---------------------------------------------------------------------------

TEST(DictionaryTests, TryAdd_AddsWhenAbsent) {
    Dictionary<std::string, int> d;
    EXPECT_TRUE(d.TryAdd("x", 1));
    EXPECT_EQ(d["x"], 1);
}

TEST(DictionaryTests, TryAdd_ReturnsFalseWhenPresent) {
    Dictionary<std::string, int> d;
    d.Add("x", 1);
    EXPECT_FALSE(d.TryAdd("x", 99));
    EXPECT_EQ(d["x"], 1);
}

TEST(DictionaryTests, ContainsValue_Found) {
    Dictionary<std::string, int> d;
    d.Add("a", 42);
    EXPECT_TRUE(d.ContainsValue(42));
}

TEST(DictionaryTests, ContainsValue_NotFound) {
    Dictionary<std::string, int> d;
    d.Add("a", 1);
    EXPECT_FALSE(d.ContainsValue(99));
}

TEST(DictionaryTests, GetValueOrDefault_Present) {
    Dictionary<std::string, int> d;
    d.Add("k", 7);
    EXPECT_EQ(d.GetValueOrDefault("k", 0), 7);
}

TEST(DictionaryTests, GetValueOrDefault_Absent) {
    Dictionary<std::string, int> d;
    EXPECT_EQ(d.GetValueOrDefault("missing", 42), 42);
}

TEST(DictionaryTests, GetValueOrDefault_AbsentDefaultDefault) {
    Dictionary<std::string, int> d;
    EXPECT_EQ(d.GetValueOrDefault("missing"), 0);
}

TEST(DictionaryTests, Keys_ContainsAllKeys) {
    Dictionary<std::string, int> d;
    d.Add("a", 1); d.Add("b", 2);
    auto keys = d.getKeysProperty();
    EXPECT_EQ(keys.size(), 2u);
    bool hasA = std::find(keys.begin(), keys.end(), "a") != keys.end();
    bool hasB = std::find(keys.begin(), keys.end(), "b") != keys.end();
    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasB);
}

TEST(DictionaryTests, Values_ContainsAllValues) {
    Dictionary<std::string, int> d;
    d.Add("a", 10); d.Add("b", 20);
    auto vals = d.getValuesProperty();
    EXPECT_EQ(vals.size(), 2u);
    bool has10 = std::find(vals.begin(), vals.end(), 10) != vals.end();
    bool has20 = std::find(vals.begin(), vals.end(), 20) != vals.end();
    EXPECT_TRUE(has10);
    EXPECT_TRUE(has20);
}
