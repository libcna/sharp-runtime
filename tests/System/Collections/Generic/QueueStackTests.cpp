// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "System/Collections/Generic/Queue.hpp"
#include "System/Collections/Generic/Stack.hpp"

using System::Collections::Generic::Queue;
using System::Collections::Generic::Stack;

// ---------------------------------------------------------------------------
// Queue<T>
// ---------------------------------------------------------------------------

TEST(QueueTests, DefaultCtorIsEmpty) {
    Queue<int> q;
    EXPECT_EQ(q.getCountProperty(), 0);
}

TEST(QueueTests, EnqueueIncreasesCount) {
    Queue<int> q;
    q.Enqueue(1);
    q.Enqueue(2);
    EXPECT_EQ(q.getCountProperty(), 2);
}

TEST(QueueTests, DequeueReturnsFront) {
    Queue<int> q;
    q.Enqueue(10);
    q.Enqueue(20);
    q.Enqueue(30);
    EXPECT_EQ(q.Dequeue(), 10);
    EXPECT_EQ(q.Dequeue(), 20);
    EXPECT_EQ(q.Dequeue(), 30);
}

TEST(QueueTests, DequeueDecreasesCount) {
    Queue<int> q;
    q.Enqueue(1); q.Enqueue(2);
    q.Dequeue();
    EXPECT_EQ(q.getCountProperty(), 1);
}

TEST(QueueTests, DequeueEmptyThrows) {
    Queue<int> q;
    EXPECT_THROW(q.Dequeue(), std::runtime_error);
}

TEST(QueueTests, PeekReturnsFrontWithoutRemoving) {
    Queue<int> q;
    q.Enqueue(42);
    q.Enqueue(99);
    EXPECT_EQ(q.Peek(), 42);
    EXPECT_EQ(q.getCountProperty(), 2); // still 2
    EXPECT_EQ(q.Peek(), 42);            // unchanged
}

TEST(QueueTests, PeekEmptyThrows) {
    Queue<int> q;
    EXPECT_THROW((void)q.Peek(), std::runtime_error);
}

TEST(QueueTests, FifoOrder) {
    Queue<int> q;
    for (int i = 1; i <= 5; ++i) q.Enqueue(i);
    for (int i = 1; i <= 5; ++i) EXPECT_EQ(q.Dequeue(), i);
}

TEST(QueueTests, ContainsFound) {
    Queue<int> q;
    q.Enqueue(1); q.Enqueue(2); q.Enqueue(3);
    EXPECT_TRUE(q.Contains(2));
}

TEST(QueueTests, ContainsNotFound) {
    Queue<int> q;
    q.Enqueue(1); q.Enqueue(2);
    EXPECT_FALSE(q.Contains(99));
}

TEST(QueueTests, ContainsDoesNotMutate) {
    Queue<int> q;
    q.Enqueue(7); q.Enqueue(8);
    (void)q.Contains(7);
    EXPECT_EQ(q.getCountProperty(), 2);
    EXPECT_EQ(q.Peek(), 7);
}

TEST(QueueTests, ClearResetsCount) {
    Queue<int> q;
    q.Enqueue(1); q.Enqueue(2); q.Enqueue(3);
    q.Clear();
    EXPECT_EQ(q.getCountProperty(), 0);
    EXPECT_THROW((void)q.Peek(), std::runtime_error);
}

TEST(QueueTests, ToArrayFifoOrder) {
    Queue<int> q;
    q.Enqueue(10); q.Enqueue(20); q.Enqueue(30);
    std::vector<int> arr = q.ToArray();
    ASSERT_EQ(arr.size(), 3u);
    EXPECT_EQ(arr[0], 10);
    EXPECT_EQ(arr[1], 20);
    EXPECT_EQ(arr[2], 30);
}

TEST(QueueTests, ToArrayDoesNotMutate) {
    Queue<int> q;
    q.Enqueue(1); q.Enqueue(2);
    (void)q.ToArray();
    EXPECT_EQ(q.getCountProperty(), 2);
    EXPECT_EQ(q.Peek(), 1);
}

TEST(QueueTests, EnqueueAfterDequeue) {
    Queue<int> q;
    q.Enqueue(1); q.Enqueue(2);
    q.Dequeue();       // removes 1
    q.Enqueue(3);      // now: 2, 3
    EXPECT_EQ(q.Dequeue(), 2);
    EXPECT_EQ(q.Dequeue(), 3);
    EXPECT_EQ(q.getCountProperty(), 0);
}

TEST(QueueTests, StringQueue) {
    Queue<std::string> q;
    q.Enqueue(std::string("alpha"));
    q.Enqueue(std::string("beta"));
    EXPECT_EQ(q.Dequeue(), "alpha");
    EXPECT_EQ(q.Peek(), "beta");
    EXPECT_TRUE(q.Contains(std::string("beta")));
}

TEST(QueueTests, StressFifo) {
    Queue<int> q;
    for (int i = 0; i < 1000; ++i) q.Enqueue(i);
    EXPECT_EQ(q.getCountProperty(), 1000);
    for (int i = 0; i < 1000; ++i) EXPECT_EQ(q.Dequeue(), i);
    EXPECT_EQ(q.getCountProperty(), 0);
}

// ---------------------------------------------------------------------------
// Stack<T>
// ---------------------------------------------------------------------------

TEST(StackTests, DefaultCtorIsEmpty) {
    Stack<int> s;
    EXPECT_EQ(s.getCountProperty(), 0);
}

TEST(StackTests, PushIncreasesCount) {
    Stack<int> s;
    s.Push(1); s.Push(2);
    EXPECT_EQ(s.getCountProperty(), 2);
}

TEST(StackTests, PopReturnsTop) {
    Stack<int> s;
    s.Push(10); s.Push(20); s.Push(30);
    EXPECT_EQ(s.Pop(), 30);
    EXPECT_EQ(s.Pop(), 20);
    EXPECT_EQ(s.Pop(), 10);
}

TEST(StackTests, PopDecreasesCount) {
    Stack<int> s;
    s.Push(1); s.Push(2);
    s.Pop();
    EXPECT_EQ(s.getCountProperty(), 1);
}

TEST(StackTests, PopEmptyThrows) {
    Stack<int> s;
    EXPECT_THROW(s.Pop(), std::runtime_error);
}

TEST(StackTests, PeekReturnsTopWithoutRemoving) {
    Stack<int> s;
    s.Push(5); s.Push(99);
    EXPECT_EQ(s.Peek(), 99);
    EXPECT_EQ(s.getCountProperty(), 2); // still 2
    EXPECT_EQ(s.Peek(), 99);            // unchanged
}

TEST(StackTests, PeekEmptyThrows) {
    Stack<int> s;
    EXPECT_THROW((void)s.Peek(), std::runtime_error);
}

TEST(StackTests, LifoOrder) {
    Stack<int> s;
    for (int i = 1; i <= 5; ++i) s.Push(i);
    for (int i = 5; i >= 1; --i) EXPECT_EQ(s.Pop(), i);
}

TEST(StackTests, ContainsFound) {
    Stack<int> s;
    s.Push(1); s.Push(2); s.Push(3);
    EXPECT_TRUE(s.Contains(2));
}

TEST(StackTests, ContainsNotFound) {
    Stack<int> s;
    s.Push(1); s.Push(2);
    EXPECT_FALSE(s.Contains(99));
}

TEST(StackTests, ContainsDoesNotMutate) {
    Stack<int> s;
    s.Push(7); s.Push(8);
    (void)s.Contains(7);
    EXPECT_EQ(s.getCountProperty(), 2);
    EXPECT_EQ(s.Peek(), 8);
}

TEST(StackTests, ClearResetsCount) {
    Stack<int> s;
    s.Push(1); s.Push(2); s.Push(3);
    s.Clear();
    EXPECT_EQ(s.getCountProperty(), 0);
    EXPECT_THROW((void)s.Peek(), std::runtime_error);
}

TEST(StackTests, ToArrayTopFirst) {
    // .NET Stack.ToArray() returns elements top-first (LIFO order)
    Stack<int> s;
    s.Push(10); s.Push(20); s.Push(30); // top = 30
    std::vector<int> arr = s.ToArray();
    ASSERT_EQ(arr.size(), 3u);
    EXPECT_EQ(arr[0], 30);
    EXPECT_EQ(arr[1], 20);
    EXPECT_EQ(arr[2], 10);
}

TEST(StackTests, ToArrayDoesNotMutate) {
    Stack<int> s;
    s.Push(1); s.Push(2);
    (void)s.ToArray();
    EXPECT_EQ(s.getCountProperty(), 2);
    EXPECT_EQ(s.Peek(), 2);
}

TEST(StackTests, PushAfterPop) {
    Stack<int> s;
    s.Push(1); s.Push(2);
    s.Pop();        // removes 2
    s.Push(3);      // now: 1, 3 (top = 3)
    EXPECT_EQ(s.Pop(), 3);
    EXPECT_EQ(s.Pop(), 1);
    EXPECT_EQ(s.getCountProperty(), 0);
}

TEST(StackTests, StringStack) {
    Stack<std::string> s;
    s.Push(std::string("first"));
    s.Push(std::string("second"));
    EXPECT_EQ(s.Pop(), "second");
    EXPECT_EQ(s.Peek(), "first");
    EXPECT_TRUE(s.Contains(std::string("first")));
}

TEST(StackTests, StressLifo) {
    Stack<int> s;
    for (int i = 0; i < 1000; ++i) s.Push(i);
    EXPECT_EQ(s.getCountProperty(), 1000);
    for (int i = 999; i >= 0; --i) EXPECT_EQ(s.Pop(), i);
    EXPECT_EQ(s.getCountProperty(), 0);
}
