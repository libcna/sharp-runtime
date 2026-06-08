// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for Threading::Tasks: Task, TaskT, TaskCompletionSource,
// ValueTask, ValueTaskT, Parallel, ParallelOptions, ParallelLoopResult.
#include <gtest/gtest.h>
#include <atomic>
#include <stdexcept>
#include <string>
#include <vector>
#include "System/Threading/Tasks/Task.hpp"
#include "System/Threading/Tasks/TaskCompletionSource.hpp"
#include "System/Threading/Tasks/ValueTask.hpp"
#include "System/Threading/Tasks/Parallel.hpp"
#include "System/Threading/CancellationToken.hpp"

using System::Threading::Tasks::Task;
using System::Threading::Tasks::TaskT;
using System::Threading::Tasks::TaskCompletionSource;
using System::Threading::Tasks::ValueTask;
using System::Threading::Tasks::ValueTaskT;
using System::Threading::Tasks::Parallel;
using System::Threading::Tasks::ParallelOptions;
using System::Threading::Tasks::ParallelLoopResult;
using System::Threading::CancellationToken;

// ===========================================================================
// Task
// ===========================================================================

TEST(TaskTests, DefaultCtor_IsCompleted) {
    Task t;
    EXPECT_TRUE(t.getIsCompletedProperty());
    EXPECT_FALSE(t.getIsFaultedProperty());
    EXPECT_FALSE(t.getIsCanceledProperty());
}

TEST(TaskTests, CompletedTask_IsCompletedSuccessfully) {
    Task t = Task::CompletedTask();
    EXPECT_TRUE(t.getIsCompletedProperty());
    EXPECT_TRUE(t.getIsCompletedSuccessfullyProperty());
}

TEST(TaskTests, Run_ExecutesAction) {
    std::atomic<bool> ran{false};
    Task t = Task::Run([&ran]() { ran = true; });
    t.Wait();
    EXPECT_TRUE(ran.load());
    EXPECT_TRUE(t.getIsCompletedProperty());
}

TEST(TaskTests, Run_Wait_CompletedSuccessfully) {
    Task t = Task::Run([]() {});
    t.Wait();
    EXPECT_TRUE(t.getIsCompletedSuccessfullyProperty());
    EXPECT_FALSE(t.getIsFaultedProperty());
}

TEST(TaskTests, FromException_IsFaulted) {
    auto ex = std::make_exception_ptr(std::runtime_error("err"));
    Task t = Task::FromException(ex);
    EXPECT_TRUE(t.getIsFaultedProperty());
    EXPECT_TRUE(t.getIsCompletedProperty());
}

TEST(TaskTests, FromException_Wait_Rethrows) {
    auto ex = std::make_exception_ptr(std::runtime_error("rethrown"));
    Task t = Task::FromException(ex);
    EXPECT_THROW(t.Wait(), std::runtime_error);
}

TEST(TaskTests, FromCanceled_IsCanceled) {
    Task t = Task::FromCanceled(CancellationToken());
    EXPECT_TRUE(t.getIsCanceledProperty());
    EXPECT_TRUE(t.getIsCompletedProperty());
}

TEST(TaskTests, Run_ThrowingAction_IsFaulted) {
    Task t = Task::Run([]() { throw std::runtime_error("task error"); });
    EXPECT_THROW(t.Wait(), std::runtime_error);
    EXPECT_TRUE(t.getIsFaultedProperty());
}

TEST(TaskTests, Delay_CompletesAfterWait) {
    Task t = Task::Delay(1);
    EXPECT_NO_THROW(t.Wait());
}

// ===========================================================================
// TaskT<TResult>
// ===========================================================================

TEST(TaskTTests, Run_ReturnsValue) {
    TaskT<int> t = TaskT<int>::Run([]() { return 42; });
    EXPECT_EQ(t.Wait(), 42);
    EXPECT_TRUE(t.getIsCompletedProperty());
}

TEST(TaskTTests, FromResult_ReturnsValueImmediately) {
    TaskT<int> t = TaskT<int>::FromResult(99);
    EXPECT_EQ(t.getResultProperty(), 99);
}

TEST(TaskTTests, Run_ThrowingFunc_IsFaulted) {
    TaskT<int> t = TaskT<int>::Run([]() -> int { throw std::runtime_error("fail"); });
    EXPECT_THROW(t.Wait(), std::runtime_error);
    EXPECT_TRUE(t.getIsFaultedProperty());
}

TEST(TaskTTests, Run_StringResult) {
    TaskT<std::string> t = TaskT<std::string>::Run([]() { return std::string("hello"); });
    EXPECT_EQ(t.Wait(), "hello");
}

// ===========================================================================
// TaskCompletionSource<TResult>
// ===========================================================================

TEST(TaskCompletionSourceTests, SetResult_GetResult) {
    TaskCompletionSource<int> tcs;
    tcs.SetResult(77);
    EXPECT_EQ(tcs.GetResult(), 77);
}

TEST(TaskCompletionSourceTests, SetResult_Twice_Throws) {
    TaskCompletionSource<int> tcs;
    tcs.SetResult(1);
    EXPECT_THROW(tcs.SetResult(2), std::invalid_argument);
}

TEST(TaskCompletionSourceTests, TrySetResult_FirstTime_True) {
    TaskCompletionSource<int> tcs;
    EXPECT_TRUE(tcs.TrySetResult(5));
}

TEST(TaskCompletionSourceTests, TrySetResult_SecondTime_False) {
    TaskCompletionSource<int> tcs;
    tcs.TrySetResult(1);
    EXPECT_FALSE(tcs.TrySetResult(2));
}

TEST(TaskCompletionSourceTests, SetException_GetResult_Throws) {
    TaskCompletionSource<int> tcs;
    tcs.SetException(std::make_exception_ptr(std::runtime_error("tcs error")));
    EXPECT_THROW(tcs.GetResult(), std::runtime_error);
}

TEST(TaskCompletionSourceTests, TrySetException_FirstTime_True) {
    TaskCompletionSource<int> tcs;
    EXPECT_TRUE(tcs.TrySetException(std::make_exception_ptr(std::runtime_error("x"))));
}

TEST(TaskCompletionSourceTests, SetCanceled_GetResult_Throws) {
    TaskCompletionSource<int> tcs;
    tcs.SetCanceled();
    EXPECT_THROW(tcs.GetResult(), std::runtime_error);
}

TEST(TaskCompletionSourceTests, TrySetCanceled_FirstTime_True) {
    TaskCompletionSource<int> tcs;
    EXPECT_TRUE(tcs.TrySetCanceled());
}

TEST(TaskCompletionSourceTests, TrySetCanceled_AfterResult_False) {
    TaskCompletionSource<int> tcs;
    tcs.SetResult(1);
    EXPECT_FALSE(tcs.TrySetCanceled());
}

// ===========================================================================
// TaskCompletionSource<void>
// ===========================================================================

TEST(TaskCompletionSourceVoidTests, SetResult_Wait_NoThrow) {
    TaskCompletionSource<void> tcs;
    tcs.SetResult();
    EXPECT_NO_THROW(tcs.Wait());
}

TEST(TaskCompletionSourceVoidTests, TrySetResult_FirstTime_True) {
    TaskCompletionSource<void> tcs;
    EXPECT_TRUE(tcs.TrySetResult());
}

TEST(TaskCompletionSourceVoidTests, TrySetResult_Twice_False) {
    TaskCompletionSource<void> tcs;
    tcs.TrySetResult();
    EXPECT_FALSE(tcs.TrySetResult());
}

TEST(TaskCompletionSourceVoidTests, SetException_Wait_Throws) {
    TaskCompletionSource<void> tcs;
    tcs.SetException(std::make_exception_ptr(std::runtime_error("void err")));
    EXPECT_THROW(tcs.Wait(), std::runtime_error);
}

TEST(TaskCompletionSourceVoidTests, SetCanceled_Wait_Throws) {
    TaskCompletionSource<void> tcs;
    tcs.SetCanceled();
    EXPECT_THROW(tcs.Wait(), std::runtime_error);
}

// ===========================================================================
// ValueTask
// ===========================================================================

TEST(ValueTaskTests, DefaultCtor_IsCompleted) {
    ValueTask vt;
    EXPECT_TRUE(vt.getIsCompletedProperty());
    EXPECT_TRUE(vt.getIsCompletedSuccessfullyProperty());
    EXPECT_FALSE(vt.getIsFaultedProperty());
}

TEST(ValueTaskTests, CompletedTask_IsCompleted) {
    ValueTask vt = ValueTask::CompletedTask();
    EXPECT_TRUE(vt.getIsCompletedProperty());
}

TEST(ValueTaskTests, FromException_IsFaulted) {
    ValueTask vt = ValueTask::FromException(std::make_exception_ptr(std::runtime_error("vt")));
    EXPECT_TRUE(vt.getIsFaultedProperty());
    EXPECT_FALSE(vt.getIsCompletedSuccessfullyProperty());
}

TEST(ValueTaskTests, FromException_GetAwaiter_Rethrows) {
    ValueTask vt = ValueTask::FromException(std::make_exception_ptr(std::runtime_error("await")));
    EXPECT_THROW(vt.GetAwaiter(), std::runtime_error);
}

TEST(ValueTaskTests, FromTask_CompletedTask_IsCompleted) {
    ValueTask vt(Task::CompletedTask());
    EXPECT_TRUE(vt.getIsCompletedProperty());
}

// ===========================================================================
// ValueTaskT<TResult>
// ===========================================================================

TEST(ValueTaskTTests, FromResult_IsCompletedSuccessfully) {
    ValueTaskT<int> vt = ValueTaskT<int>::FromResult(42);
    EXPECT_TRUE(vt.getIsCompletedProperty());
    EXPECT_TRUE(vt.getIsCompletedSuccessfullyProperty());
    EXPECT_EQ(vt.getResultProperty(), 42);
}

TEST(ValueTaskTTests, FromException_IsFaulted) {
    ValueTaskT<int> vt = ValueTaskT<int>::FromException(
        std::make_exception_ptr(std::runtime_error("vtT")));
    EXPECT_TRUE(vt.getIsFaultedProperty());
}

TEST(ValueTaskTTests, FromException_GetResult_Throws) {
    ValueTaskT<int> vt = ValueTaskT<int>::FromException(
        std::make_exception_ptr(std::runtime_error("get")));
    EXPECT_THROW(vt.getResultProperty(), std::runtime_error);
}

TEST(ValueTaskTTests, DefaultCtor_NotCompleted) {
    ValueTaskT<int> vt;
    EXPECT_FALSE(vt.getIsCompletedProperty());
}

// ===========================================================================
// Parallel
// ===========================================================================

TEST(ParallelTests, For_ExecutesAllIterations) {
    std::atomic<int> sum{0};
    auto result = Parallel::For(0, 10, [&sum](int i) { sum += i; });
    EXPECT_EQ(sum.load(), 45);
    EXPECT_TRUE(result.getIsCompletedProperty());
}

TEST(ParallelTests, For_EmptyRange_IsCompleted) {
    auto result = Parallel::For(5, 5, [](int) {});
    EXPECT_TRUE(result.getIsCompletedProperty());
}

TEST(ParallelTests, For_WithOptions_ExecutesAll) {
    std::atomic<int> count{0};
    ParallelOptions opts;
    opts.MaxDegreeOfParallelism = 2;
    auto result = Parallel::For(0, 4, opts, [&count](int) { ++count; });
    EXPECT_EQ(count.load(), 4);
    EXPECT_TRUE(result.getIsCompletedProperty());
}

TEST(ParallelTests, ForEach_ExecutesAllItems) {
    std::vector<int> items = {1, 2, 3, 4, 5};
    std::atomic<int> sum{0};
    auto result = Parallel::ForEach<int>(items, [&sum](int v) { sum += v; });
    EXPECT_EQ(sum.load(), 15);
    EXPECT_TRUE(result.getIsCompletedProperty());
}

TEST(ParallelTests, ForEach_EmptyVector_IsCompleted) {
    std::vector<int> empty;
    auto result = Parallel::ForEach<int>(empty, [](int) {});
    EXPECT_TRUE(result.getIsCompletedProperty());
}

TEST(ParallelTests, Invoke_ExecutesAllActions) {
    std::atomic<int> count{0};
    Parallel::Invoke({
        [&count]() { ++count; },
        [&count]() { ++count; },
        [&count]() { ++count; }
    });
    EXPECT_EQ(count.load(), 3);
}

// ===========================================================================
// ParallelLoopState
// ===========================================================================

TEST(ParallelLoopStateTests, DefaultState_NotStopped) {
    System::Threading::Tasks::ParallelLoopState state;
    EXPECT_FALSE(state.getShouldExitCurrentIterationProperty());
    EXPECT_FALSE(state.getIsExceptionalProperty());
}

TEST(ParallelLoopStateTests, Stop_SetsShouldExit) {
    System::Threading::Tasks::ParallelLoopState state;
    state.Stop();
    EXPECT_TRUE(state.getShouldExitCurrentIterationProperty());
}
