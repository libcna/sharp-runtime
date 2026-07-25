// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Concurrent/BlockingCollection.hpp"
#include "System/Collections/Concurrent/ConcurrentBag.hpp"
#include "System/Collections/Concurrent/ConcurrentStack.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/OperationCanceledException.hpp"
#include "System/Threading/CancellationTokenSource.hpp"

using System::Collections::Concurrent::BlockingCollection;
using System::Collections::Concurrent::ConcurrentBag;
using System::Collections::Concurrent::ConcurrentStack;
using SharpRuntime::intcs;

TEST(ConcurrentBagTests, AddTryTakePeekAndCount) {
    ConcurrentBag<int> bag;
    EXPECT_TRUE(bag.getIsEmptyProperty());

    bag.Add(10);
    EXPECT_TRUE(bag.TryAdd(20));
    EXPECT_EQ(bag.getCountProperty(), 2);

    int value = 0;
    EXPECT_TRUE(bag.TryPeek(value));
    EXPECT_EQ(value, 20);
    EXPECT_EQ(bag.getCountProperty(), 2);
    EXPECT_TRUE(bag.TryTake(value));
    EXPECT_EQ(value, 20);
    EXPECT_TRUE(bag.TryTake(value));
    EXPECT_EQ(value, 10);
    EXPECT_FALSE(bag.TryTake(value));
}

TEST(ConcurrentBagTests, SnapshotCopyEnumerationAndClear) {
    ConcurrentBag<int> bag(std::vector<int>{1, 2, 3});
    std::vector<int> copy(5, -1);
    bag.CopyTo(copy, 1);
    std::sort(copy.begin() + 1, copy.begin() + 4);
    EXPECT_EQ(copy, (std::vector<int>{-1, 1, 2, 3, -1}));

    const auto snapshot = bag.ToArray();
    EXPECT_EQ(snapshot.size(), 3u);
    bag.Clear();
    EXPECT_TRUE(bag.getIsEmptyProperty());

    ConcurrentBag<int> enumerated(snapshot);
    auto* enumerator = enumerated.GetEnumerator();
    std::vector<int> seen;
    while (enumerator->MoveNext()) {
        seen.push_back(enumerator->Current());
    }
    delete enumerator;
    std::sort(seen.begin(), seen.end());
    EXPECT_EQ(seen, (std::vector<int>{1, 2, 3}));
}

TEST(ConcurrentBagTests, CopyToValidatesArguments) {
    ConcurrentBag<int> bag;
    bag.Add(1);
    std::vector<int> target(1);
    std::vector<int> tooSmall;
    EXPECT_THROW(bag.CopyTo(target, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(bag.CopyTo(tooSmall, 0), System::ArgumentException);
}

TEST(ConcurrentBagTests, ConcurrentAddAndTakeLosesNoItems) {
    ConcurrentBag<int> bag;
    constexpr int kThreadCount = 6;
    constexpr int kItemsPerThread = 1000;

    std::vector<std::thread> producers;
    for (int threadIndex = 0; threadIndex < kThreadCount; ++threadIndex) {
        producers.emplace_back([&bag, threadIndex]() {
            for (int value = 0; value < kItemsPerThread; ++value) {
                bag.Add(threadIndex * kItemsPerThread + value);
            }
        });
    }
    for (auto& producer : producers) {
        producer.join();
    }
    EXPECT_EQ(bag.getCountProperty(), kThreadCount * kItemsPerThread);

    std::atomic<int> taken{0};
    std::vector<std::thread> consumers;
    for (int index = 0; index < kThreadCount; ++index) {
        consumers.emplace_back([&bag, &taken]() {
            int value = 0;
            while (bag.TryTake(value)) {
                taken.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& consumer : consumers) {
        consumer.join();
    }
    EXPECT_EQ(taken.load(), kThreadCount * kItemsPerThread);
    EXPECT_TRUE(bag.getIsEmptyProperty());
}

TEST(BlockingCollectionTests, DefaultCollectionIsUnboundedFifo) {
    BlockingCollection<int> collection;
    EXPECT_EQ(collection.getBoundedCapacityProperty(), -1);
    collection.Add(1);
    collection.Add(2);
    EXPECT_EQ(collection.getCountProperty(), 2);
    EXPECT_EQ(collection.Take(), 1);
    EXPECT_EQ(collection.Take(), 2);
}

TEST(BlockingCollectionTests, BoundedTryAddCompleteAndTakeCompletion) {
    BlockingCollection<int> collection(2);
    EXPECT_TRUE(collection.TryAdd(1));
    EXPECT_TRUE(collection.TryAdd(2));
    EXPECT_FALSE(collection.TryAdd(3));

    collection.CompleteAdding();
    EXPECT_TRUE(collection.getIsAddingCompletedProperty());
    EXPECT_FALSE(collection.getIsCompletedProperty());
    EXPECT_THROW(collection.TryAdd(3), System::InvalidOperationException);

    EXPECT_EQ(collection.Take(), 1);
    EXPECT_EQ(collection.Take(), 2);
    EXPECT_TRUE(collection.getIsCompletedProperty());
    int value = 0;
    EXPECT_FALSE(collection.TryTake(value));
    EXPECT_THROW(collection.Take(), System::InvalidOperationException);
}

TEST(BlockingCollectionTests, ProducerAndConsumerBlockUntilCollectionStateChanges) {
    BlockingCollection<int> collection(1);
    collection.Add(10);

    std::atomic<bool> producerAdded{false};
    std::thread producer([&]() {
        collection.Add(20);
        producerAdded = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_FALSE(producerAdded.load());
    EXPECT_EQ(collection.Take(), 10);
    producer.join();
    EXPECT_TRUE(producerAdded.load());
    EXPECT_EQ(collection.Take(), 20);

    std::atomic<int> consumed{0};
    std::thread consumer([&]() { consumed = collection.Take(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_EQ(consumed.load(), 0);
    collection.Add(30);
    consumer.join();
    EXPECT_EQ(consumed.load(), 30);
}

TEST(BlockingCollectionTests, TimeoutsAndCancellationAreObserved) {
    BlockingCollection<int> full(1);
    full.Add(1);
    EXPECT_FALSE(full.TryAdd(2, static_cast<intcs>(10)));

    BlockingCollection<int> empty;
    int value = 0;
    EXPECT_FALSE(empty.TryTake(value, static_cast<intcs>(10)));

    System::Threading::CancellationTokenSource source;
    std::atomic<bool> canceled{false};
    std::thread waiter([&]() {
        try {
            (void)empty.TryTake(value, static_cast<intcs>(-1), source.getTokenProperty());
        } catch (const System::OperationCanceledException&) {
            canceled = true;
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    source.Cancel();
    waiter.join();
    EXPECT_TRUE(canceled.load());
}

TEST(BlockingCollectionTests, ConsumingEnumerableDrainsUntilCompletion) {
    BlockingCollection<int> collection;
    collection.Add(1);
    collection.Add(2);
    collection.CompleteAdding();

    auto enumerable = collection.GetConsumingEnumerable();
    auto* enumerator = enumerable.GetEnumerator();
    std::vector<int> values;
    while (enumerator->MoveNext()) {
        values.push_back(enumerator->Current());
    }
    delete enumerator;

    EXPECT_EQ(values, (std::vector<int>{1, 2}));
    EXPECT_TRUE(collection.getIsCompletedProperty());
}

TEST(BlockingCollectionTests, CustomUnderlyingCollectionAndStaticAnyOperations) {
    ConcurrentStack<int> stack;
    BlockingCollection<int> stackBacked(stack, 3);
    stackBacked.Add(1);
    stackBacked.Add(2);
    EXPECT_EQ(stackBacked.Take(), 2);

    BlockingCollection<int> first(1);
    BlockingCollection<int> second(1);
    const std::vector<BlockingCollection<int>*> collections{&first, &second};
    EXPECT_EQ(BlockingCollection<int>::TryAddToAny(collections, 42), 0);
    int value = 0;
    EXPECT_EQ(BlockingCollection<int>::TryTakeFromAny(collections, value), 0);
    EXPECT_EQ(value, 42);
}

TEST(BlockingCollectionTests, TimeSpanTokenSnapshotAndAllAnyOverloads) {
    BlockingCollection<int> collection;
    const System::TimeSpan shortTimeout(0, 0, 0, 0, 5);
    System::Threading::CancellationTokenSource source;

    EXPECT_TRUE(collection.TryAdd(1, shortTimeout));
    collection.Add(2, source.getTokenProperty());
    EXPECT_EQ(collection.ToArray(), (std::vector<int>{1, 2}));

    std::vector<int> copy(3, -1);
    collection.CopyTo(copy, 1);
    EXPECT_EQ(copy, (std::vector<int>{-1, 1, 2}));
    auto* snapshot = collection.GetEnumerator();
    ASSERT_TRUE(snapshot->MoveNext());
    EXPECT_EQ(snapshot->Current(), 1);
    delete snapshot;

    int value = 0;
    EXPECT_TRUE(collection.TryTake(value, shortTimeout));
    EXPECT_EQ(value, 1);
    EXPECT_EQ(collection.Take(source.getTokenProperty()), 2);

    BlockingCollection<int> first(1);
    BlockingCollection<int> second(1);
    const std::vector<BlockingCollection<int>*> collections{&first, &second};
    EXPECT_EQ(BlockingCollection<int>::AddToAny(collections, 7), 0);
    EXPECT_EQ(BlockingCollection<int>::TakeFromAny(collections, value, source.getTokenProperty()), 0);
    EXPECT_EQ(value, 7);
    EXPECT_EQ(BlockingCollection<int>::TryAddToAny(collections, 8, shortTimeout), 0);
    EXPECT_EQ(BlockingCollection<int>::TryTakeFromAny(collections, value, shortTimeout), 0);
    EXPECT_EQ(value, 8);
}

TEST(BlockingCollectionTests, ConstructorAndDisposeValidateState) {
    EXPECT_THROW(BlockingCollection<int>(0), System::ArgumentOutOfRangeException);

    ConcurrentStack<int> backing;
    backing.Push(1);
    backing.Push(2);
    EXPECT_THROW(BlockingCollection<int>(backing, 1), System::ArgumentException);

    BlockingCollection<int> collection;
    collection.Dispose();
    EXPECT_THROW(collection.getCountProperty(), System::ObjectDisposedException);
    EXPECT_THROW(collection.Add(1), System::ObjectDisposedException);
}
