// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Concurrent/ConcurrentBag.hpp"

using System::Collections::Concurrent::ConcurrentBag;

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
