// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#include "System/Exception.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Threading/Channels/Channel.hpp"
#include "System/Threading/Channels/ChannelClosedException.hpp"

using namespace System::Threading::Channels;

TEST(ChannelTests, UnboundedTryWriteThenTryRead) {
    auto channel = Channel<int>::CreateUnbounded();
    EXPECT_TRUE(channel.Writer->TryWrite(42));
    int value = 0;
    EXPECT_TRUE(channel.Reader->TryRead(value));
    EXPECT_EQ(value, 42);
}

TEST(ChannelTests, TryRead_EmptyChannel_ReturnsFalse) {
    auto channel = Channel<int>::CreateUnbounded();
    int value = 0;
    EXPECT_FALSE(channel.Reader->TryRead(value));
}

TEST(ChannelTests, FifoOrdering) {
    auto channel = Channel<int>::CreateUnbounded();
    channel.Writer->TryWrite(1);
    channel.Writer->TryWrite(2);
    channel.Writer->TryWrite(3);
    int a, b, c;
    channel.Reader->TryRead(a);
    channel.Reader->TryRead(b);
    channel.Reader->TryRead(c);
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(c, 3);
}

TEST(ChannelTests, BoundedChannel_RespectsCapacity_WaitMode) {
    auto channel = Channel<int>::CreateBounded(2);
    EXPECT_TRUE(channel.Writer->TryWrite(1));
    EXPECT_TRUE(channel.Writer->TryWrite(2));
    EXPECT_FALSE(channel.Writer->TryWrite(3)); // full, Wait mode rejects synchronous TryWrite
}

TEST(ChannelTests, BoundedChannel_DropOldest) {
    BoundedChannelOptions options(2);
    options.FullMode = BoundedChannelFullMode::DropOldest;
    auto channel = Channel<int>::CreateBounded(options);
    channel.Writer->TryWrite(1);
    channel.Writer->TryWrite(2);
    EXPECT_TRUE(channel.Writer->TryWrite(3)); // drops 1, keeps [2,3]
    int a, b;
    channel.Reader->TryRead(a);
    channel.Reader->TryRead(b);
    EXPECT_EQ(a, 2);
    EXPECT_EQ(b, 3);
}

// Verified against BoundedChannel.cs's DropNewest handling (DequeueTail): the item removed to
// make room is the most-recently-written one, not the oldest -- easy to accidentally swap with
// DropOldest's DequeueHead, so this pins the distinction explicitly rather than relying only on
// DropOldest's existing coverage.
TEST(ChannelTests, BoundedChannel_DropNewest) {
    BoundedChannelOptions options(2);
    options.FullMode = BoundedChannelFullMode::DropNewest;
    auto channel = Channel<int>::CreateBounded(options);
    channel.Writer->TryWrite(1);
    channel.Writer->TryWrite(2);
    EXPECT_TRUE(channel.Writer->TryWrite(3)); // drops 2 (newest), keeps [1,3]
    int a, b;
    channel.Reader->TryRead(a);
    channel.Reader->TryRead(b);
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 3);
}

TEST(ChannelTests, BoundedChannel_DropWrite) {
    BoundedChannelOptions options(1);
    options.FullMode = BoundedChannelFullMode::DropWrite;
    auto channel = Channel<int>::CreateBounded(options);
    channel.Writer->TryWrite(1);
    EXPECT_TRUE(channel.Writer->TryWrite(2)); // "handled" (dropped), item 1 stays
    int a;
    channel.Reader->TryRead(a);
    EXPECT_EQ(a, 1);
    EXPECT_FALSE(channel.Reader->TryRead(a));
}

TEST(ChannelTests, TryComplete_ThenTryWrite_Fails) {
    auto channel = Channel<int>::CreateUnbounded();
    EXPECT_TRUE(channel.Writer->TryComplete());
    EXPECT_FALSE(channel.Writer->TryWrite(1));
    EXPECT_FALSE(channel.Writer->TryComplete()); // already completed
}

TEST(ChannelTests, WaitToReadAsync_UnblocksWhenItemWritten) {
    auto channel = Channel<int>::CreateUnbounded();
    auto waitTask = channel.Reader->WaitToReadAsync();
    std::thread writer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        channel.Writer->TryWrite(99);
    });
    EXPECT_TRUE(waitTask.getResultProperty());
    int value = 0;
    EXPECT_TRUE(channel.Reader->TryRead(value));
    EXPECT_EQ(value, 99);
    writer.join();
}

TEST(ChannelTests, WaitToReadAsync_ReturnsFalse_WhenClosedEmpty) {
    auto channel = Channel<int>::CreateUnbounded();
    channel.Writer->TryComplete();
    EXPECT_FALSE(channel.Reader->WaitToReadAsync().getResultProperty());
}

// Regression test for a wave-3 audit finding: WaitToReadAsync() never inspected closeError, so
// completing the channel with an exception and an empty queue made it return false (as if
// gracefully closed) instead of faulting -- the real error was only observable via the
// separate getCompletionProperty() task, not the primary read path. Verified against
// UnboundedChannel.cs's WaitToReadAsync, which returns Task.FromException when the channel
// completed with a non-null error.
TEST(ChannelTests, WaitToReadAsync_ThrowsCompletionError_WhenClosedWithException) {
    auto channel = Channel<int>::CreateUnbounded();
    channel.Writer->TryComplete(std::make_exception_ptr(std::runtime_error("boom")));
    EXPECT_THROW(channel.Reader->WaitToReadAsync().getResultProperty(), std::runtime_error);
}

TEST(ChannelTests, WaitToWriteAsync_ThrowsCompletionError_WhenClosedWithException) {
    auto channel = Channel<int>::CreateUnbounded();
    channel.Writer->TryComplete(std::make_exception_ptr(std::runtime_error("boom")));
    EXPECT_THROW(channel.Writer->WaitToWriteAsync().getResultProperty(), std::runtime_error);
}

TEST(ChannelTests, ReadAsync_ThrowsChannelClosedException_WhenClosedEmpty) {
    auto channel = Channel<int>::CreateUnbounded();
    channel.Writer->TryComplete();
    EXPECT_THROW(channel.Reader->ReadAsync().getResultProperty(), ChannelClosedException);
}

TEST(ChannelTests, ReadAsync_ReturnsItemWrittenConcurrently) {
    auto channel = Channel<int>::CreateUnbounded();
    auto readTask = channel.Reader->ReadAsync();
    std::thread writer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        channel.Writer->TryWrite(7);
    });
    EXPECT_EQ(readTask.getResultProperty(), 7);
    writer.join();
}

TEST(ChannelTests, WriteAsync_BlocksUntilSpaceAvailable) {
    auto channel = Channel<int>::CreateBounded(1);
    channel.Writer->TryWrite(1);
    auto writeTask = channel.Writer->WriteAsync(2);

    int value = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    channel.Reader->TryRead(value); // frees a slot, allowing the pending WriteAsync to complete
    writeTask.Wait();
    EXPECT_EQ(value, 1);

    int second = 0;
    EXPECT_TRUE(channel.Reader->TryRead(second));
    EXPECT_EQ(second, 2);
}

// Regression test for a wave-3 audit finding: the default ChannelReader<T>::ReadAsync()/
// ChannelWriter<T>::WriteAsync() captured a raw `this` pointer into a lambda that runs on a
// background thread (Task's std::async(std::launch::async, ...)) outlasting the synchronous
// call. If the caller drops every other shared_ptr reference to the Channel/Reader/Writer
// right after issuing the call -- a realistic "fire and forget" pattern -- the background
// thread could still be executing against an already-destroyed object: a genuine
// heap-use-after-free, confirmed via a standalone AddressSanitizer repro against the pre-fix
// code (reliably crashed on the very first iteration). Fixed by having ChannelReader<T>/
// ChannelWriter<T> inherit std::enable_shared_from_this and capture shared_from_this()
// instead of `this`.
TEST(ChannelTests, WriteAsync_ChannelDroppedImmediately_StillCompletesSafely) {
    System::Threading::Tasks::Task task;
    {
        auto channel = Channel<int>::CreateBounded(1);
        task = channel.Writer->WriteAsync(42);
        // `channel` (and its shared_ptr Reader/Writer) is destroyed here. `task` is the only
        // thing keeping the write's background work referenced; before the fix, that
        // background work referenced the (now being destroyed) ChannelWriterImpl directly.
    }
    task.Wait();
    EXPECT_TRUE(task.getIsCompletedProperty());
    EXPECT_FALSE(task.getIsFaultedProperty());
}

static System::Threading::Tasks::TaskT<int> IssueReadAsyncThenDropChannel() {
    auto channel = Channel<int>::CreateUnbounded();
    channel.Writer->TryWrite(99);
    return channel.Reader->ReadAsync();
    // `channel` (and its shared_ptr Reader/Writer) is destroyed here, on function return.
}

TEST(ChannelTests, ReadAsync_ChannelDroppedImmediately_StillCompletesSafely) {
    auto task = IssueReadAsyncThenDropChannel();
    EXPECT_EQ(task.getResultProperty(), 99);
}

// REWRITTEN by ticket #1968 (SR-AUD-233). The two tests that stood here --
// ZeroCapacityChannel_TryWrite_SucceedsOnceThenBlocksLikeCapacityOne and
// ZeroCapacityChannel_WriteAsync_UnblocksOnceReaderDrains -- asserted that a zero-capacity
// bounded channel behaves like a one-element buffer. That behaviour is the defect: a
// zero-capacity channel is a RENDEZVOUS channel, so a write succeeds only when a reader is
// already waiting for it. The assertions below replace those expectations rather than deleting
// the coverage; each of the old tests' concerns (synchronous TryWrite, and a WriteAsync that
// completes once a peer arrives) still has a test here, asserting the corrected contract.
TEST(ChannelTests, ZeroCapacityChannel_TryWrite_FailsWithoutAWaitingReader) {
    auto channel = Channel<int>::CreateBounded(0);
    EXPECT_FALSE(channel.Writer->TryWrite(1));
    EXPECT_EQ(channel.Reader->getCountProperty(), 0);
    int value = 0;
    EXPECT_FALSE(channel.Reader->TryRead(value));
    // Still false on a second attempt: nothing was buffered by the first.
    EXPECT_FALSE(channel.Writer->TryWrite(2));
    EXPECT_EQ(channel.Reader->getCountProperty(), 0);
}

TEST(ChannelTests, ZeroCapacityChannel_WriteAsync_CompletesOnlyOnceAReaderArrives) {
    auto channel = Channel<int>::CreateBounded(0);
    auto writeTask = channel.Writer->WriteAsync(2);

    // The write must NOT complete while no reader is waiting. Bounded observation window, no
    // dependence on a sleep for correctness: the assertion that follows the read is what proves
    // the hand-off, and this only guards against an immediate completion.
    std::atomic<bool> completed{false};
    std::thread waiter([&] {
        writeTask.Wait();
        completed.store(true);
    });
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
    while (std::chrono::steady_clock::now() < until && !completed.load()) { }
    EXPECT_FALSE(completed.load()) << "a rendezvous write completed with no reader waiting";

    EXPECT_EQ(channel.Reader->ReadAsync().getResultProperty(), 2);
    waiter.join();
    EXPECT_TRUE(completed.load());
    EXPECT_EQ(channel.Reader->getCountProperty(), 0);
}

TEST(ChannelTests, Completion_CompletesOnceClosedAndDrained) {
    auto channel = Channel<int>::CreateUnbounded();
    channel.Writer->TryWrite(1);
    channel.Writer->TryComplete();

    std::thread drainer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        int v;
        channel.Reader->TryRead(v);
    });
    channel.Reader->getCompletionProperty().Wait();
    drainer.join();
}

TEST(ChannelTests, CanCountAndCount) {
    auto channel = Channel<int>::CreateUnbounded();
    EXPECT_TRUE(channel.Reader->getCanCountProperty());
    channel.Writer->TryWrite(1);
    channel.Writer->TryWrite(2);
    EXPECT_EQ(channel.Reader->getCountProperty(), 2);
}

TEST(ChannelTests, ChannelClosedException_DefaultMessage) {
    ChannelClosedException ex;
    EXPECT_STREQ(ex.what(), "The channel has been closed.");
}

// Ticket 1726 (post-stabilization-audit): all prior std::thread-using tests above are
// single-writer/single-reader coordination checks. Channel<T> is exactly the kind of primitive
// real .NET code uses for fan-in/fan-out, so this exercises genuine multi-producer/
// multi-consumer contention: N writer threads each TryWrite a distinct block of values (using an
// unbounded channel, so TryWrite always succeeds immediately -- no need to also race
// WaitToWriteAsync backpressure here, that's covered by the existing bounded-channel tests
// above), M reader threads race TryRead against each other and against getIsCompletedProperty()
// once the writers finish and TryComplete() is called, verifying the total items read matches
// total written with no duplicates or losses.
TEST(ChannelTests, ConcurrentMultiWriterMultiReader_NoLostOrDuplicatedItems) {
    auto channel = Channel<int>::CreateUnbounded();
    constexpr int kWriters = 4;
    constexpr int kReaders = 4;
    constexpr int kPerWriter = 2000;
    constexpr int kTotal = kWriters * kPerWriter;

    std::vector<std::thread> writers;
    for (int w = 0; w < kWriters; ++w) {
        writers.emplace_back([&channel, w]() {
            for (int i = 0; i < kPerWriter; ++i) {
                EXPECT_TRUE(channel.Writer->TryWrite(w * kPerWriter + i));
            }
        });
    }
    for (auto& th : writers) th.join();
    channel.Writer->TryComplete();

    std::atomic<int> readCount{0};
    std::vector<int> seenPerValue(kTotal, 0);
    std::mutex seenMutex;
    std::vector<std::thread> readers;
    for (int r = 0; r < kReaders; ++r) {
        readers.emplace_back([&]() {
            int value;
            while (channel.Reader->TryRead(value)) {
                readCount.fetch_add(1, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(seenMutex);
                ASSERT_GE(value, 0);
                ASSERT_LT(value, kTotal);
                seenPerValue[static_cast<size_t>(value)]++;
            }
        });
    }
    for (auto& th : readers) th.join();

    EXPECT_EQ(readCount.load(), kTotal);
    for (int v = 0; v < kTotal; ++v) {
        EXPECT_EQ(seenPerValue[static_cast<size_t>(v)], 1) << "value " << v << " seen "
            << seenPerValue[static_cast<size_t>(v)] << " times (expected exactly once)";
    }
}

TEST(ChannelTests, MultipleBlockedReaders_AllUnblockWithinBoundedTime) {
    // Regression test for a lost-wakeup bug (fixed 2026-07-14, confirmed via a standalone
    // repro): TryWrite used to call notify_one(), which wakes at most one blocked
    // WaitToReadAsync/ReadAsync waiter. With multiple readers concurrently blocked, that could
    // permanently starve any reader notify_one() doesn't happen to pick, even though real
    // .NET's WaitToReadAsync contract notifies every currently-pending registration on a write,
    // not just one. Uses a bounded poll-with-timeout (not a plain join()) so a regression fails
    // this test instead of hanging the suite -- any reader thread still running at the deadline
    // is detached, not joined, to avoid std::terminate on a joinable thread's destructor. See
    // PrioritizedChannelTests.MultipleBlockedReaders_AllUnblockWithinBoundedTime for the
    // equivalent coverage of PrioritizedChannelWriterImpl::TryWrite's identical fix.
    auto channel = Channel<int>::CreateUnbounded();
    constexpr int kReaders = 6;

    std::vector<std::atomic<bool>> done(kReaders);
    for (auto& d : done) d = false;
    std::vector<std::thread> readers;
    for (int r = 0; r < kReaders; ++r) {
        readers.emplace_back([&channel, &done, r]() {
            channel.Reader->ReadAsync().getResultProperty();
            done[static_cast<size_t>(r)] = true;
        });
    }

    // Give every reader time to actually block inside WaitToReadAsync's condition_variable::wait().
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    for (int i = 0; i < kReaders; ++i) {
        EXPECT_TRUE(channel.Writer->TryWrite(i));
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    auto allDone = [&] {
        return std::all_of(done.begin(), done.end(), [](const std::atomic<bool>& d) { return d.load(); });
    };
    while (!allDone() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (int r = 0; r < kReaders; ++r) {
        EXPECT_TRUE(done[static_cast<size_t>(r)].load()) << "reader " << r << " never unblocked";
    }
    for (int r = 0; r < kReaders; ++r) {
        if (done[static_cast<size_t>(r)]) readers[static_cast<size_t>(r)].join();
        else readers[static_cast<size_t>(r)].detach();
    }
}

// -----------------------------------------------------------------------
// Channel<T>::CreateUnboundedPrioritized
// -----------------------------------------------------------------------

TEST(PrioritizedChannelTests, DefaultComparer_DequeuesSmallestFirst) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    channel.Writer->TryWrite(5);
    channel.Writer->TryWrite(1);
    channel.Writer->TryWrite(3);
    int a, b, c;
    ASSERT_TRUE(channel.Reader->TryRead(a));
    ASSERT_TRUE(channel.Reader->TryRead(b));
    ASSERT_TRUE(channel.Reader->TryRead(c));
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 3);
    EXPECT_EQ(c, 5);
}

TEST(PrioritizedChannelTests, WriteOrderDoesNotAffectReadOrder) {
    // Priority order, not insertion order -- distinguishes this from the plain FIFO channel.
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    for (int v : {10, 2, 8, 4, 6}) channel.Writer->TryWrite(v);
    std::vector<int> got;
    int value;
    while (channel.Reader->TryRead(value)) got.push_back(value);
    EXPECT_EQ(got, (std::vector<int>{2, 4, 6, 8, 10}));
}

TEST(PrioritizedChannelTests, CustomComparer_DescendingOrder) {
    UnboundedPrioritizedChannelOptions<int> options;
    options.Comparer = [](const int& a, const int& b) { return a > b; }; // largest first
    auto channel = Channel<int>::CreateUnboundedPrioritized(options);
    channel.Writer->TryWrite(1);
    channel.Writer->TryWrite(5);
    channel.Writer->TryWrite(3);
    int a, b, c;
    ASSERT_TRUE(channel.Reader->TryRead(a));
    ASSERT_TRUE(channel.Reader->TryRead(b));
    ASSERT_TRUE(channel.Reader->TryRead(c));
    EXPECT_EQ(a, 5);
    EXPECT_EQ(b, 3);
    EXPECT_EQ(c, 1);
}

TEST(PrioritizedChannelTests, TryRead_EmptyChannel_ReturnsFalse) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    int value;
    EXPECT_FALSE(channel.Reader->TryRead(value));
}

TEST(PrioritizedChannelTests, TryPeek_DoesNotRemoveItem) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    channel.Writer->TryWrite(7);
    channel.Writer->TryWrite(2);
    int peeked = 0;
    EXPECT_TRUE(channel.Reader->TryPeek(peeked));
    EXPECT_EQ(peeked, 2);
    EXPECT_EQ(channel.Reader->getCountProperty(), 2); // peek didn't remove it
    int read = 0;
    EXPECT_TRUE(channel.Reader->TryRead(read));
    EXPECT_EQ(read, 2);
}

TEST(PrioritizedChannelTests, CanPeekAndCanCountAreTrue) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    EXPECT_TRUE(channel.Reader->getCanPeekProperty());
    EXPECT_TRUE(channel.Reader->getCanCountProperty());
}

TEST(PrioritizedChannelTests, TryComplete_ThenTryWrite_Fails) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    EXPECT_TRUE(channel.Writer->TryComplete());
    EXPECT_FALSE(channel.Writer->TryWrite(1));
    EXPECT_FALSE(channel.Writer->TryComplete()); // already completed
}

TEST(PrioritizedChannelTests, WaitToWriteAsync_AlwaysTrueUntilClosed) {
    // Unbounded: writing never blocks on capacity, matching CreateUnbounded()'s own contract.
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    EXPECT_TRUE(channel.Writer->WaitToWriteAsync().getResultProperty());
    channel.Writer->TryComplete();
    EXPECT_FALSE(channel.Writer->WaitToWriteAsync().getResultProperty());
}

TEST(PrioritizedChannelTests, WaitToReadAsync_UnblocksWhenItemWritten) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    std::thread writer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        channel.Writer->TryWrite(9);
    });
    EXPECT_TRUE(channel.Reader->WaitToReadAsync().getResultProperty());
    writer.join();
    int value = 0;
    EXPECT_TRUE(channel.Reader->TryRead(value));
    EXPECT_EQ(value, 9);
}

TEST(PrioritizedChannelTests, WaitToReadAsync_ReturnsFalse_WhenClosedEmpty) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    channel.Writer->TryComplete();
    EXPECT_FALSE(channel.Reader->WaitToReadAsync().getResultProperty());
}

TEST(PrioritizedChannelTests, WaitToReadAsync_ThrowsCompletionError_WhenClosedWithException) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    channel.Writer->TryComplete(std::make_exception_ptr(std::runtime_error("boom")));
    EXPECT_THROW(channel.Reader->WaitToReadAsync().getResultProperty(), std::runtime_error);
}

TEST(PrioritizedChannelTests, Completion_CompletesOnceClosedAndDrained) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    channel.Writer->TryWrite(1);
    channel.Writer->TryComplete();
    auto completion = channel.Reader->getCompletionProperty();
    EXPECT_FALSE(completion.getIsCompletedProperty());
    int value;
    channel.Reader->TryRead(value);
    completion.Wait();
    EXPECT_TRUE(completion.getIsCompletedSuccessfullyProperty());
}

TEST(PrioritizedChannelTests, ReadAsync_ReturnsHighestPriorityItem) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    channel.Writer->TryWrite(9);
    channel.Writer->TryWrite(1);
    EXPECT_EQ(channel.Reader->ReadAsync().getResultProperty(), 1);
}

TEST(PrioritizedChannelTests, MultipleBlockedReaders_AllUnblockWithinBoundedTime) {
    // Regression test for a lost-wakeup bug (fixed 2026-07-14, confirmed via a standalone
    // repro): TryWrite used to call notify_one(), which wakes at most one blocked
    // WaitToReadAsync/ReadAsync waiter. With multiple readers concurrently blocked, that could
    // permanently starve any reader notify_one() doesn't happen to pick, even though real
    // .NET's WaitToReadAsync contract notifies every currently-pending registration on a write,
    // not just one. Uses a bounded poll-with-timeout (not a plain join()) so a regression fails
    // this test instead of hanging the suite -- any reader thread still running at the deadline
    // is detached, not joined, to avoid std::terminate on a joinable thread's destructor.
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    constexpr int kReaders = 6;

    std::vector<std::atomic<bool>> done(kReaders);
    for (auto& d : done) d = false;
    std::vector<std::thread> readers;
    for (int r = 0; r < kReaders; ++r) {
        readers.emplace_back([&channel, &done, r]() {
            channel.Reader->ReadAsync().getResultProperty();
            done[static_cast<size_t>(r)] = true;
        });
    }

    // Give every reader time to actually block inside WaitToReadAsync's condition_variable::wait().
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    for (int i = 0; i < kReaders; ++i) {
        EXPECT_TRUE(channel.Writer->TryWrite(i));
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    auto allDone = [&] {
        return std::all_of(done.begin(), done.end(), [](const std::atomic<bool>& d) { return d.load(); });
    };
    while (!allDone() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (int r = 0; r < kReaders; ++r) {
        EXPECT_TRUE(done[static_cast<size_t>(r)].load()) << "reader " << r << " never unblocked";
    }
    for (int r = 0; r < kReaders; ++r) {
        if (done[static_cast<size_t>(r)]) readers[static_cast<size_t>(r)].join();
        else readers[static_cast<size_t>(r)].detach();
    }
}

TEST(PrioritizedChannelTests, ConcurrentMultiWriterMultiReader_NoLostOrDuplicatedItems) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    constexpr int kWriters = 4;
    constexpr int kPerWriter = 250;
    constexpr int kTotal = kWriters * kPerWriter;
    constexpr int kReaders = 4;

    std::vector<std::thread> writers;
    for (int w = 0; w < kWriters; ++w) {
        writers.emplace_back([&, w]() {
            for (int i = 0; i < kPerWriter; ++i) {
                channel.Writer->TryWrite(w * kPerWriter + i);
            }
        });
    }
    for (auto& th : writers) th.join();
    channel.Writer->TryComplete();

    std::atomic<int> readCount{0};
    std::vector<int> seenPerValue(kTotal, 0);
    std::mutex seenMutex;
    std::vector<std::thread> readers;
    for (int r = 0; r < kReaders; ++r) {
        readers.emplace_back([&]() {
            int value;
            while (channel.Reader->TryRead(value)) {
                readCount.fetch_add(1, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(seenMutex);
                ASSERT_GE(value, 0);
                ASSERT_LT(value, kTotal);
                seenPerValue[static_cast<size_t>(value)]++;
            }
        });
    }
    for (auto& th : readers) th.join();

    EXPECT_EQ(readCount.load(), kTotal);
    for (int v = 0; v < kTotal; ++v) {
        EXPECT_EQ(seenPerValue[static_cast<size_t>(v)], 1) << "value " << v << " seen "
            << seenPerValue[static_cast<size_t>(v)] << " times (expected exactly once)";
    }
}

// =============================================================================================
// Ticket #1967 (SR-AUD-234, cause TC-C) -- the closed-channel exception boundary
//
// When a writer completes a channel WITH an error, ReadAsync used to rethrow that error
// directly. .NET routes a closed channel through ChannelUtilities.GetInvalidCompletionValueTask,
// which produces a ChannelClosedException whose InnerException is the supplied error, so a
// caller can tell "the channel is closed" from "the producer failed" while still reaching the
// cause. WaitToReadAsync, WaitToWriteAsync and Completion correctly expose the error unwrapped
// and must keep doing so.
//
// Correction to the finding, measured rather than assumed: WriteAsync had the IDENTICAL defect,
// which SR-AUD-234 does not mention. Repaired with it, as the same defect at a second site; no
// new SR-AUD identifier was issued.
// =============================================================================================

namespace {

    std::exception_ptr makeBoom() { return std::make_exception_ptr(std::runtime_error("boom")); }

    // Asserts the outer type is ChannelClosedException and its inner exception is the writer's
    // own error, with its message intact.
    void expectClosedWithCause(const std::function<void()>& call) {
        bool threw = false;
        try {
            call();
        } catch (const ChannelClosedException& e) {
            threw = true;
            EXPECT_EQ(std::string(e.getMessageProperty()), "The channel has been closed.");
            std::exception_ptr inner = e.getInnerExceptionProperty();
            ASSERT_TRUE(static_cast<bool>(inner)) << "the producer's error was not retained";
            try {
                std::rethrow_exception(inner);
            } catch (const std::runtime_error& cause) {
                EXPECT_STREQ(cause.what(), "boom");
            } catch (...) {
                FAIL() << "inner exception is not the writer's original error";
            }
        }
        EXPECT_TRUE(threw) << "expected ChannelClosedException";
    }

} // namespace

TEST(ChannelClosedBoundaryTests, ReadAsync_ErrorCompleted_ThrowsChannelClosedWithCause) {
    auto channel = Channel<int>::CreateUnbounded();
    channel.Writer->Complete(makeBoom());
    expectClosedWithCause([&] { (void)channel.Reader->ReadAsync().getResultProperty(); });
}

TEST(ChannelClosedBoundaryTests, ReadAsync_ErrorCompletedBounded_ThrowsChannelClosedWithCause) {
    auto channel = Channel<int>::CreateBounded(4);
    channel.Writer->Complete(makeBoom());
    expectClosedWithCause([&] { (void)channel.Reader->ReadAsync().getResultProperty(); });
}

TEST(PrioritizedChannelClosedBoundaryTests, ReadAsync_ErrorCompleted_ThrowsChannelClosedWithCause) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    channel.Writer->Complete(makeBoom());
    expectClosedWithCause([&] { (void)channel.Reader->ReadAsync().getResultProperty(); });
}

// The writer's mirror-image entry point -- the site the finding does not name.
TEST(ChannelClosedBoundaryTests, WriteAsync_ErrorCompleted_ThrowsChannelClosedWithCause) {
    auto channel = Channel<int>::CreateUnbounded();
    channel.Writer->Complete(makeBoom());
    expectClosedWithCause([&] { channel.Writer->WriteAsync(1).Wait(); });
}

TEST(PrioritizedChannelClosedBoundaryTests, WriteAsync_ErrorCompleted_ThrowsChannelClosedWithCause) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    channel.Writer->Complete(makeBoom());
    expectClosedWithCause([&] { channel.Writer->WriteAsync(1).Wait(); });
}

// The three sibling paths must keep exposing the cause UNWRAPPED -- that is .NET's contract for
// them, and it is what makes the wrapper meaningful on ReadAsync/WriteAsync.
TEST(ChannelClosedBoundaryTests, SiblingPaths_StillExposeTheCauseUnwrapped) {
    auto channel = Channel<int>::CreateUnbounded();
    channel.Writer->Complete(makeBoom());
    EXPECT_THROW((void)channel.Reader->WaitToReadAsync().getResultProperty(), std::runtime_error);
    EXPECT_THROW((void)channel.Writer->WaitToWriteAsync().getResultProperty(), std::runtime_error);
    EXPECT_THROW(channel.Reader->getCompletionProperty().Wait(), std::runtime_error);
}

TEST(PrioritizedChannelClosedBoundaryTests, SiblingPaths_StillExposeTheCauseUnwrapped) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    channel.Writer->Complete(makeBoom());
    EXPECT_THROW((void)channel.Reader->WaitToReadAsync().getResultProperty(), std::runtime_error);
    EXPECT_THROW((void)channel.Writer->WaitToWriteAsync().getResultProperty(), std::runtime_error);
    EXPECT_THROW(channel.Reader->getCompletionProperty().Wait(), std::runtime_error);
}

// A CLEAN completion keeps the plain ChannelClosedException with no inner exception.
TEST(ChannelClosedBoundaryTests, CleanCompletion_ThrowsChannelClosedWithoutCause) {
    auto channel = Channel<int>::CreateUnbounded();
    channel.Writer->Complete();
    try {
        (void)channel.Reader->ReadAsync().getResultProperty();
        FAIL() << "expected ChannelClosedException";
    } catch (const ChannelClosedException& e) {
        EXPECT_FALSE(static_cast<bool>(e.getInnerExceptionProperty()));
    }
    try {
        channel.Writer->WriteAsync(1).Wait();
        FAIL() << "expected ChannelClosedException";
    } catch (const ChannelClosedException& e) {
        EXPECT_FALSE(static_cast<bool>(e.getInnerExceptionProperty()));
    }
}

// Buffered items are handed over BEFORE the closure is reported, on both channel shapes.
TEST(ChannelClosedBoundaryTests, ErrorCompleted_StillDrainsBufferedItemsFirst) {
    auto channel = Channel<int>::CreateUnbounded();
    EXPECT_TRUE(channel.Writer->TryWrite(7));
    channel.Writer->Complete(makeBoom());
    EXPECT_EQ(channel.Reader->ReadAsync().getResultProperty(), 7);
    expectClosedWithCause([&] { (void)channel.Reader->ReadAsync().getResultProperty(); });
}

TEST(PrioritizedChannelClosedBoundaryTests, ErrorCompleted_StillDrainsBufferedItemsFirst) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    EXPECT_TRUE(channel.Writer->TryWrite(3));
    channel.Writer->Complete(makeBoom());
    EXPECT_EQ(channel.Reader->ReadAsync().getResultProperty(), 3);
    expectClosedWithCause([&] { (void)channel.Reader->ReadAsync().getResultProperty(); });
}

// A still-open channel is untouched, and so are the synchronous try-methods.
TEST(ChannelClosedBoundaryTests, OpenChannelAndTryMethods_AreUnchanged) {
    auto open = Channel<int>::CreateUnbounded();
    EXPECT_TRUE(open.Writer->TryWrite(42));
    EXPECT_EQ(open.Reader->ReadAsync().getResultProperty(), 42);

    auto closed = Channel<int>::CreateUnbounded();
    closed.Writer->Complete(makeBoom());
    EXPECT_FALSE(closed.Writer->TryWrite(1));
    int value = 0;
    EXPECT_FALSE(closed.Reader->TryRead(value));
}

// ChannelClosedException stays an InvalidOperationException, as .NET's does, so existing
// handlers keep working.
TEST(ChannelClosedBoundaryTests, WrappedException_KeepsItsHierarchy) {
    auto channel = Channel<int>::CreateUnbounded();
    channel.Writer->Complete(makeBoom());
    const auto read = [&] { (void)channel.Reader->ReadAsync().getResultProperty(); };
    EXPECT_THROW(read(), ChannelClosedException);
    EXPECT_THROW(read(), System::InvalidOperationException);
    EXPECT_THROW(read(), System::Exception);
}

// A reader blocked BEFORE the error completion arrives takes the same path -- deterministic
// hand-off, no sleep-based timing.
TEST(ChannelClosedBoundaryTests, BlockedReader_ErrorCompletedWhileWaiting_ThrowsChannelClosedWithCause) {
    auto channel = Channel<int>::CreateUnbounded();
    std::atomic<bool> readerIssued{false};
    auto readTask = channel.Reader->ReadAsync();
    readerIssued.store(true);
    while (!readerIssued.load()) { }
    channel.Writer->Complete(makeBoom());
    expectClosedWithCause([&] { (void)readTask.getResultProperty(); });
}

// =============================================================================================
// Ticket #1968 (SR-AUD-233, cause TC-B/2) -- a zero-capacity bounded channel is a rendezvous
//
// Before this ticket ChannelState::effectiveCapacity() rewrote a configured capacity of 0 to 1,
// so CreateBounded(0).Writer->TryWrite(7) returned true and a reader immediately retrieved 7 --
// the port advertised a bounded-memory contract it did not keep, and did so silently. A
// zero-capacity channel holds nothing: a write succeeds only when a reader is already waiting,
// and the item is handed straight to it.
// =============================================================================================

TEST(ZeroCapacityRendezvousTests, NoPeer_TryWriteAndTryReadBothFail) {
    auto channel = Channel<int>::CreateBounded(0);
    EXPECT_FALSE(channel.Writer->TryWrite(1));
    int value = 0;
    EXPECT_FALSE(channel.Reader->TryRead(value));
    EXPECT_EQ(channel.Reader->getCountProperty(), 0);
}

TEST(ZeroCapacityRendezvousTests, BlockedReader_IsHandedTheWritersItemDirectly) {
    auto channel = Channel<int>::CreateBounded(0);
    auto readTask = channel.Reader->ReadAsync();

    // A succeeding TryWrite IS the observation that a reader is parked -- on a rendezvous
    // channel it cannot succeed otherwise -- so this loop is a deterministic rendezvous, not a
    // timing guess. The deadline only bounds a failure.
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool wrote = false;
    while (std::chrono::steady_clock::now() < until) {
        if (channel.Writer->TryWrite(11)) { wrote = true; break; }
    }
    ASSERT_TRUE(wrote) << "a parked reader never made the channel writable";
    EXPECT_EQ(readTask.getResultProperty(), 11);
    EXPECT_EQ(channel.Reader->getCountProperty(), 0);
}

TEST(ZeroCapacityRendezvousTests, OnlyOneWritePerWaitingReaderIsAccepted) {
    auto channel = Channel<int>::CreateBounded(0);
    auto readTask = channel.Reader->ReadAsync();

    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool first = false;
    while (std::chrono::steady_clock::now() < until) {
        if (channel.Writer->TryWrite(1)) { first = true; break; }
    }
    ASSERT_TRUE(first);
    // The single waiting reader's slot is taken; a second write has no peer left.
    EXPECT_FALSE(channel.Writer->TryWrite(2));
    EXPECT_EQ(readTask.getResultProperty(), 1);
}

TEST(ZeroCapacityRendezvousTests, MultipleReadersEachReceiveExactlyOneItem) {
    auto channel = Channel<int>::CreateBounded(0);
    constexpr int kReaders = 4;
    std::vector<System::Threading::Tasks::TaskT<int>> readers;
    readers.reserve(kReaders);
    for (int i = 0; i < kReaders; ++i) readers.push_back(channel.Reader->ReadAsync());

    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    int written = 0;
    while (written < kReaders && std::chrono::steady_clock::now() < until) {
        if (channel.Writer->TryWrite(100 + written)) ++written;
    }
    ASSERT_EQ(written, kReaders);

    std::vector<int> received;
    for (auto& r : readers) received.push_back(r.getResultProperty());
    std::sort(received.begin(), received.end());
    EXPECT_EQ(received, (std::vector<int>{100, 101, 102, 103}));
    EXPECT_EQ(channel.Reader->getCountProperty(), 0);
}

TEST(ZeroCapacityRendezvousTests, WaitToWriteAsync_UnblocksWhenAReaderArrives) {
    auto channel = Channel<int>::CreateBounded(0);
    auto waitTask = channel.Writer->WaitToWriteAsync();

    std::atomic<bool> ready{false};
    std::thread waiter([&] {
        (void)waitTask.getResultProperty();
        ready.store(true);
    });

    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
    while (std::chrono::steady_clock::now() < until && !ready.load()) { }
    EXPECT_FALSE(ready.load()) << "WaitToWriteAsync completed with no reader waiting";

    auto readTask = channel.Reader->ReadAsync();
    waiter.join();
    EXPECT_TRUE(ready.load());

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        if (channel.Writer->TryWrite(5)) break;
    }
    EXPECT_EQ(readTask.getResultProperty(), 5);
}

// Every drop mode keeps Count at 0, because a rendezvous channel has no room to hold anything:
// DropWrite drops the incoming item by definition, and with an always-empty buffer that item is
// simultaneously the newest and the oldest. Before this ticket all three buffered an item.
TEST(ZeroCapacityRendezvousTests, DropModes_DiscardTheItemAndKeepCountAtZero) {
    for (auto mode : {BoundedChannelFullMode::DropWrite, BoundedChannelFullMode::DropNewest,
                      BoundedChannelFullMode::DropOldest}) {
        BoundedChannelOptions options(0);
        options.FullMode = mode;
        auto channel = Channel<int>::CreateBounded(options);
        EXPECT_TRUE(channel.Writer->TryWrite(1)) << "drop modes report the write as handled";
        EXPECT_EQ(channel.Reader->getCountProperty(), 0);
        int value = 0;
        EXPECT_FALSE(channel.Reader->TryRead(value));
    }
}

TEST(ZeroCapacityRendezvousTests, ClosedChannel_BehavesAsBefore) {
    auto channel = Channel<int>::CreateBounded(0);
    channel.Writer->Complete();
    EXPECT_FALSE(channel.Writer->TryWrite(1));
    EXPECT_THROW((void)channel.Reader->ReadAsync().getResultProperty(), ChannelClosedException);
    EXPECT_FALSE(channel.Reader->WaitToReadAsync().getResultProperty());
}

// A reader parked on a rendezvous channel that is then closed must still unblock -- the
// waiting-peer bookkeeping must not be able to strand it.
TEST(ZeroCapacityRendezvousTests, BlockedReader_IsReleasedByCompletion) {
    auto channel = Channel<int>::CreateBounded(0);
    auto readTask = channel.Reader->ReadAsync();
    channel.Writer->Complete();
    EXPECT_THROW((void)readTask.getResultProperty(), ChannelClosedException);
    // The registration was undone, so the channel is not left believing a peer is waiting.
    EXPECT_FALSE(channel.Writer->TryWrite(1));
}

// An error completion while a reader is parked takes #1967's wrapped path, not a stranded wait.
TEST(ZeroCapacityRendezvousTests, BlockedReader_IsReleasedByErrorCompletion) {
    auto channel = Channel<int>::CreateBounded(0);
    auto readTask = channel.Reader->ReadAsync();
    channel.Writer->Complete(std::make_exception_ptr(std::runtime_error("boom")));
    expectClosedWithCause([&] { (void)readTask.getResultProperty(); });
}

// Non-zero capacities are untouched -- the whole point of the repair is that it is confined to
// capacity 0.
TEST(ZeroCapacityRendezvousTests, NonZeroCapacities_AreUnchanged) {
    auto one = Channel<int>::CreateBounded(1);
    EXPECT_TRUE(one.Writer->TryWrite(1));
    EXPECT_FALSE(one.Writer->TryWrite(2));
    EXPECT_EQ(one.Reader->getCountProperty(), 1);
    int value = 0;
    EXPECT_TRUE(one.Reader->TryRead(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(one.Writer->TryWrite(3));

    auto three = Channel<int>::CreateBounded(3);
    int accepted = 0;
    for (int i = 0; i < 5; ++i)
        if (three.Writer->TryWrite(i)) ++accepted;
    EXPECT_EQ(accepted, 3);
    EXPECT_EQ(three.Reader->getCountProperty(), 3);

    auto unbounded = Channel<int>::CreateUnbounded();
    accepted = 0;
    for (int i = 0; i < 100; ++i)
        if (unbounded.Writer->TryWrite(i)) ++accepted;
    EXPECT_EQ(accepted, 100);

    BoundedChannelOptions dropOldest(1);
    dropOldest.FullMode = BoundedChannelFullMode::DropOldest;
    auto dropping = Channel<int>::CreateBounded(dropOldest);
    dropping.Writer->TryWrite(1);
    dropping.Writer->TryWrite(2);
    int survivor = 0;
    EXPECT_TRUE(dropping.Reader->TryRead(survivor));
    EXPECT_EQ(survivor, 2);
}

// The prioritized channel shape is unbounded by construction and has no capacity at all, so it
// is unaffected -- asserted so a future change to the shared reader base cannot silently
// regress it.
TEST(ZeroCapacityRendezvousTests, PrioritizedChannel_IsUnaffected) {
    auto channel = Channel<int>::CreateUnboundedPrioritized();
    EXPECT_TRUE(channel.Writer->TryWrite(5));
    EXPECT_TRUE(channel.Writer->TryWrite(1));
    EXPECT_EQ(channel.Reader->getCountProperty(), 2);
    EXPECT_EQ(channel.Reader->ReadAsync().getResultProperty(), 1);
}

// Layout gate: every type a consumer can name keeps its size and alignment. Only the internal
// detail::ChannelState<T> grew (240 -> 248 bytes on LP64), and it appears in no public
// signature; Threading.Channels is an INTERFACE target, so nothing can be linked across
// versions of it.
TEST(ZeroCapacityRendezvousTests, PublicLayoutIsUnchanged) {
    static_assert(sizeof(Channel<int>) == 32, "Channel<T> layout changed");
    static_assert(sizeof(ChannelReader<int>) == 24, "ChannelReader<T> layout changed");
    static_assert(sizeof(ChannelWriter<int>) == 24, "ChannelWriter<T> layout changed");
    static_assert(sizeof(ChannelOptions) == 16, "ChannelOptions layout changed");
    static_assert(sizeof(BoundedChannelOptions) == 24, "BoundedChannelOptions layout changed");
    static_assert(alignof(Channel<int>) == 8 && alignof(ChannelReader<int>) == 8 &&
                      alignof(ChannelWriter<int>) == 8 && alignof(ChannelOptions) == 8 &&
                      alignof(BoundedChannelOptions) == 8,
                  "channel alignment changed");
    SUCCEED();
}
