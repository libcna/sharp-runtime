// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <limits>
#include <thread>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Concurrent/BlockingCollection.hpp"
#include "System/Collections/Concurrent/ConcurrentStack.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/OperationCanceledException.hpp"
#include "System/Threading/CancellationTokenSource.hpp"

using System::Collections::Concurrent::BlockingCollection;
using System::Collections::Concurrent::ConcurrentStack;
using SharpRuntime::intcs;

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

TEST(BlockingCollectionTests, FractionalNegativeTimeSpanUsesDotNetTruncationBoundaries) {
    using System::TimeSpan;
    constexpr auto ticksPerMillisecond = TimeSpan::TicksPerMillisecond;
    const TimeSpan negativeHalfMillisecond(-ticksPerMillisecond / 2);
    const TimeSpan negativeOneAndAHalfMilliseconds(-(ticksPerMillisecond * 3) / 2);
    const TimeSpan negativeAlmostTwoMilliseconds(-(ticksPerMillisecond * 2) + 1);
    const TimeSpan negativeTwoMilliseconds(-(ticksPerMillisecond * 2));

    // (-1ms, 0) truncates to zero, so an empty take returns immediately.
    BlockingCollection<int> empty;
    int value = 0;
    EXPECT_FALSE(empty.TryTake(value, negativeHalfMillisecond));

    // [-1.9999ms, -1ms] truncates to -1, the infinite sentinel. A full bounded collection
    // must therefore keep waiting until capacity is released rather than returning false.
    BlockingCollection<int> bounded(1);
    bounded.Add(1);
    std::promise<void> entered;
    auto enteredFuture = entered.get_future();
    auto add = std::async(std::launch::async, [&] {
        entered.set_value();
        return bounded.TryAdd(2, negativeOneAndAHalfMilliseconds);
    });
    enteredFuture.wait();
    EXPECT_EQ(add.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
    EXPECT_TRUE(bounded.TryTake(value, 0));
    EXPECT_TRUE(add.get());

    // The other three TimeSpan doors share the same conversion helper.
    BlockingCollection<int> first;
    const std::vector<BlockingCollection<int>*> collections{&first};
    EXPECT_EQ(BlockingCollection<int>::TryAddToAny(
                  collections, 7, negativeAlmostTwoMilliseconds),
              0);
    EXPECT_EQ(BlockingCollection<int>::TryTakeFromAny(
                  collections, value, negativeHalfMillisecond),
              0);
    EXPECT_EQ(value, 7);

    EXPECT_THROW(empty.TryAdd(1, negativeTwoMilliseconds), System::ArgumentOutOfRangeException);
    EXPECT_THROW(empty.TryTake(value, negativeTwoMilliseconds), System::ArgumentOutOfRangeException);
    EXPECT_THROW(BlockingCollection<int>::TryAddToAny(
                     collections, 1, negativeTwoMilliseconds),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(BlockingCollection<int>::TryTakeFromAny(
                     collections, value, negativeTwoMilliseconds),
                 System::ArgumentOutOfRangeException);
}

TEST(BlockingCollectionTests, TimeSpanMillisecondRepresentabilityIsCheckedAfterTruncation) {
    using System::TimeSpan;
    const auto maxMilliseconds = static_cast<SharpRuntime::longcs>(
        std::numeric_limits<intcs>::max());
    const TimeSpan maxPlusFraction(
        maxMilliseconds * TimeSpan::TicksPerMillisecond + TimeSpan::TicksPerMillisecond - 1);
    const TimeSpan firstUnrepresentable(
        (maxMilliseconds + 1) * TimeSpan::TicksPerMillisecond);

    BlockingCollection<int> collection;
    EXPECT_TRUE(collection.TryAdd(1, maxPlusFraction));
    EXPECT_THROW(collection.TryAdd(2, firstUnrepresentable),
                 System::ArgumentOutOfRangeException);
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
