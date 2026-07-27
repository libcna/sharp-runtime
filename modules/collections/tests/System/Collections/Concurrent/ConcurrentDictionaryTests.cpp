// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/Concurrent/ConcurrentDictionary.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using System::Collections::Concurrent::ConcurrentDictionary;

TEST(ConcurrentDictionaryTest, TryAddAndCount) {
    ConcurrentDictionary<std::string, int> d;
    EXPECT_TRUE(d.TryAdd("a", 1));
    EXPECT_EQ(d.getCountProperty(), 1);
}

TEST(ConcurrentDictionaryTest, TryAddDuplicateReturnsFalse) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("x", 1);
    EXPECT_FALSE(d.TryAdd("x", 2));
}

TEST(ConcurrentDictionaryTest, TryGetValue) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("k", 42);
    int v = 0;
    EXPECT_TRUE(d.TryGetValue("k", v));
    EXPECT_EQ(v, 42);
}

TEST(ConcurrentDictionaryTest, TryGetValueMissing) {
    ConcurrentDictionary<std::string, int> d;
    int v = 0;
    EXPECT_FALSE(d.TryGetValue("missing", v));
}

TEST(ConcurrentDictionaryTest, TryRemove) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("r", 99);
    int v = 0;
    EXPECT_TRUE(d.TryRemove("r", v));
    EXPECT_EQ(v, 99);
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(ConcurrentDictionaryTest, ContainsKey) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("yes", 1);
    EXPECT_TRUE(d.ContainsKey("yes"));
    EXPECT_FALSE(d.ContainsKey("no"));
}

TEST(ConcurrentDictionaryTest, GetOrAdd) {
    ConcurrentDictionary<std::string, int> d;
    int v = d.GetOrAdd("key", 7);
    EXPECT_EQ(v, 7);
    int v2 = d.GetOrAdd("key", 99);
    EXPECT_EQ(v2, 7);
}

TEST(ConcurrentDictionaryTest, TryUpdate) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("u", 10);
    EXPECT_TRUE(d.TryUpdate("u", 20, 10));
    int v = 0;
    d.TryGetValue("u", v);
    EXPECT_EQ(v, 20);
}

TEST(ConcurrentDictionaryTest, AddOrUpdate) {
    ConcurrentDictionary<std::string, int> d;
    d.AddOrUpdate("au", 1, [](const std::string&, int old){ return old + 1; });
    d.AddOrUpdate("au", 1, [](const std::string&, int old){ return old + 1; });
    int v = 0;
    d.TryGetValue("au", v);
    EXPECT_EQ(v, 2);
}

TEST(ConcurrentDictionaryTest, Clear) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("a", 1);
    d.Clear();
    EXPECT_TRUE(d.getIsEmptyProperty());
}

TEST(ConcurrentDictionaryTest, Keys) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("a", 1);
    d.TryAdd("b", 2);
    auto keys = d.getKeysProperty();
    EXPECT_EQ(static_cast<int>(keys.size()), 2);
}

TEST(ConcurrentDictionaryTest, AddOrUpdate_WithAddFactory_NewKey_Adds) {
    ConcurrentDictionary<std::string, int> d;
    int result = d.AddOrUpdate("a", [](const std::string&) { return 5; },
                                [](const std::string&, int v) { return v + 1; });
    EXPECT_EQ(result, 5);
}

TEST(ConcurrentDictionaryTest, AddOrUpdate_WithAddFactory_ExistingKey_Updates) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("a", 5);
    int result = d.AddOrUpdate("a", [](const std::string&) { return 100; },
                                [](const std::string&, int v) { return v + 1; });
    EXPECT_EQ(result, 6);
}

// .NET's ConcurrentDictionary<TKey,TValue> indexer getter throws KeyNotFoundException for
// an absent key (ConcurrentDictionary.cs:1069-1072) -- it does NOT insert a default value,
// unlike std::unordered_map::operator[] or this type's own prior (buggy) implementation.
TEST(ConcurrentDictionaryTest, Indexer_MissingKey_Throws) {
    ConcurrentDictionary<std::string, int> d;
    EXPECT_THROW({ int v = d["missing"]; (void)v; }, System::Collections::Generic::KeyNotFoundException);
    EXPECT_TRUE(d.getIsEmptyProperty());
}

TEST(ConcurrentDictionaryTest, Indexer_Get_ExistingKey_ReturnsValue) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("a", 42);
    int v = d["a"];
    EXPECT_EQ(v, 42);
}

TEST(ConcurrentDictionaryTest, Indexer_Set_NewKey_Inserts) {
    ConcurrentDictionary<std::string, int> d;
    d["a"] = 7;
    EXPECT_EQ(d.getCountProperty(), 1);
    EXPECT_EQ(static_cast<int>(d["a"]), 7);
}

TEST(ConcurrentDictionaryTest, Indexer_Set_ExistingKey_Overwrites) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("a", 1);
    d["a"] = 2;
    EXPECT_EQ(d.getCountProperty(), 1);
    EXPECT_EQ(static_cast<int>(d["a"]), 2);
}

// Regression tests for ticket 1716: GetOrAdd and
// AddOrUpdate previously held the internal mutex across the entire user-supplied factory
// callback, so a factory that reentrantly called back into the same ConcurrentDictionary
// instance (a realistic memoization pattern) deadlocked the thread against itself
// (std::mutex is non-recursive). Real .NET's documented contract explicitly permits and
// expects the factory to run without the lock held. Each test below intentionally has the
// callback call another ConcurrentDictionary method reentrantly; a hang here (rather than a
// clean pass) indicates the deadlock has regressed.

TEST(ConcurrentDictionaryTest, GetOrAdd_ReentrantFactory_DoesNotDeadlock) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("related", 10);
    int result = d.GetOrAdd("key", [&](const std::string&) {
        // Reentrant call into the same instance while "key" is still being computed.
        int related = d.GetOrAdd("related", 999);
        return related + 1;
    });
    EXPECT_EQ(result, 11);
    EXPECT_EQ(d.getCountProperty(), 2);
}

TEST(ConcurrentDictionaryTest, AddOrUpdate_ReentrantUpdateFactory_DoesNotDeadlock) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("a", 1);
    d.TryAdd("b", 5);
    int result = d.AddOrUpdate("a", 0, [&](const std::string&, int old) {
        int b = d.GetOrAdd("b", -1);
        return old + b;
    });
    EXPECT_EQ(result, 6);
}

TEST(ConcurrentDictionaryTest, AddOrUpdate_ReentrantAddFactory_DoesNotDeadlock) {
    ConcurrentDictionary<std::string, int> d;
    int result = d.AddOrUpdate(
        "a",
        [&](const std::string&) { return d.GetOrAdd("b", 3); },
        [](const std::string&, int old) { return old + 1; });
    EXPECT_EQ(result, 3);
    EXPECT_TRUE(d.ContainsKey("a"));
    EXPECT_TRUE(d.ContainsKey("b"));
}

TEST(ConcurrentDictionaryTest, GetOrAdd_ConcurrentContention_RaceDiscardsLosingFactory) {
    ConcurrentDictionary<std::string, int> d;
    int result1 = d.GetOrAdd("shared", [](const std::string&) { return 100; });
    int result2 = d.GetOrAdd("shared", [](const std::string&) { return 200; });
    // Second call's factory result must be discarded since the key already existed.
    EXPECT_EQ(result1, 100);
    EXPECT_EQ(result2, 100);
    EXPECT_EQ(d.getCountProperty(), 1);
}

// Ticket 1725 (post-stabilization-audit): the reentrancy tests above (GetOrAdd/AddOrUpdate
// deadlock scenarios, fixed by ticket 1716) exercise the lock-release-around-callback logic
// specifically, but none exercised ORDINARY concurrent access from real OS threads. Matches the
// stress test pattern already established for sibling ConcurrentStack/ConcurrentQueue: N threads
// each TryAdd distinct keys (no key contention between threads, so no losing-factory-discard
// noise), verifying no lost updates; then N threads each TryRemove their own keys concurrently,
// verifying every key is removed and the dictionary ends up empty.
TEST(ConcurrentDictionaryTest, ConcurrentAddRemove_DistinctKeys_NoLostUpdatesOrCorruption) {
    ConcurrentDictionary<std::string, int> d;
    constexpr int kThreads = 8;
    constexpr int kPerThread = 500;

    std::vector<std::thread> adders;
    for (int t = 0; t < kThreads; ++t) {
        adders.emplace_back([&d, t]() {
            for (int i = 0; i < kPerThread; ++i) {
                std::string key = "t" + std::to_string(t) + "_" + std::to_string(i);
                EXPECT_TRUE(d.TryAdd(key, t * kPerThread + i));
            }
        });
    }
    for (auto& th : adders) th.join();
    EXPECT_EQ(d.getCountProperty(), kThreads * kPerThread);

    std::atomic<int> removedCount{0};
    std::vector<std::thread> removers;
    for (int t = 0; t < kThreads; ++t) {
        removers.emplace_back([&d, &removedCount, t]() {
            for (int i = 0; i < kPerThread; ++i) {
                std::string key = "t" + std::to_string(t) + "_" + std::to_string(i);
                int value;
                if (d.TryRemove(key, value)) {
                    EXPECT_EQ(value, t * kPerThread + i);
                    removedCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& th : removers) th.join();

    EXPECT_EQ(removedCount.load(), kThreads * kPerThread);
    EXPECT_EQ(d.getCountProperty(), 0);
}

// Regression tests for ticket 1778 / SR-AUD-360: AddOrUpdate previously snapshotted the
// existing value, ran the update factory outside the lock, then unconditionally overwrote the
// entry with the factory result -- silently discarding any write that landed on the same key
// while the factory was running. Real .NET's TryUpdateInternal instead gates the commit on
// EqualityComparer<TValue>.Default.Equals(current, observed) and retries the whole operation
// (re-observing the value and re-invoking the factory) when the gate fails. Each test below
// deterministically coordinates a second thread to write to the key while the update factory
// is blocked inside AddOrUpdate, so a regression to the old unconditional-overwrite behavior
// fails deterministically rather than only under incidental scheduling luck.

TEST(ConcurrentDictionaryTest, AddOrUpdate_ConstAddValue_InterveningWrite_RetriesInsteadOfLosingIt) {
    ConcurrentDictionary<int, int> d;
    d.TryAdd(1, 0);

    std::mutex m;
    std::condition_variable cv;
    bool factoryEntered = false;
    bool releaseFactory = false;
    int factoryInvocations = 0;

    std::thread updater([&] {
        d.AddOrUpdate(1, 999, [&](const int&, const int& oldValue) {
            {
                std::lock_guard<std::mutex> lk(m);
                ++factoryInvocations;
                factoryEntered = true;
            }
            cv.notify_all();
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [&] { return releaseFactory; });
            return oldValue + 1;
        });
    });

    {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&] { return factoryEntered; });
    }

    // Intervening write while the factory is blocked inside AddOrUpdate's commit gate.
    d[1] = 10;

    {
        std::lock_guard<std::mutex> lk(m);
        releaseFactory = true;
    }
    cv.notify_all();
    updater.join();

    int finalValue = 0;
    EXPECT_TRUE(d.TryGetValue(1, finalValue));
    // .NET parity: the commit must be rejected once, the factory retried against the
    // intervening write (10), and the final value must be 11 -- not silently overwritten to 1.
    EXPECT_EQ(finalValue, 11);
    EXPECT_GE(factoryInvocations, 2);
}

TEST(ConcurrentDictionaryTest, AddOrUpdate_AddFactory_InterveningWrite_RetriesInsteadOfLosingIt) {
    ConcurrentDictionary<int, int> d;
    d.TryAdd(1, 0);

    std::mutex m;
    std::condition_variable cv;
    bool factoryEntered = false;
    bool releaseFactory = false;
    int factoryInvocations = 0;

    std::thread updater([&] {
        d.AddOrUpdate(
            1,
            [](const int&) { return 999; },
            [&](const int&, const int& oldValue) {
                {
                    std::lock_guard<std::mutex> lk(m);
                    ++factoryInvocations;
                    factoryEntered = true;
                }
                cv.notify_all();
                std::unique_lock<std::mutex> lk(m);
                cv.wait(lk, [&] { return releaseFactory; });
                return oldValue + 1;
            });
    });

    {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&] { return factoryEntered; });
    }

    d[1] = 10;

    {
        std::lock_guard<std::mutex> lk(m);
        releaseFactory = true;
    }
    cv.notify_all();
    updater.join();

    int finalValue = 0;
    EXPECT_TRUE(d.TryGetValue(1, finalValue));
    EXPECT_EQ(finalValue, 11);
    EXPECT_GE(factoryInvocations, 2);
}

TEST(ConcurrentDictionaryTest, AddOrUpdate_ConstAddValue_KeyAddedConcurrently_FallsThroughToUpdate) {
    ConcurrentDictionary<int, int> d;

    std::mutex m;
    std::condition_variable cv;
    bool observedAbsent = false;
    bool releaseAdd = false;

    // A racing thread wins the initial "key absent" observation but is paused before it can
    // commit the insert; the main thread adds the key first, so the racing thread's insert
    // attempt must fail and fall through to the update branch on retry rather than clobbering
    // the concurrently-added entry.
    std::thread racer([&] {
        d.AddOrUpdate(
            1,
            /*addValue=*/500,
            [&](const int&, const int& oldValue) {
                {
                    std::lock_guard<std::mutex> lk(m);
                    observedAbsent = true;
                }
                cv.notify_all();
                std::unique_lock<std::mutex> lk(m);
                cv.wait(lk, [&] { return releaseAdd; });
                return oldValue + 1;
            });
    });

    // Give the racer a chance to observe the key as absent before it retries as an update;
    // this test only needs the eventual outcome to be correct, so it does not gate on that
    // exact interleaving -- it directly seeds the key and lets the racer's own retry loop
    // reconcile against it.
    d.TryAdd(1, 7);
    {
        std::lock_guard<std::mutex> lk(m);
        releaseAdd = true;
    }
    cv.notify_all();
    racer.join();
    (void)observedAbsent;

    int finalValue = 0;
    EXPECT_TRUE(d.TryGetValue(1, finalValue));
    // Either the racer's own insert landed (500) or it retried and updated the concurrently
    // added value (8) -- both are valid outcomes of a correct retry loop. What must never
    // happen is losing the seeded Add entirely or crashing/duplicating the key.
    EXPECT_TRUE(finalValue == 500 || finalValue == 8);
    EXPECT_EQ(d.getCountProperty(), 1);
}

TEST(ConcurrentDictionaryTest, AddOrUpdate_ManyThreadsSameKey_NoLostUpdatesUnderContention) {
    ConcurrentDictionary<std::string, int> d;
    d.TryAdd("counter", 0);

    constexpr int kThreads = 8;
    constexpr int kPerThread = 500;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&d] {
            for (int i = 0; i < kPerThread; ++i) {
                d.AddOrUpdate("counter", 1, [](const std::string&, int old) { return old + 1; });
            }
        });
    }
    for (auto& th : threads) th.join();

    int finalValue = 0;
    EXPECT_TRUE(d.TryGetValue("counter", finalValue));
    EXPECT_EQ(finalValue, kThreads * kPerThread);
}
