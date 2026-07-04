// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/Concurrent/ConcurrentQueue.hpp"

using System::Collections::Concurrent::ConcurrentQueue;

TEST(ConcurrentQueueTest, EnqueueAndCount) {
    ConcurrentQueue<int> q;
    q.Enqueue(1);
    q.Enqueue(2);
    EXPECT_EQ(q.getCountProperty(), 2);
}

TEST(ConcurrentQueueTest, TryDequeueFIFO) {
    ConcurrentQueue<int> q;
    q.Enqueue(10);
    q.Enqueue(20);
    int v = 0;
    EXPECT_TRUE(q.TryDequeue(v));
    EXPECT_EQ(v, 10);
    EXPECT_TRUE(q.TryDequeue(v));
    EXPECT_EQ(v, 20);
}

TEST(ConcurrentQueueTest, TryDequeueEmptyReturnsFalse) {
    ConcurrentQueue<int> q;
    int v = 0;
    EXPECT_FALSE(q.TryDequeue(v));
}

TEST(ConcurrentQueueTest, TryPeek) {
    ConcurrentQueue<int> q;
    q.Enqueue(5);
    int v = 0;
    EXPECT_TRUE(q.TryPeek(v));
    EXPECT_EQ(v, 5);
    EXPECT_EQ(q.getCountProperty(), 1);
}

TEST(ConcurrentQueueTest, IsEmpty) {
    ConcurrentQueue<int> q;
    EXPECT_TRUE(q.getIsEmptyProperty());
    q.Enqueue(1);
    EXPECT_FALSE(q.getIsEmptyProperty());
}

TEST(ConcurrentQueueTest, Clear) {
    ConcurrentQueue<int> q;
    q.Enqueue(1);
    q.Enqueue(2);
    q.Clear();
    EXPECT_TRUE(q.getIsEmptyProperty());
}

TEST(ConcurrentQueueTest, TryAdd_AddsAtBack) {
    ConcurrentQueue<int> q;
    q.Enqueue(1);
    EXPECT_TRUE(q.TryAdd(2));
    int v = 0;
    q.TryDequeue(v);
    EXPECT_EQ(v, 1);
    q.TryDequeue(v);
    EXPECT_EQ(v, 2);
}

TEST(ConcurrentQueueTest, TryTake_EquivalentToTryDequeue) {
    ConcurrentQueue<int> q;
    q.Enqueue(7);
    int v = 0;
    EXPECT_TRUE(q.TryTake(v));
    EXPECT_EQ(v, 7);
    EXPECT_TRUE(q.getIsEmptyProperty());
}

TEST(ConcurrentQueueTest, ToArray_FrontToBackOrder) {
    ConcurrentQueue<int> q;
    q.Enqueue(1);
    q.Enqueue(2);
    q.Enqueue(3);
    auto arr = q.ToArray();
    ASSERT_EQ(arr.size(), 3u);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
}

TEST(ConcurrentQueueTest, CopyTo_TooSmall_Throws) {
    ConcurrentQueue<int> q;
    q.Enqueue(1);
    q.Enqueue(2);
    std::vector<int> dest(1);
    EXPECT_THROW(q.CopyTo(dest, 0), std::out_of_range);
}

TEST(ConcurrentQueueTest, GetEnumerator_IteratesFrontToBack) {
    ConcurrentQueue<int> q;
    q.Enqueue(1);
    q.Enqueue(2);
    auto* e = q.GetEnumerator();
    ASSERT_TRUE(e->MoveNext());
    EXPECT_EQ(e->Current(), 1);
    ASSERT_TRUE(e->MoveNext());
    EXPECT_EQ(e->Current(), 2);
    EXPECT_FALSE(e->MoveNext());
    delete e;
}
