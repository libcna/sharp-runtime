// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for Threading::Tasks: Task, TaskT, TaskCompletionSource,
// ValueTask, ValueTaskT, Parallel, ParallelOptions, ParallelLoopResult,
// TaskStatus, TaskCreationOptions, TaskContinuationOptions, ConfigureAwaitOptions,
// TaskCanceledException, TaskSchedulerException, UnobservedTaskExceptionEventArgs,
// TaskScheduler, TaskFactory.
#include <gtest/gtest.h>
#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "System/AggregateException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/OperationCanceledException.hpp"
#include "System/Threading/CancellationToken.hpp"
#include "System/Threading/CancellationTokenSource.hpp"
#include "System/Threading/Tasks/ConfigureAwaitOptions.hpp"
#include "System/Threading/Tasks/Parallel.hpp"
#include "System/Threading/Tasks/Task.hpp"
#include "System/Threading/Tasks/TaskCanceledException.hpp"
#include "System/Threading/Tasks/TaskCompletionSource.hpp"
#include "System/Threading/Tasks/TaskContinuationOptions.hpp"
#include "System/Threading/Tasks/TaskCreationOptions.hpp"
#include "System/Threading/Tasks/TaskFactory.hpp"
#include "System/Threading/Tasks/TaskSchedulerException.hpp"
#include "System/Threading/Tasks/TaskStatus.hpp"
#include "System/Threading/Tasks/UnobservedTaskExceptionEventArgs.hpp"
#include "System/Threading/Tasks/ValueTask.hpp"

using System::Threading::Tasks::Task;
using System::Threading::Tasks::TaskT;
using System::Threading::Tasks::TaskCompletionSource;
using System::Threading::Tasks::ValueTask;
using System::Threading::Tasks::ValueTaskT;
using System::Threading::Tasks::Parallel;
using System::Threading::Tasks::ParallelOptions;
using System::Threading::Tasks::ParallelLoopResult;
using System::Threading::CancellationToken;
using System::Threading::CancellationTokenSource;
using System::Threading::Tasks::TaskStatus;
using System::Threading::Tasks::TaskCreationOptions;
using System::Threading::Tasks::TaskContinuationOptions;
using System::Threading::Tasks::ConfigureAwaitOptions;
using System::Threading::Tasks::TaskCanceledException;
using System::Threading::Tasks::TaskSchedulerException;
using System::Threading::Tasks::UnobservedTaskExceptionEventArgs;
using System::Threading::Tasks::TaskScheduler;
using System::Threading::Tasks::TaskFactory;

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

TEST(TaskTests, FromCanceled_Wait_ThrowsTaskCanceledException) {
    Task t = Task::FromCanceled(CancellationToken());
    EXPECT_THROW(t.Wait(), System::Threading::Tasks::TaskCanceledException);
}

TEST(TaskTests, RunWithCanceledToken_Wait_ThrowsTaskCanceledException) {
    CancellationTokenSource cts;
    cts.Cancel();
    Task t = Task::Run([]() {}, cts.getTokenProperty());
    EXPECT_THROW(t.Wait(), System::Threading::Tasks::TaskCanceledException);
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
    EXPECT_THROW(tcs.SetResult(2), System::InvalidOperationException);
}

TEST(TaskCompletionSourceTests, ConcurrentTrySetResult_ExactlyOneWinner_NoUncaughtException) {
    for (int iter = 0; iter < 200; ++iter) {
        TaskCompletionSource<int> tcs;
        std::atomic<int> successCount{0};
        std::thread t1([&] { if (tcs.TrySetResult(1)) ++successCount; });
        std::thread t2([&] { if (tcs.TrySetResult(2)) ++successCount; });
        t1.join();
        t2.join();
        EXPECT_EQ(successCount.load(), 1);
    }
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
    EXPECT_THROW(tcs.GetResult(), System::Threading::Tasks::TaskCanceledException);
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
    EXPECT_THROW(tcs.Wait(), System::Threading::Tasks::TaskCanceledException);
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

// ===========================================================================
// TaskStatus
// ===========================================================================

TEST(TaskStatusTests, CompletedTask_ReportsRanToCompletion) {
    Task t = Task::CompletedTask();
    EXPECT_EQ(t.getStatusProperty(), TaskStatus::RanToCompletion);
}

TEST(TaskStatusTests, FromException_ReportsFaulted) {
    Task t = Task::FromException(std::make_exception_ptr(std::runtime_error("boom")));
    EXPECT_EQ(t.getStatusProperty(), TaskStatus::Faulted);
}

TEST(TaskStatusTests, FromCanceled_ReportsCanceled) {
    Task t = Task::FromCanceled(CancellationToken::None());
    EXPECT_EQ(t.getStatusProperty(), TaskStatus::Canceled);
}

// ===========================================================================
// TaskCreationOptions / TaskContinuationOptions / ConfigureAwaitOptions
// ===========================================================================

TEST(TaskCreationOptionsTests, BitwiseOr_CombinesFlags) {
    auto combined = TaskCreationOptions::LongRunning | TaskCreationOptions::AttachedToParent;
    EXPECT_EQ(static_cast<int>(combined), 0x02 | 0x04);
}

TEST(TaskCreationOptionsTests, BitwiseAnd_ExtractsFlag) {
    auto combined = TaskCreationOptions::LongRunning | TaskCreationOptions::AttachedToParent;
    EXPECT_EQ(combined & TaskCreationOptions::LongRunning, TaskCreationOptions::LongRunning);
}

TEST(TaskContinuationOptionsTests, OnlyOnRanToCompletion_MatchesDotNetValue) {
    EXPECT_EQ(static_cast<int>(TaskContinuationOptions::OnlyOnRanToCompletion), 0x60000);
}

TEST(TaskContinuationOptionsTests, OnlyOnFaulted_MatchesDotNetValue) {
    EXPECT_EQ(static_cast<int>(TaskContinuationOptions::OnlyOnFaulted), 0x50000);
}

TEST(TaskContinuationOptionsTests, OnlyOnCanceled_MatchesDotNetValue) {
    EXPECT_EQ(static_cast<int>(TaskContinuationOptions::OnlyOnCanceled), 0x30000);
}

TEST(ConfigureAwaitOptionsTests, BitwiseOr_CombinesFlags) {
    auto combined = ConfigureAwaitOptions::ContinueOnCapturedContext | ConfigureAwaitOptions::ForceYielding;
    EXPECT_EQ(static_cast<int>(combined), 0x1 | 0x4);
}

// ===========================================================================
// Task cooperative cancellation
// ===========================================================================

TEST(TaskCancellationTests, PreCanceledToken_TaskIsImmediatelyCanceled) {
    CancellationTokenSource cts;
    cts.Cancel();
    std::atomic<bool> ran{false};
    Task t([&ran]() { ran = true; }, cts.getTokenProperty());
    EXPECT_TRUE(t.getIsCanceledProperty());
    EXPECT_FALSE(ran.load());
}

TEST(TaskCancellationTests, ActionThrowsOperationCanceled_MatchingToken_ReportsCanceled) {
    CancellationTokenSource cts;
    CancellationToken token = cts.getTokenProperty();
    Task t([&cts, token]() {
        cts.Cancel();
        token.ThrowIfCancellationRequested();
    }, token);
    EXPECT_THROW(t.Wait(), System::Threading::Tasks::TaskCanceledException);
    EXPECT_TRUE(t.getIsCanceledProperty());
    EXPECT_FALSE(t.getIsFaultedProperty());
}

TEST(TaskCancellationTests, ActionThrowsOtherException_ReportsFaultedNotCanceled) {
    CancellationTokenSource cts;
    Task t([]() { throw std::runtime_error("boom"); }, cts.getTokenProperty());
    EXPECT_THROW(t.Wait(), std::runtime_error);
    EXPECT_TRUE(t.getIsFaultedProperty());
    EXPECT_FALSE(t.getIsCanceledProperty());
}

TEST(TaskCancellationTests, RunWithToken_GetCancellationTokenProperty_ReturnsSameToken) {
    CancellationTokenSource cts;
    Task t = Task::Run([]() {}, cts.getTokenProperty());
    t.Wait();
    EXPECT_FALSE(t.getCancellationTokenProperty().getIsCancellationRequestedProperty());
}

// ===========================================================================
// TaskCanceledException
// ===========================================================================

TEST(TaskCanceledExceptionTests, DefaultCtor_HasDefaultMessage) {
    TaskCanceledException ex;
    EXPECT_STREQ(ex.what(), "A task was canceled.");
}

TEST(TaskCanceledExceptionTests, MessageCtor_UsesGivenMessage) {
    TaskCanceledException ex("custom message");
    EXPECT_STREQ(ex.what(), "custom message");
}

TEST(TaskCanceledExceptionTests, TaskCtor_StoresTaskPointer) {
    Task t = Task::CompletedTask();
    TaskCanceledException ex(&t);
    EXPECT_EQ(ex.getTaskProperty(), &t);
}

TEST(TaskCanceledExceptionTests, NullTaskCtor_TaskPropertyIsNull) {
    TaskCanceledException ex(static_cast<const Task*>(nullptr));
    EXPECT_EQ(ex.getTaskProperty(), nullptr);
}

TEST(TaskCanceledExceptionTests, IsAnOperationCanceledException) {
    TaskCanceledException ex;
    const System::OperationCanceledException& base = ex;
    EXPECT_STREQ(base.what(), "A task was canceled.");
}

// ===========================================================================
// TaskSchedulerException
// ===========================================================================

TEST(TaskSchedulerExceptionTests, DefaultCtor_HasDefaultMessage) {
    TaskSchedulerException ex;
    EXPECT_STREQ(ex.what(), "An exception was thrown by a TaskScheduler.");
}

TEST(TaskSchedulerExceptionTests, MessageCtor_UsesGivenMessage) {
    TaskSchedulerException ex("scheduler broke");
    EXPECT_STREQ(ex.what(), "scheduler broke");
}

TEST(TaskSchedulerExceptionTests, InnerExceptionCtor_KeepsDefaultMessage) {
    TaskSchedulerException ex(std::make_exception_ptr(std::runtime_error("inner")));
    EXPECT_STREQ(ex.what(), "An exception was thrown by a TaskScheduler.");
}

// ===========================================================================
// UnobservedTaskExceptionEventArgs
// ===========================================================================

TEST(UnobservedTaskExceptionEventArgsTests, DefaultsToNotObserved) {
    System::AggregateException agg("boom");
    UnobservedTaskExceptionEventArgs args(agg);
    EXPECT_FALSE(args.getObservedProperty());
}

TEST(UnobservedTaskExceptionEventArgsTests, SetObserved_MarksObserved) {
    System::AggregateException agg("boom");
    UnobservedTaskExceptionEventArgs args(agg);
    args.SetObserved();
    EXPECT_TRUE(args.getObservedProperty());
}

TEST(UnobservedTaskExceptionEventArgsTests, ExceptionProperty_ReturnsStoredException) {
    System::AggregateException agg("boom");
    UnobservedTaskExceptionEventArgs args(agg);
    EXPECT_STREQ(args.getExceptionProperty().what(), "boom");
}

// ===========================================================================
// TaskScheduler
// ===========================================================================

TEST(TaskSchedulerTests, Default_ReturnsSameInstanceEachCall) {
    EXPECT_EQ(&TaskScheduler::Default(), &TaskScheduler::Default());
}

TEST(TaskSchedulerTests, Current_ReturnsDefault) {
    EXPECT_EQ(&TaskScheduler::Current(), &TaskScheduler::Default());
}

TEST(TaskSchedulerTests, MaximumConcurrencyLevel_IsPositive) {
    EXPECT_GT(TaskScheduler::Default().getMaximumConcurrencyLevelProperty(), 0);
}

// ===========================================================================
// TaskFactory
// ===========================================================================

TEST(TaskFactoryTests, DefaultCtor_HasNoneCreationOptions) {
    TaskFactory factory;
    EXPECT_EQ(factory.getCreationOptionsProperty(), TaskCreationOptions::None);
    EXPECT_EQ(factory.getContinuationOptionsProperty(), TaskContinuationOptions::None);
}

TEST(TaskFactoryTests, StartNew_ExecutesAction) {
    TaskFactory factory;
    std::atomic<bool> ran{false};
    Task t = factory.StartNew([&ran]() { ran = true; });
    t.Wait();
    EXPECT_TRUE(ran.load());
}

TEST(TaskFactoryTests, StartNewWithResult_ReturnsValue) {
    TaskFactory factory;
    TaskT<int> t = factory.StartNew<int>([]() -> int { return 42; });
    EXPECT_EQ(t.Wait(), 42);
}

TEST(TaskFactoryTests, TaskDotFactory_StartNew_ExecutesAction) {
    std::atomic<bool> ran{false};
    Task t = Task::Factory().StartNew([&ran]() { ran = true; });
    t.Wait();
    EXPECT_TRUE(ran.load());
}

TEST(TaskFactoryTests, CreationOptionsCtor_StoresOptions) {
    TaskFactory factory(TaskCreationOptions::LongRunning, TaskContinuationOptions::None);
    EXPECT_EQ(factory.getCreationOptionsProperty(), TaskCreationOptions::LongRunning);
}

// ===========================================================================
// Parallel: ParallelLoopState-aware overloads
// ===========================================================================

TEST(ParallelWithLoopStateTests, For_StopStopsSchedulingFurtherIterations) {
    std::atomic<int> executed{0};
    auto result = Parallel::For(0, 1000,
        std::function<void(int, System::Threading::Tasks::ParallelLoopState&)>(
            [&executed](int i, System::Threading::Tasks::ParallelLoopState& state) {
                ++executed;
                if (i == 0) state.Stop();
            }));
    EXPECT_FALSE(result.getIsCompletedProperty());
    EXPECT_LT(executed.load(), 1000);
}

TEST(ParallelWithLoopStateTests, For_NoStop_CompletesAndRunsEveryIteration) {
    std::atomic<int> sum{0};
    auto result = Parallel::For(0, 10,
        std::function<void(int, System::Threading::Tasks::ParallelLoopState&)>(
            [&sum](int i, System::Threading::Tasks::ParallelLoopState&) { sum += i; }));
    EXPECT_TRUE(result.getIsCompletedProperty());
    EXPECT_EQ(sum.load(), 45);
}

TEST(ParallelWithLoopStateTests, ForEach_StopStopsSchedulingFurtherIterations) {
    std::vector<int> items(1000);
    for (int i = 0; i < 1000; ++i) items[i] = i;
    std::atomic<int> executed{0};
    auto result = Parallel::ForEach<int>(items,
        std::function<void(int, System::Threading::Tasks::ParallelLoopState&)>(
            [&executed](int i, System::Threading::Tasks::ParallelLoopState& state) {
                ++executed;
                if (i == 0) state.Stop();
            }));
    EXPECT_FALSE(result.getIsCompletedProperty());
    EXPECT_LT(executed.load(), 1000);
}

TEST(ParallelLoopResultTests, LowestBreakIteration_DefaultsToNullopt) {
    ParallelLoopResult result;
    EXPECT_FALSE(result.getLowestBreakIterationProperty().has_value());
}
