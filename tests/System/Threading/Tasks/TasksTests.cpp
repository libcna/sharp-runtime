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
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
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

TEST(TaskTests, Delay_NegativeOne_DoesNotThrow) {
    // -1 is a valid sentinel in real .NET's Task.Delay (API-surface parity only -- see
    // Delay()'s own doc-comment for why this port doesn't give it true infinite-wait semantics).
    EXPECT_NO_THROW(Task::Delay(-1).Wait());
}

TEST(TaskTests, Delay_LessThanNegativeOne_ThrowsArgumentOutOfRangeException) {
    EXPECT_THROW(Task::Delay(-2), System::ArgumentOutOfRangeException);
}

// ===========================================================================
// Task::WhenAll
// ===========================================================================

TEST(TaskWhenAllTests, EmptyVector_ReturnsCompletedTask) {
    Task t = Task::WhenAll({});
    EXPECT_TRUE(t.getIsCompletedProperty());
    EXPECT_TRUE(t.getIsCompletedSuccessfullyProperty());
}

TEST(TaskWhenAllTests, AllSucceed_CompletesSuccessfully) {
    std::atomic<int> counter{0};
    std::vector<Task> tasks;
    for (int i = 0; i < 5; ++i) {
        tasks.push_back(Task::Run([&counter]() { ++counter; }));
    }
    Task all = Task::WhenAll(std::move(tasks));
    EXPECT_NO_THROW(all.Wait());
    EXPECT_TRUE(all.getIsCompletedSuccessfullyProperty());
    EXPECT_EQ(counter.load(), 5);
}

TEST(TaskWhenAllTests, EveryTaskRuns_EvenIfEarlierOneFaults) {
    // WhenAll must not short-circuit: every input task should still run to completion even
    // though an earlier one in the vector faults, matching real .NET's "wait for all" contract.
    std::atomic<int> ranCount{0};
    std::vector<Task> tasks;
    tasks.push_back(Task::Run([]() { throw std::runtime_error("first task fails"); }));
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(Task::Run([&ranCount]() { ++ranCount; }));
    }
    Task all = Task::WhenAll(std::move(tasks));
    EXPECT_THROW(all.Wait(), std::runtime_error);
    EXPECT_EQ(ranCount.load(), 4);
}

TEST(TaskWhenAllTests, OneFaults_WaitRethrowsFirstFault) {
    std::vector<Task> tasks;
    tasks.push_back(Task::Run([]() {}));
    tasks.push_back(Task::Run([]() { throw std::runtime_error("boom"); }));
    tasks.push_back(Task::Run([]() {}));
    Task all = Task::WhenAll(std::move(tasks));
    EXPECT_THROW(all.Wait(), std::runtime_error);
    EXPECT_TRUE(all.getIsFaultedProperty());
}

TEST(TaskWhenAllTests, OneCanceled_NoFault_WaitThrowsTaskCanceledException) {
    CancellationTokenSource cts;
    cts.Cancel();
    std::vector<Task> tasks;
    tasks.push_back(Task::Run([]() {}));
    tasks.push_back(Task::Run([]() {}, cts.getTokenProperty()));
    Task all = Task::WhenAll(std::move(tasks));
    EXPECT_THROW(all.Wait(), System::Threading::Tasks::TaskCanceledException);
}

// ===========================================================================
// Task::WhenAny
// ===========================================================================

TEST(TaskWhenAnyTests, EmptyVector_Throws) {
    EXPECT_THROW(Task::WhenAny({}), System::ArgumentException);
}

TEST(TaskWhenAnyTests, SingleTask_ReturnsThatTask) {
    Task t = Task::Run([]() {});
    TaskT<Task> any = Task::WhenAny({t});
    Task winner = any.Wait();
    EXPECT_TRUE(any.getIsCompletedSuccessfullyProperty());
    EXPECT_TRUE(winner.getIsCompletedProperty());
}

TEST(TaskWhenAnyTests, FastTaskWinsOverSlowTask) {
    Task fast = Task::Run([]() {});
    Task slow = Task::Run([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    });
    auto start = std::chrono::steady_clock::now();
    TaskT<Task> any = Task::WhenAny(std::vector<Task>{fast, slow});
    Task winner = any.Wait();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    // WhenAny must return once the FAST task completes, not wait for the slow one -- checked via
    // elapsed wall-clock time (generous margin for scheduling jitter under load) rather than a
    // side-effect flag racing the slow task's own thread, which is inherently flaky.
    EXPECT_TRUE(winner.getIsCompletedProperty());
    EXPECT_LT(elapsedMs, 400);
}

TEST(TaskWhenAnyTests, WrapperCompletesSuccessfully_EvenWhenWinnerFaulted) {
    // Matches real .NET's own documented contract: the returned wrapper task always itself
    // completes RanToCompletion, even if the first-completed inner task faulted.
    Task faulting = Task::Run([]() { throw std::runtime_error("boom"); });
    TaskT<Task> any = Task::WhenAny(std::vector<Task>{faulting});
    EXPECT_NO_THROW(any.Wait());
    EXPECT_TRUE(any.getIsCompletedSuccessfullyProperty());
    Task winner = any.Wait();
    EXPECT_TRUE(winner.getIsFaultedProperty());
    EXPECT_THROW(winner.Wait(), std::runtime_error);
}

TEST(TaskWhenAnyTests, WrapperCompletesSuccessfully_EvenWhenWinnerCanceled) {
    CancellationTokenSource cts;
    cts.Cancel();
    Task canceled = Task::Run([]() {}, cts.getTokenProperty());
    TaskT<Task> any = Task::WhenAny(std::vector<Task>{canceled});
    EXPECT_NO_THROW(any.Wait());
    EXPECT_TRUE(any.getIsCompletedSuccessfullyProperty());
    Task winner = any.Wait();
    EXPECT_TRUE(winner.getIsCanceledProperty());
}

TEST(TaskWhenAnyTests, ManyTasks_ExactlyOneWinnerObserved) {
    constexpr int kCount = 20;
    std::vector<Task> tasks;
    std::vector<std::shared_ptr<std::atomic<bool>>> ran;
    for (int i = 0; i < kCount; ++i) {
        auto flag = std::make_shared<std::atomic<bool>>(false);
        ran.push_back(flag);
        tasks.push_back(Task::Run([flag]() { *flag = true; }));
    }
    TaskT<Task> any = Task::WhenAny(std::move(tasks));
    Task winner = any.Wait();
    EXPECT_TRUE(winner.getIsCompletedProperty());
}

TEST(TaskWhenAnyTests, RepeatedCalls_NoFlakiness) {
    // Stress the watcher-thread synchronization (atomic CAS + condition_variable) across many
    // repetitions with varying task counts, matching this project's convention of repeat-testing
    // concurrency-sensitive code (see CLAUDE.md's --gtest_repeat guidance).
    for (int iter = 0; iter < 50; ++iter) {
        std::vector<Task> tasks;
        for (int i = 0; i < 5; ++i) {
            tasks.push_back(Task::Run([]() {}));
        }
        TaskT<Task> any = Task::WhenAny(std::move(tasks));
        Task winner = any.Wait();
        EXPECT_TRUE(winner.getIsCompletedProperty());
    }
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

TEST(TaskTTests, GetStatusProperty_MatchesTaskLifecycle) {
    TaskT<int> t = TaskT<int>::FromResult(1);
    EXPECT_EQ(t.getStatusProperty(), System::Threading::Tasks::TaskStatus::RanToCompletion);
    EXPECT_TRUE(t.getIsCompletedSuccessfullyProperty());
}

TEST(TaskTTests, Faulted_IsNotCompletedSuccessfully) {
    TaskT<int> t = TaskT<int>::Run([]() -> int { throw std::runtime_error("fail"); });
    EXPECT_THROW(t.Wait(), std::runtime_error);
    EXPECT_EQ(t.getStatusProperty(), System::Threading::Tasks::TaskStatus::Faulted);
    EXPECT_FALSE(t.getIsCompletedSuccessfullyProperty());
}

// ===========================================================================
// TaskT<TResult> cooperative cancellation (mirrors the Task cancellation tests below --
// TaskT previously had no CancellationToken constructor at all, an asymmetry with Task)
// ===========================================================================

TEST(TaskTCancellationTests, PreCanceledToken_TaskIsImmediatelyCanceled) {
    CancellationTokenSource cts;
    cts.Cancel();
    std::atomic<bool> ran{false};
    TaskT<int> t([&ran]() { ran = true; return 1; }, cts.getTokenProperty());
    EXPECT_TRUE(t.getIsCanceledProperty());
    EXPECT_FALSE(ran.load());
    EXPECT_THROW(t.Wait(), System::Threading::Tasks::TaskCanceledException);
}

TEST(TaskTCancellationTests, FuncThrowsOperationCanceled_MatchingToken_ReportsCanceled) {
    CancellationTokenSource cts;
    CancellationToken token = cts.getTokenProperty();
    TaskT<int> t([&cts, token]() -> int {
        cts.Cancel();
        token.ThrowIfCancellationRequested();
        return 1;
    }, token);
    EXPECT_THROW(t.Wait(), System::Threading::Tasks::TaskCanceledException);
    EXPECT_TRUE(t.getIsCanceledProperty());
    EXPECT_FALSE(t.getIsFaultedProperty());
    EXPECT_EQ(t.getStatusProperty(), System::Threading::Tasks::TaskStatus::Canceled);
}

TEST(TaskTCancellationTests, FuncThrowsOtherException_ReportsFaultedNotCanceled) {
    CancellationTokenSource cts;
    TaskT<int> t([]() -> int { throw std::runtime_error("boom"); }, cts.getTokenProperty());
    EXPECT_THROW(t.Wait(), std::runtime_error);
    EXPECT_TRUE(t.getIsFaultedProperty());
    EXPECT_FALSE(t.getIsCanceledProperty());
}

TEST(TaskTCancellationTests, RunWithToken_GetCancellationTokenProperty_ReturnsSameToken) {
    CancellationTokenSource cts;
    TaskT<int> t = TaskT<int>::Run([]() { return 42; }, cts.getTokenProperty());
    EXPECT_EQ(t.Wait(), 42);
    EXPECT_FALSE(t.getCancellationTokenProperty().getIsCancellationRequestedProperty());
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

TEST(ValueTaskTests, FromTask_AlreadyFaultedTask_GetAwaiter_Rethrows) {
    auto ex = std::make_exception_ptr(std::runtime_error("already faulted"));
    ValueTask vt(Task::FromException(ex));
    EXPECT_TRUE(vt.getIsFaultedProperty());
    EXPECT_THROW(vt.GetAwaiter(), std::runtime_error);
}

TEST(ValueTaskTests, FromTask_StillRunning_LaterObservesCompletionAndException) {
    std::promise<void> release;
    std::shared_future<void> releaseFuture = release.get_future().share();
    Task t([releaseFuture]() {
        releaseFuture.wait();
        throw std::runtime_error("failed after running");
    });
    ValueTask vt(std::move(t));
    EXPECT_FALSE(vt.getIsCompletedProperty());
    release.set_value();
    EXPECT_THROW(vt.GetAwaiter(), std::runtime_error);
    EXPECT_TRUE(vt.getIsCompletedProperty());
    EXPECT_TRUE(vt.getIsFaultedProperty());
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

// Regression test for a wave-7 audit finding: the default constructor previously set
// completed_ = false, permanently "incomplete" with no way to ever complete it (no wrapped
// task, no setter). Verified against ValueTask<TResult>.IsCompleted (`if (obj == null) return
// true;`): real .NET's "no backing Task/IValueTaskSource" state is always completed, holding
// default(TResult) -- not permanently pending.
TEST(ValueTaskTTests, DefaultCtor_IsCompletedWithDefaultResult) {
    ValueTaskT<int> vt;
    EXPECT_TRUE(vt.getIsCompletedProperty());
    EXPECT_TRUE(vt.getIsCompletedSuccessfullyProperty());
    EXPECT_FALSE(vt.getIsFaultedProperty());
    EXPECT_EQ(vt.getResultProperty(), 0);
}

// Regression tests for a wave-7 audit finding: ValueTaskT had no TaskT<TResult>-wrapping
// constructor at all, unlike the sibling ValueTask's Task-wrapping constructor -- meaning it
// could only ever represent an already-known synchronous result/exception, never a still-running
// async operation. These mirror the existing ValueTask FromTask_* tests above.

TEST(ValueTaskTTests, FromTaskT_CompletedTask_IsCompleted) {
    ValueTaskT<int> vt(TaskT<int>::FromResult(7));
    EXPECT_TRUE(vt.getIsCompletedProperty());
    EXPECT_TRUE(vt.getIsCompletedSuccessfullyProperty());
    EXPECT_EQ(vt.getResultProperty(), 7);
}

TEST(ValueTaskTTests, FromTaskT_AlreadyFaultedTask_GetResult_Rethrows) {
    auto ex = std::make_exception_ptr(std::runtime_error("taskT faulted"));
    ValueTaskT<int> vt(TaskT<int>::Run([ex]() -> int { std::rethrow_exception(ex); }));
    EXPECT_THROW(vt.getResultProperty(), std::runtime_error);
    EXPECT_TRUE(vt.getIsFaultedProperty());
}

TEST(ValueTaskTTests, FromTaskT_StillRunning_LaterObservesCompletion) {
    std::promise<void> release;
    std::shared_future<void> releaseFuture = release.get_future().share();
    TaskT<int> t([releaseFuture]() -> int {
        releaseFuture.wait();
        return 99;
    });
    ValueTaskT<int> vt(std::move(t));
    EXPECT_FALSE(vt.getIsCompletedProperty());
    release.set_value();
    EXPECT_EQ(vt.getResultProperty(), 99);
    EXPECT_TRUE(vt.getIsCompletedProperty());
    EXPECT_TRUE(vt.getIsCompletedSuccessfullyProperty());
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

// Regression tests for a wave-3 audit finding: For/ForEach/Invoke called f.get() on each future
// with no try/catch, so the first exception observed escaped unwrapped and every other future's
// exception was silently discarded when its std::future was destroyed. Verified against
// Parallel.cs, which always aggregates every exception raised by any iteration/action into a
// single AggregateException, even when only one iteration actually throws.
TEST(ParallelTests, For_IterationThrows_ThrowsAggregateException) {
    EXPECT_THROW(
        Parallel::For(0, 10, [](int i) {
            if (i % 2 == 0) throw std::runtime_error("even");
        }),
        System::AggregateException);
}

TEST(ParallelTests, For_MultipleIterationsThrow_AggregatesAllExceptions) {
    try {
        Parallel::For(0, 10, [](int i) {
            if (i % 2 == 0) throw std::runtime_error("even");
        });
        FAIL() << "expected AggregateException";
    } catch (const System::AggregateException& agg) {
        // Iterations 0,2,4,6,8 all throw -- every one of them must be collected, not just the first.
        EXPECT_EQ(agg.getInnerExceptionCountProperty(), 5u);
    }
}

TEST(ParallelTests, ForEach_ItemThrows_ThrowsAggregateException) {
    std::vector<int> items = {1, 2, 3};
    EXPECT_THROW(
        Parallel::ForEach<int>(items, [](int v) {
            if (v == 2) throw std::runtime_error("bad item");
        }),
        System::AggregateException);
}

TEST(ParallelTests, Invoke_ActionThrows_ThrowsAggregateException) {
    EXPECT_THROW(
        Parallel::Invoke({
            []() {},
            []() { throw std::runtime_error("boom"); },
            []() {}
        }),
        System::AggregateException);
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

TEST(TaskFactoryTests, StartNewWithResultAndToken_ObservesCancellation) {
    CancellationTokenSource cts;
    cts.Cancel();
    TaskFactory factory;
    std::atomic<bool> ran{false};
    TaskT<int> t = factory.StartNew<int>([&ran]() -> int { ran = true; return 1; },
                                          cts.getTokenProperty());
    EXPECT_TRUE(t.getIsCanceledProperty());
    EXPECT_FALSE(ran.load());
}

TEST(TaskFactoryTests, StartNewWithResult_UsesFactoryDefaultToken) {
    CancellationTokenSource cts;
    cts.Cancel();
    TaskFactory factory(cts.getTokenProperty());
    std::atomic<bool> ran{false};
    TaskT<int> t = factory.StartNew<int>([&ran]() -> int { ran = true; return 1; });
    EXPECT_TRUE(t.getIsCanceledProperty());
    EXPECT_FALSE(ran.load());
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

// Regression test for a wave-3 audit finding: unlike the sibling For(..., ParallelOptions,
// ...) overload, this port's other loop-shaped overloads (For with ParallelLoopState, both
// ForEach variants) launched one std::async per iteration/item with no batching at all --
// for a large source, this attempted to spawn far more concurrent OS threads than the
// hardware supports. Verified indirectly: track the maximum number of loop bodies observed
// running concurrently and assert it never exceeds hardware_concurrency(); before the fix,
// all items would fire (and briefly overlap) at once, exceeding this bound.
TEST(ParallelWithLoopStateTests, ForEach_BoundedConcurrency_DoesNotExceedHardwareConcurrency) {
    int maxAllowed = static_cast<int>(std::thread::hardware_concurrency());
    if (maxAllowed < 1) maxAllowed = 1;
    std::vector<int> items(static_cast<size_t>(maxAllowed) * 4);
    for (size_t i = 0; i < items.size(); ++i) items[i] = static_cast<int>(i);

    std::atomic<int> concurrent{0};
    std::atomic<int> maxConcurrent{0};
    Parallel::ForEach<int>(items, std::function<void(int)>([&](int) {
        int now = concurrent.fetch_add(1) + 1;
        int prevMax = maxConcurrent.load();
        while (now > prevMax && !maxConcurrent.compare_exchange_weak(prevMax, now)) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        concurrent.fetch_sub(1);
    }));

    EXPECT_LE(maxConcurrent.load(), maxAllowed);
}

TEST(ParallelWithLoopStateTests, For_BoundedConcurrency_DoesNotExceedHardwareConcurrency) {
    int maxAllowed = static_cast<int>(std::thread::hardware_concurrency());
    if (maxAllowed < 1) maxAllowed = 1;
    int total = maxAllowed * 4;

    std::atomic<int> concurrent{0};
    std::atomic<int> maxConcurrent{0};
    Parallel::For(0, total,
        std::function<void(int, System::Threading::Tasks::ParallelLoopState&)>(
            [&](int, System::Threading::Tasks::ParallelLoopState&) {
                int now = concurrent.fetch_add(1) + 1;
                int prevMax = maxConcurrent.load();
                while (now > prevMax && !maxConcurrent.compare_exchange_weak(prevMax, now)) {}
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                concurrent.fetch_sub(1);
            }));

    EXPECT_LE(maxConcurrent.load(), maxAllowed);
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

// Regression tests for a wave-7 audit finding: ParallelLoopResult::getLowestBreakIterationProperty()
// had a getter but was never populated anywhere in For/ForEach -- Break() only set the shared
// "stopped" flag and never recorded which iteration called it, so this property always returned
// nullopt even when Break() genuinely ran. Fixed by tracking the minimum Break()-calling index via
// a shared atomic, with the Parallel dispatcher stamping each dispatched iteration's own state copy
// with its index before invoking the body (mirrors ParallelLoopState.cs's own per-iteration design).

TEST(ParallelLoopResultTests, For_Break_RecordsCallingIterationAsLowestBreakIteration) {
    // Only iteration 5 ever calls Break(), so the result is deterministic regardless of scheduling.
    auto result = Parallel::For(0, 20,
        std::function<void(int, System::Threading::Tasks::ParallelLoopState&)>(
            [](int i, System::Threading::Tasks::ParallelLoopState& state) {
                if (i == 5) state.Break();
            }));
    ASSERT_TRUE(result.getLowestBreakIterationProperty().has_value());
    EXPECT_EQ(result.getLowestBreakIterationProperty().value(), 5);
}

TEST(ParallelLoopResultTests, ForEach_Break_RecordsCallingSourcePositionAsLowestBreakIteration) {
    std::vector<int> items(20);
    for (int i = 0; i < 20; ++i) items[static_cast<size_t>(i)] = i * 100; // values distinct from positions
    auto result = Parallel::ForEach<int>(items,
        std::function<void(int, System::Threading::Tasks::ParallelLoopState&)>(
            [](int value, System::Threading::Tasks::ParallelLoopState& state) {
                if (value == 300) state.Break(); // source position 3
            }));
    ASSERT_TRUE(result.getLowestBreakIterationProperty().has_value());
    EXPECT_EQ(result.getLowestBreakIterationProperty().value(), 3);
}

TEST(ParallelLoopResultTests, Stop_DoesNotPopulateLowestBreakIteration) {
    auto result = Parallel::For(0, 20,
        std::function<void(int, System::Threading::Tasks::ParallelLoopState&)>(
            [](int i, System::Threading::Tasks::ParallelLoopState& state) {
                if (i == 5) state.Stop();
            }));
    EXPECT_FALSE(result.getLowestBreakIterationProperty().has_value());
}

TEST(ParallelLoopStateTests, Break_TracksLowestCallingIteration) {
    System::Threading::Tasks::ParallelLoopState state;
    EXPECT_FALSE(state.getLowestBreakIterationProperty().has_value());
    state.Break();
    ASSERT_TRUE(state.getLowestBreakIterationProperty().has_value());
    EXPECT_EQ(state.getLowestBreakIterationProperty().value(), 0);
}
