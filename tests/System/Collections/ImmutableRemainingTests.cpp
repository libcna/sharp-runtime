// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for remaining Immutable collections:
// ImmutableHashSet, ImmutableQueue, ImmutableSortedDictionary,
// ImmutableSortedSet, ImmutableStack.
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "System/Collections/Immutable/ImmutableHashSet.hpp"
#include "System/Collections/Immutable/ImmutableQueue.hpp"
#include "System/Collections/Immutable/ImmutableSortedDictionary.hpp"
#include "System/Collections/Immutable/ImmutableSortedSet.hpp"
#include "System/Collections/Immutable/ImmutableStack.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/ArgumentException.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"

using System::Collections::Immutable::ImmutableHashSet;
using System::Collections::Immutable::ImmutableQueue;
using System::Collections::Immutable::ImmutableSortedDictionary;
using System::Collections::Immutable::ImmutableSortedSet;
using System::Collections::Immutable::ImmutableStack;

// ===========================================================================
// ImmutableHashSet<T>
// ===========================================================================

TEST(ImmutableHashSetTests, Empty_IsEmptyAndCountZero) {
    auto s = ImmutableHashSet<int>::Empty();
    EXPECT_TRUE(s.getIsEmptyProperty());
    EXPECT_EQ(s.getCountProperty(), 0);
}

TEST(ImmutableHashSetTests, Create_WithItems_CountMatches) {
    auto s = ImmutableHashSet<int>::Create({1, 2, 3});
    EXPECT_EQ(s.getCountProperty(), 3);
    EXPECT_FALSE(s.getIsEmptyProperty());
}

TEST(ImmutableHashSetTests, Contains_True_False) {
    auto s = ImmutableHashSet<int>::Create({10, 20, 30});
    EXPECT_TRUE(s.Contains(20));
    EXPECT_FALSE(s.Contains(99));
}

TEST(ImmutableHashSetTests, Add_ReturnsNewSetWithItem) {
    auto s = ImmutableHashSet<int>::Empty();
    auto s2 = s.Add(42);
    EXPECT_EQ(s.getCountProperty(), 0);
    EXPECT_EQ(s2.getCountProperty(), 1);
    EXPECT_TRUE(s2.Contains(42));
}

TEST(ImmutableHashSetTests, Add_DuplicateNotIncreased) {
    auto s = ImmutableHashSet<int>::Create({5});
    auto s2 = s.Add(5);
    EXPECT_EQ(s2.getCountProperty(), 1);
}

TEST(ImmutableHashSetTests, Remove_ReturnsNewSetWithoutItem) {
    auto s = ImmutableHashSet<int>::Create({1, 2, 3});
    auto s2 = s.Remove(2);
    EXPECT_EQ(s.getCountProperty(), 3);
    EXPECT_EQ(s2.getCountProperty(), 2);
    EXPECT_FALSE(s2.Contains(2));
}

TEST(ImmutableHashSetTests, Remove_NonExistent_SameCount) {
    auto s = ImmutableHashSet<int>::Create({1, 2});
    auto s2 = s.Remove(99);
    EXPECT_EQ(s2.getCountProperty(), 2);
}

TEST(ImmutableHashSetTests, Clear_ReturnsEmpty) {
    auto s = ImmutableHashSet<int>::Create({1, 2, 3});
    auto empty = s.Clear();
    EXPECT_EQ(empty.getCountProperty(), 0);
    EXPECT_TRUE(empty.getIsEmptyProperty());
}

TEST(ImmutableHashSetTests, Union_CombinesBothSets) {
    auto a = ImmutableHashSet<int>::Create({1, 2});
    auto b = ImmutableHashSet<int>::Create({2, 3});
    auto u = a.Union(b);
    EXPECT_EQ(u.getCountProperty(), 3);
    EXPECT_TRUE(u.Contains(1));
    EXPECT_TRUE(u.Contains(2));
    EXPECT_TRUE(u.Contains(3));
}

TEST(ImmutableHashSetTests, Intersect_OnlyCommonItems) {
    auto a = ImmutableHashSet<int>::Create({1, 2, 3});
    auto b = ImmutableHashSet<int>::Create({2, 3, 4});
    auto inter = a.Intersect(b);
    EXPECT_EQ(inter.getCountProperty(), 2);
    EXPECT_TRUE(inter.Contains(2));
    EXPECT_TRUE(inter.Contains(3));
    EXPECT_FALSE(inter.Contains(1));
}

TEST(ImmutableHashSetTests, Except_RemovesOtherItems) {
    auto a = ImmutableHashSet<int>::Create({1, 2, 3});
    auto b = ImmutableHashSet<int>::Create({2, 3});
    auto ex = a.Except(b);
    EXPECT_EQ(ex.getCountProperty(), 1);
    EXPECT_TRUE(ex.Contains(1));
}

TEST(ImmutableHashSetTests, IsSubsetOf_True_False) {
    auto sub = ImmutableHashSet<int>::Create({2, 3});
    auto sup = ImmutableHashSet<int>::Create({1, 2, 3, 4});
    EXPECT_TRUE(sub.IsSubsetOf(sup));
    EXPECT_FALSE(sup.IsSubsetOf(sub));
}

TEST(ImmutableHashSetTests, IsSubsetOf_EmptyIsSubsetOfAny) {
    auto empty = ImmutableHashSet<int>::Empty();
    auto s = ImmutableHashSet<int>::Create({1, 2});
    EXPECT_TRUE(empty.IsSubsetOf(s));
}

// ===========================================================================
// ImmutableQueue<T>
// ===========================================================================

TEST(ImmutableQueueTests, Empty_IsEmptyAndCountZero) {
    auto q = ImmutableQueue<int>::Empty();
    EXPECT_TRUE(q.getIsEmptyProperty());
    EXPECT_EQ(q.getCountProperty(), 0);
}

TEST(ImmutableQueueTests, Enqueue_ReturnsNewQueueWithItem) {
    auto q = ImmutableQueue<int>::Empty();
    auto q2 = q.Enqueue(10);
    EXPECT_EQ(q.getCountProperty(), 0);
    EXPECT_EQ(q2.getCountProperty(), 1);
}

TEST(ImmutableQueueTests, Enqueue_MultipleItems_FIFOOrder) {
    auto q = ImmutableQueue<int>::Empty()
                 .Enqueue(1).Enqueue(2).Enqueue(3);
    EXPECT_EQ(q.getCountProperty(), 3);
    EXPECT_EQ(q.Peek(), 1);
}

TEST(ImmutableQueueTests, Peek_ReturnsFront) {
    auto q = ImmutableQueue<int>::Empty().Enqueue(7).Enqueue(8);
    EXPECT_EQ(q.Peek(), 7);
}

TEST(ImmutableQueueTests, Peek_EmptyQueue_Throws) {
    auto q = ImmutableQueue<int>::Empty();
    EXPECT_THROW(q.Peek(), System::InvalidOperationException);
}

TEST(ImmutableQueueTests, Dequeue_ReturnsNewQueueWithoutFront) {
    auto q = ImmutableQueue<int>::Empty().Enqueue(1).Enqueue(2).Enqueue(3);
    auto q2 = q.Dequeue();
    EXPECT_EQ(q.getCountProperty(), 3);
    EXPECT_EQ(q2.getCountProperty(), 2);
    EXPECT_EQ(q2.Peek(), 2);
}

TEST(ImmutableQueueTests, Dequeue_EmptyQueue_Throws) {
    auto q = ImmutableQueue<int>::Empty();
    EXPECT_THROW(q.Dequeue(), System::InvalidOperationException);
}

TEST(ImmutableQueueTests, DequeueWithValue_FillsValue) {
    auto q = ImmutableQueue<int>::Empty().Enqueue(42).Enqueue(99);
    int val = 0;
    auto q2 = q.Dequeue(val);
    EXPECT_EQ(val, 42);
    EXPECT_EQ(q2.getCountProperty(), 1);
}

TEST(ImmutableQueueTests, Clear_ReturnsEmpty) {
    auto q = ImmutableQueue<int>::Empty().Enqueue(1).Enqueue(2);
    auto empty = q.Clear();
    EXPECT_TRUE(empty.getIsEmptyProperty());
}

// ===========================================================================
// ImmutableSortedDictionary<TKey, TValue>
// ===========================================================================

TEST(ImmutableSortedDictionaryTests, Empty_IsEmptyAndCountZero) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty();
    EXPECT_TRUE(d.getIsEmptyProperty());
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(ImmutableSortedDictionaryTests, Add_IncreasesCount) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty()
                 .Add("a", 1).Add("b", 2);
    EXPECT_EQ(d.getCountProperty(), 2);
}

TEST(ImmutableSortedDictionaryTests, Add_DuplicateKey_Throws) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty().Add("x", 10);
    EXPECT_THROW(d.Add("x", 20), System::ArgumentException);
}

TEST(ImmutableSortedDictionaryTests, ContainsKey_True_False) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty().Add("key", 42);
    EXPECT_TRUE(d.ContainsKey("key"));
    EXPECT_FALSE(d.ContainsKey("missing"));
}

TEST(ImmutableSortedDictionaryTests, OperatorBracket_ReturnsValue) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty().Add("k", 99);
    EXPECT_EQ(d["k"], 99);
}

TEST(ImmutableSortedDictionaryTests, OperatorBracket_MissingKey_Throws) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty();
    EXPECT_THROW((void)d["nope"], System::Collections::Generic::KeyNotFoundException);
}

TEST(ImmutableSortedDictionaryTests, TryGetValue_Found) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty().Add("z", 55);
    int v = 0;
    EXPECT_TRUE(d.TryGetValue("z", v));
    EXPECT_EQ(v, 55);
}

TEST(ImmutableSortedDictionaryTests, TryGetValue_NotFound_ReturnsFalse) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty();
    int v = 0;
    EXPECT_FALSE(d.TryGetValue("missing", v));
}

TEST(ImmutableSortedDictionaryTests, SetItem_OverwritesExistingKey) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty()
                 .Add("a", 1).SetItem("a", 100);
    EXPECT_EQ(d["a"], 100);
    EXPECT_EQ(d.getCountProperty(), 1);
}

TEST(ImmutableSortedDictionaryTests, Remove_DecreaseCount) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty()
                 .Add("a", 1).Add("b", 2);
    auto d2 = d.Remove("a");
    EXPECT_EQ(d.getCountProperty(), 2);
    EXPECT_EQ(d2.getCountProperty(), 1);
    EXPECT_FALSE(d2.ContainsKey("a"));
}

TEST(ImmutableSortedDictionaryTests, Clear_ReturnsEmpty) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty()
                 .Add("a", 1).Add("b", 2);
    auto empty = d.Clear();
    EXPECT_EQ(empty.getCountProperty(), 0);
}

TEST(ImmutableSortedDictionaryTests, Keys_AreSorted) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty()
                 .Add("c", 3).Add("a", 1).Add("b", 2);
    auto keys = d.getKeysProperty();
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], "a");
    EXPECT_EQ(keys[1], "b");
    EXPECT_EQ(keys[2], "c");
}

TEST(ImmutableSortedDictionaryTests, Values_InKeyOrder) {
    auto d = ImmutableSortedDictionary<std::string, int>::Empty()
                 .Add("c", 3).Add("a", 1).Add("b", 2);
    auto vals = d.getValuesProperty();
    ASSERT_EQ(vals.size(), 3u);
    EXPECT_EQ(vals[0], 1);
    EXPECT_EQ(vals[1], 2);
    EXPECT_EQ(vals[2], 3);
}

// ===========================================================================
// ImmutableSortedSet<T>
// ===========================================================================

TEST(ImmutableSortedSetTests, Empty_IsEmptyAndCountZero) {
    auto s = ImmutableSortedSet<int>::Empty();
    EXPECT_TRUE(s.getIsEmptyProperty());
    EXPECT_EQ(s.getCountProperty(), 0);
}

TEST(ImmutableSortedSetTests, Create_CountMatches) {
    auto s = ImmutableSortedSet<int>::Create({3, 1, 2});
    EXPECT_EQ(s.getCountProperty(), 3);
}

TEST(ImmutableSortedSetTests, Contains_True_False) {
    auto s = ImmutableSortedSet<int>::Create({10, 20, 30});
    EXPECT_TRUE(s.Contains(20));
    EXPECT_FALSE(s.Contains(99));
}

TEST(ImmutableSortedSetTests, Add_ReturnsNewSetWithItem) {
    auto s = ImmutableSortedSet<int>::Empty();
    auto s2 = s.Add(5);
    EXPECT_EQ(s.getCountProperty(), 0);
    EXPECT_EQ(s2.getCountProperty(), 1);
    EXPECT_TRUE(s2.Contains(5));
}

TEST(ImmutableSortedSetTests, Add_DuplicateNotIncreased) {
    auto s = ImmutableSortedSet<int>::Create({7});
    auto s2 = s.Add(7);
    EXPECT_EQ(s2.getCountProperty(), 1);
}

TEST(ImmutableSortedSetTests, Remove_ReturnsNewSetWithoutItem) {
    auto s = ImmutableSortedSet<int>::Create({1, 2, 3});
    auto s2 = s.Remove(2);
    EXPECT_EQ(s2.getCountProperty(), 2);
    EXPECT_FALSE(s2.Contains(2));
}

TEST(ImmutableSortedSetTests, Clear_ReturnsEmpty) {
    auto s = ImmutableSortedSet<int>::Create({1, 2});
    EXPECT_EQ(s.Clear().getCountProperty(), 0);
}

TEST(ImmutableSortedSetTests, Min_Max) {
    auto s = ImmutableSortedSet<int>::Create({5, 1, 9, 3});
    EXPECT_EQ(s.getMinProperty(), 1);
    EXPECT_EQ(s.getMaxProperty(), 9);
}

TEST(ImmutableSortedSetTests, Min_Max_Empty_ReturnsDefault) {
    auto s = ImmutableSortedSet<int>::Empty();
    EXPECT_EQ(s.getMinProperty(), 0);
    EXPECT_EQ(s.getMaxProperty(), 0);
}

TEST(ImmutableSortedSetTests, Union_CombinesBothSets) {
    auto a = ImmutableSortedSet<int>::Create({1, 2});
    auto b = ImmutableSortedSet<int>::Create({2, 3});
    auto u = a.Union(b);
    EXPECT_EQ(u.getCountProperty(), 3);
}

TEST(ImmutableSortedSetTests, Intersect_OnlyCommonItems) {
    auto a = ImmutableSortedSet<int>::Create({1, 2, 3});
    auto b = ImmutableSortedSet<int>::Create({2, 3, 4});
    auto inter = a.Intersect(b);
    EXPECT_EQ(inter.getCountProperty(), 2);
    EXPECT_TRUE(inter.Contains(2));
    EXPECT_TRUE(inter.Contains(3));
}

TEST(ImmutableSortedSetTests, Except_RemovesOtherItems) {
    auto a = ImmutableSortedSet<int>::Create({1, 2, 3});
    auto b = ImmutableSortedSet<int>::Create({2, 3});
    auto ex = a.Except(b);
    EXPECT_EQ(ex.getCountProperty(), 1);
    EXPECT_TRUE(ex.Contains(1));
}

TEST(ImmutableSortedSetTests, IsSubsetOf_True_False) {
    auto sub = ImmutableSortedSet<int>::Create({2, 3});
    auto sup = ImmutableSortedSet<int>::Create({1, 2, 3, 4});
    EXPECT_TRUE(sub.IsSubsetOf(sup));
    EXPECT_FALSE(sup.IsSubsetOf(sub));
}

TEST(ImmutableSortedSetTests, Iteration_IsSorted) {
    auto s = ImmutableSortedSet<int>::Create({5, 3, 1, 4, 2});
    std::vector<int> items(s.begin(), s.end());
    ASSERT_EQ(items.size(), 5u);
    for (int i = 0; i < 4; ++i)
        EXPECT_LT(items[i], items[i + 1]);
}

// ===========================================================================
// ImmutableStack<T>
// ===========================================================================

TEST(ImmutableStackTests, Empty_IsEmptyAndCountZero) {
    auto st = ImmutableStack<int>::Empty();
    EXPECT_TRUE(st.getIsEmptyProperty());
    EXPECT_EQ(st.getCountProperty(), 0);
}

TEST(ImmutableStackTests, Push_ReturnsNewStackWithItem) {
    auto st = ImmutableStack<int>::Empty();
    auto st2 = st.Push(10);
    EXPECT_EQ(st.getCountProperty(), 0);
    EXPECT_EQ(st2.getCountProperty(), 1);
}

TEST(ImmutableStackTests, Push_MultipleItems_LIFOOrder) {
    auto st = ImmutableStack<int>::Empty().Push(1).Push(2).Push(3);
    EXPECT_EQ(st.Peek(), 3);
}

TEST(ImmutableStackTests, Peek_ReturnsTop) {
    auto st = ImmutableStack<int>::Empty().Push(7).Push(8);
    EXPECT_EQ(st.Peek(), 8);
}

TEST(ImmutableStackTests, Peek_EmptyStack_Throws) {
    auto st = ImmutableStack<int>::Empty();
    EXPECT_THROW(st.Peek(), System::InvalidOperationException);
}

TEST(ImmutableStackTests, Pop_ReturnsNewStackWithoutTop) {
    auto st = ImmutableStack<int>::Empty().Push(1).Push(2).Push(3);
    auto st2 = st.Pop();
    EXPECT_EQ(st.getCountProperty(), 3);
    EXPECT_EQ(st2.getCountProperty(), 2);
    EXPECT_EQ(st2.Peek(), 2);
}

TEST(ImmutableStackTests, Pop_EmptyStack_Throws) {
    auto st = ImmutableStack<int>::Empty();
    EXPECT_THROW(st.Pop(), System::InvalidOperationException);
}

TEST(ImmutableStackTests, PopWithValue_FillsValue) {
    auto st = ImmutableStack<int>::Empty().Push(10).Push(20);
    int val = 0;
    auto st2 = st.Pop(val);
    EXPECT_EQ(val, 20);
    EXPECT_EQ(st2.getCountProperty(), 1);
    EXPECT_EQ(st2.Peek(), 10);
}

TEST(ImmutableStackTests, PopWithValue_EmptyStack_Throws) {
    auto st = ImmutableStack<int>::Empty();
    int val = 0;
    EXPECT_THROW(st.Pop(val), System::InvalidOperationException);
}

TEST(ImmutableStackTests, Clear_ReturnsEmpty) {
    auto st = ImmutableStack<int>::Empty().Push(1).Push(2);
    auto empty = st.Clear();
    EXPECT_TRUE(empty.getIsEmptyProperty());
    EXPECT_EQ(empty.getCountProperty(), 0);
}

TEST(ImmutableStackTests, Iteration_TopToBottom) {
    auto st = ImmutableStack<int>::Empty().Push(1).Push(2).Push(3);
    std::vector<int> items(st.begin(), st.end());
    ASSERT_EQ(items.size(), 3u);
    EXPECT_EQ(items[0], 3);
    EXPECT_EQ(items[1], 2);
    EXPECT_EQ(items[2], 1);
}

TEST(ImmutableStackTests, OriginalUnchangedAfterPush) {
    auto st = ImmutableStack<int>::Empty().Push(5);
    auto st2 = st.Push(10);
    EXPECT_EQ(st.getCountProperty(), 1);
    EXPECT_EQ(st2.getCountProperty(), 2);
}
