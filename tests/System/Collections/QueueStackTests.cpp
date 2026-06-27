// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/Queue.hpp"
#include "System/Collections/Stack.hpp"
#include "System/Collections/IStructuralEquatable.hpp"
#include "System/Collections/IEqualityComparer.hpp"

using namespace System::Collections;

// -----------------------------------------------------------------------
// Queue tests
// -----------------------------------------------------------------------

TEST(QueueTest, EnqueueAndCount) {
    Queue q;
    int a = 1, b = 2;
    q.Enqueue(&a);
    q.Enqueue(&b);
    EXPECT_EQ(q.getCountProperty(), 2);
}

TEST(QueueTest, DequeueFIFO) {
    Queue q;
    int a = 10, b = 20;
    q.Enqueue(&a);
    q.Enqueue(&b);
    EXPECT_EQ(q.Dequeue(), &a);
    EXPECT_EQ(q.Dequeue(), &b);
}

TEST(QueueTest, Peek) {
    Queue q;
    int a = 5;
    q.Enqueue(&a);
    EXPECT_EQ(q.Peek(), &a);
    EXPECT_EQ(q.getCountProperty(), 1);
}

TEST(QueueTest, Contains) {
    Queue q;
    int a = 1, b = 2;
    q.Enqueue(&a);
    EXPECT_TRUE(q.Contains(&a));
    EXPECT_FALSE(q.Contains(&b));
}

TEST(QueueTest, Clear) {
    Queue q;
    int a = 1;
    q.Enqueue(&a);
    q.Clear();
    EXPECT_EQ(q.getCountProperty(), 0);
}

TEST(QueueTest, ToArray) {
    Queue q;
    int a = 1, b = 2;
    q.Enqueue(&a);
    q.Enqueue(&b);
    auto arr = q.ToArray();
    ASSERT_EQ(static_cast<int>(arr.size()), 2);
    EXPECT_EQ(arr[0], &a);
    EXPECT_EQ(arr[1], &b);
}

TEST(QueueTest, Clone) {
    Queue q;
    int a = 1;
    q.Enqueue(&a);
    Queue copy = q.Clone();
    EXPECT_EQ(copy.getCountProperty(), 1);
    EXPECT_EQ(copy.Peek(), &a);
}

TEST(QueueTest, DequeueEmptyThrows) {
    Queue q;
    EXPECT_THROW(q.Dequeue(), std::invalid_argument);
}

TEST(QueueTest, CopyTo) {
    Queue q;
    int a = 1, b = 2;
    q.Enqueue(&a);
    q.Enqueue(&b);
    void* buf[4] = {};
    q.CopyTo(buf, 1);
    EXPECT_EQ(buf[1], &a);
    EXPECT_EQ(buf[2], &b);
}

// -----------------------------------------------------------------------
// Stack tests
// -----------------------------------------------------------------------

TEST(StackTest, PushAndCount) {
    Stack s;
    int a = 1, b = 2;
    s.Push(&a);
    s.Push(&b);
    EXPECT_EQ(s.getCountProperty(), 2);
}

TEST(StackTest, PopLIFO) {
    Stack s;
    int a = 10, b = 20;
    s.Push(&a);
    s.Push(&b);
    EXPECT_EQ(s.Pop(), &b);
    EXPECT_EQ(s.Pop(), &a);
}

TEST(StackTest, Peek) {
    Stack s;
    int a = 5;
    s.Push(&a);
    EXPECT_EQ(s.Peek(), &a);
    EXPECT_EQ(s.getCountProperty(), 1);
}

TEST(StackTest, Contains) {
    Stack s;
    int a = 1, b = 2;
    s.Push(&a);
    EXPECT_TRUE(s.Contains(&a));
    EXPECT_FALSE(s.Contains(&b));
}

TEST(StackTest, Clear) {
    Stack s;
    int a = 1;
    s.Push(&a);
    s.Clear();
    EXPECT_EQ(s.getCountProperty(), 0);
}

TEST(StackTest, ToArrayTopFirst) {
    Stack s;
    int a = 1, b = 2;
    s.Push(&a);
    s.Push(&b);
    auto arr = s.ToArray();
    ASSERT_EQ(static_cast<int>(arr.size()), 2);
    EXPECT_EQ(arr[0], &b);
    EXPECT_EQ(arr[1], &a);
}

TEST(StackTest, Clone) {
    Stack s;
    int a = 7;
    s.Push(&a);
    Stack copy = s.Clone();
    EXPECT_EQ(copy.getCountProperty(), 1);
    EXPECT_EQ(copy.Peek(), &a);
}

TEST(StackTest, PopEmptyThrows) {
    Stack s;
    EXPECT_THROW(s.Pop(), std::invalid_argument);
}

// -----------------------------------------------------------------------
// IStructuralEquatable tests
// -----------------------------------------------------------------------

namespace {

class IntEqComp : public IEqualityComparer {
public:
    bool Equals(const void* x, const void* y) const override {
        return *static_cast<const int*>(x) == *static_cast<const int*>(y);
    }
    std::size_t GetHashCode(const void* obj) const override {
        return static_cast<std::size_t>(*static_cast<const int*>(obj));
    }
};

class IntEquatable : public IStructuralEquatable {
    int val_;
public:
    explicit IntEquatable(int v) : val_(v) {}
    bool Equals(const void* other, const IEqualityComparer& cmp) const override {
        return cmp.Equals(&val_, other);
    }
    std::size_t GetHashCode(const IEqualityComparer& cmp) const override {
        return cmp.GetHashCode(&val_);
    }
};

} // namespace

TEST(IStructuralEquatableTest, EqualObjects) {
    IntEquatable a(5);
    IntEqComp cmp;
    int b = 5;
    EXPECT_TRUE(a.Equals(&b, cmp));
}

TEST(IStructuralEquatableTest, UnequalObjects) {
    IntEquatable a(3);
    IntEqComp cmp;
    int b = 7;
    EXPECT_FALSE(a.Equals(&b, cmp));
}

TEST(IStructuralEquatableTest, GetHashCodeConsistent) {
    IntEquatable a(42);
    IntEqComp cmp;
    EXPECT_EQ(a.GetHashCode(cmp), a.GetHashCode(cmp));
}
