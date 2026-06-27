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
