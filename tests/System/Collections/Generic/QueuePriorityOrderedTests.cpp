// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/Generic/Queue.hpp"
#include "System/Collections/Generic/PriorityQueue.hpp"
#include "System/Collections/Generic/OrderedDictionary.hpp"
#include "System/Collections/Generic/ReferenceEqualityComparer.hpp"
#include <string>
#include <vector>

using namespace System::Collections::Generic;

// ---- Queue<int> ----
TEST(GenQueueTest, EnqueueDequeue) {
    Queue<int> q;
    q.Enqueue(1); q.Enqueue(2); q.Enqueue(3);
    EXPECT_EQ(q.Dequeue(), 1);
    EXPECT_EQ(q.Dequeue(), 2);
    EXPECT_EQ(q.getCountProperty(), 1);
}

TEST(GenQueueTest, Peek) {
    Queue<int> q;
    q.Enqueue(42);
    EXPECT_EQ(q.Peek(), 42);
    EXPECT_EQ(q.getCountProperty(), 1);
}

TEST(GenQueueTest, TryDequeue) {
    Queue<int> q;
    q.Enqueue(7);
    int v = 0;
    EXPECT_TRUE(q.TryDequeue(v));
    EXPECT_EQ(v, 7);
    EXPECT_FALSE(q.TryDequeue(v));
}

TEST(GenQueueTest, TryPeek) {
    Queue<int> q;
    int v = 0;
    EXPECT_FALSE(q.TryPeek(v));
    q.Enqueue(99);
    EXPECT_TRUE(q.TryPeek(v));
    EXPECT_EQ(v, 99);
    EXPECT_EQ(q.getCountProperty(), 1);
}

TEST(GenQueueTest, Contains) {
    Queue<int> q;
    q.Enqueue(1); q.Enqueue(2);
    EXPECT_TRUE(q.Contains(2));
    EXPECT_FALSE(q.Contains(99));
}

TEST(GenQueueTest, Clear) {
    Queue<int> q;
    q.Enqueue(1); q.Enqueue(2);
    q.Clear();
    EXPECT_EQ(q.getCountProperty(), 0);
}

TEST(GenQueueTest, ToArray) {
    Queue<int> q;
    q.Enqueue(10); q.Enqueue(20); q.Enqueue(30);
    auto v = q.ToArray();
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 10);
    EXPECT_EQ(v[2], 30);
}

TEST(GenQueueTest, DequeueEmptyThrows) {
    Queue<int> q;
    EXPECT_THROW(q.Dequeue(), std::runtime_error);
}

// ---- PriorityQueue<string,int> ----
TEST(GenPriorityQueueTest, EnqueueDequeueMinFirst) {
    PriorityQueue<std::string, int> pq;
    pq.Enqueue("low", 10);
    pq.Enqueue("high", 1);
    pq.Enqueue("mid", 5);
    EXPECT_EQ(pq.Dequeue(), "high");
    EXPECT_EQ(pq.Dequeue(), "mid");
    EXPECT_EQ(pq.Dequeue(), "low");
}

TEST(GenPriorityQueueTest, Count) {
    PriorityQueue<int, int> pq;
    pq.Enqueue(1, 1); pq.Enqueue(2, 2);
    EXPECT_EQ(pq.getCountProperty(), 2);
}

TEST(GenPriorityQueueTest, Peek) {
    PriorityQueue<int, int> pq;
    pq.Enqueue(5, 10); pq.Enqueue(3, 1);
    EXPECT_EQ(pq.Peek(), 3);
    EXPECT_EQ(pq.getCountProperty(), 2);
}

TEST(GenPriorityQueueTest, TryDequeue) {
    PriorityQueue<int, int> pq;
    pq.Enqueue(7, 2);
    int el = 0, pri = 0;
    EXPECT_TRUE(pq.TryDequeue(el, pri));
    EXPECT_EQ(el, 7);
    EXPECT_EQ(pri, 2);
    EXPECT_FALSE(pq.TryDequeue(el, pri));
}

TEST(GenPriorityQueueTest, TryPeek) {
    PriorityQueue<int, int> pq;
    int el = 0, pri = 0;
    EXPECT_FALSE(pq.TryPeek(el, pri));
    pq.Enqueue(42, 1);
    EXPECT_TRUE(pq.TryPeek(el, pri));
    EXPECT_EQ(el, 42);
}

TEST(GenPriorityQueueTest, EnqueueRange) {
    PriorityQueue<int, int> pq;
    pq.EnqueueRange({{3, 3}, {1, 1}, {2, 2}});
    EXPECT_EQ(pq.Dequeue(), 1);
}

TEST(GenPriorityQueueTest, Clear) {
    PriorityQueue<int, int> pq;
    pq.Enqueue(1, 1); pq.Enqueue(2, 2);
    pq.Clear();
    EXPECT_EQ(pq.getCountProperty(), 0);
}

// ---- OrderedDictionary<string,int> ----
TEST(OrderedDictionaryTest, AddAndContainsKey) {
    OrderedDictionary<std::string, int> d;
    d.Add("a", 1); d.Add("b", 2);
    EXPECT_TRUE(d.ContainsKey("a"));
    EXPECT_FALSE(d.ContainsKey("z"));
}

TEST(OrderedDictionaryTest, PreservesInsertionOrder) {
    OrderedDictionary<std::string, int> d;
    d.Add("x", 10); d.Add("y", 20); d.Add("z", 30);
    auto kv0 = d.GetAt(0); auto kv1 = d.GetAt(1); auto kv2 = d.GetAt(2);
    EXPECT_EQ(kv0.Key, "x");
    EXPECT_EQ(kv1.Key, "y");
    EXPECT_EQ(kv2.Key, "z");
}

TEST(OrderedDictionaryTest, IndexOf) {
    OrderedDictionary<std::string, int> d;
    d.Add("a", 1); d.Add("b", 2);
    EXPECT_EQ(d.IndexOf("b"), 1);
    EXPECT_EQ(d.IndexOf("z"), -1);
}

TEST(OrderedDictionaryTest, TryGetValue) {
    OrderedDictionary<std::string, int> d;
    d.Add("k", 42);
    int v = 0;
    EXPECT_TRUE(d.TryGetValue("k", v));
    EXPECT_EQ(v, 42);
    EXPECT_FALSE(d.TryGetValue("missing", v));
}

TEST(OrderedDictionaryTest, TryAdd) {
    OrderedDictionary<std::string, int> d;
    EXPECT_TRUE(d.TryAdd("a", 1));
    EXPECT_FALSE(d.TryAdd("a", 2));
    EXPECT_EQ(d.getCountProperty(), 1);
}

TEST(OrderedDictionaryTest, Remove) {
    OrderedDictionary<std::string, int> d;
    d.Add("a", 1); d.Add("b", 2); d.Add("c", 3);
    EXPECT_TRUE(d.Remove("b"));
    EXPECT_EQ(d.getCountProperty(), 2);
    EXPECT_EQ(d.IndexOf("c"), 1);
}

TEST(OrderedDictionaryTest, RemoveAt) {
    OrderedDictionary<std::string, int> d;
    d.Add("a", 1); d.Add("b", 2);
    d.RemoveAt(0);
    EXPECT_EQ(d.getCountProperty(), 1);
    EXPECT_EQ(d.GetAt(0).Key, "b");
}

TEST(OrderedDictionaryTest, SetAt) {
    OrderedDictionary<std::string, int> d;
    d.Add("a", 1);
    d.SetAt(0, 99);
    EXPECT_EQ(d.GetAt(0).Value, 99);
}

TEST(OrderedDictionaryTest, Insert) {
    OrderedDictionary<std::string, int> d;
    d.Add("a", 1); d.Add("c", 3);
    d.Insert(1, "b", 2);
    EXPECT_EQ(d.GetAt(1).Key, "b");
    EXPECT_EQ(d.getCountProperty(), 3);
}

TEST(OrderedDictionaryTest, ContainsValue) {
    OrderedDictionary<std::string, int> d;
    d.Add("a", 42);
    EXPECT_TRUE(d.ContainsValue(42));
    EXPECT_FALSE(d.ContainsValue(0));
}

TEST(OrderedDictionaryTest, Clear) {
    OrderedDictionary<std::string, int> d;
    d.Add("a", 1);
    d.Clear();
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(OrderedDictionaryTest, RangeFor) {
    OrderedDictionary<std::string, int> d;
    d.Add("a", 1); d.Add("b", 2);
    int sum = 0;
    for (const auto& e : d) sum += e.second;
    EXPECT_EQ(sum, 3);
}

// ---- ReferenceEqualityComparer ----
TEST(ReferenceEqualityComparerTest, SamePointerIsEqual) {
    int x = 42;
    auto& cmp = ReferenceEqualityComparer<int>::Instance();
    EXPECT_TRUE(cmp.Equals(&x, &x));
}

TEST(ReferenceEqualityComparerTest, DifferentPointersNotEqual) {
    int x = 1, y = 1;
    auto& cmp = ReferenceEqualityComparer<int>::Instance();
    EXPECT_FALSE(cmp.Equals(&x, &y));
}

TEST(ReferenceEqualityComparerTest, HashIsPointerBased) {
    int x = 0;
    auto& cmp = ReferenceEqualityComparer<int>::Instance();
    EXPECT_EQ(cmp.GetHashCode(&x), cmp.GetHashCode(&x));
}

TEST(ReferenceEqualityComparerTest, NullPointerHash) {
    auto& cmp = ReferenceEqualityComparer<int>::Instance();
    int* p = nullptr;
    EXPECT_EQ(cmp.GetHashCode(p), std::hash<int*>{}(nullptr));
}
