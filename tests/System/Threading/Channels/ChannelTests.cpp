// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <thread>
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

TEST(ChannelTests, ZeroCapacityChannel_TryWrite_SucceedsOnceThenBlocksLikeCapacityOne) {
    auto channel = Channel<int>::CreateBounded(0);
    EXPECT_TRUE(channel.Writer->TryWrite(1));
    EXPECT_FALSE(channel.Writer->TryWrite(2)); // full, Wait mode rejects synchronous TryWrite
    int value = 0;
    EXPECT_TRUE(channel.Reader->TryRead(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(channel.Writer->TryWrite(3)); // slot freed, write succeeds again
}

TEST(ChannelTests, ZeroCapacityChannel_WriteAsync_UnblocksOnceReaderDrains) {
    auto channel = Channel<int>::CreateBounded(0);
    channel.Writer->TryWrite(1);
    auto writeTask = channel.Writer->WriteAsync(2);

    int value = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    channel.Reader->TryRead(value); // frees the slot, allowing the pending WriteAsync to complete
    writeTask.Wait();
    EXPECT_EQ(value, 1);

    int second = 0;
    EXPECT_TRUE(channel.Reader->TryRead(second));
    EXPECT_EQ(second, 2);
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
