// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for newly-added/newly-fixed Threading types: LockCookie, ReaderWriterLock,
// ReaderWriterLockSlim held-state tracking, ExecutionContext, IThreadPoolWorkItem,
// WaitCallback/WaitOrTimerCallback, RegisteredWaitHandle/ThreadPool::RegisterWaitForSingleObject,
// WaitHandle::WaitAll/WaitAny, Barrier post-phase exception wrapping, and Mutex's
// initiallyOwned/timeout fixes.
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Threading/AbandonedMutexException.hpp"
#include "System/Threading/Barrier.hpp"
#include "System/Threading/BarrierPostPhaseException.hpp"
#include "System/Threading/CancellationTokenRegistration.hpp"
#include "System/Threading/CancellationTokenSource.hpp"
#include "System/Threading/ExecutionContext.hpp"
#include "System/Threading/IThreadPoolWorkItem.hpp"
#include "System/Threading/LockCookie.hpp"
#include "System/Threading/Mutex.hpp"
#include "System/Threading/ReaderWriterLock.hpp"
#include "System/Threading/ReaderWriterLockSlim.hpp"
#include "System/Threading/RegisteredWaitHandle.hpp"
#include "System/Threading/Semaphore.hpp"
#include "System/Threading/ThreadPool.hpp"
#include "System/Threading/WaitCallback.hpp"
#include "System/Threading/WaitHandle.hpp"
#include "System/Threading/WaitOrTimerCallback.hpp"

using namespace System::Threading;

// ===========================================================================
// AbandonedMutexException
// ===========================================================================

TEST(AbandonedMutexExceptionTests, LocationAndHandleCtor_SetsMutexAndIndex) {
    Mutex m;
    AbandonedMutexException ex(3, &m);
    EXPECT_EQ(ex.getMutexIndexProperty(), 3);
    EXPECT_EQ(ex.getMutexProperty(), &m);
}

TEST(AbandonedMutexExceptionTests, DefaultCtor_HasNoMutex) {
    AbandonedMutexException ex;
    EXPECT_EQ(ex.getMutexIndexProperty(), -1);
    EXPECT_EQ(ex.getMutexProperty(), nullptr);
}

// ===========================================================================
// CancellationToken::Register / CancellationTokenSource callback invocation
// ===========================================================================

TEST(CancellationTokenRegisterTests, Register_InvokedOnCancel) {
    CancellationTokenSource cts;
    CancellationToken token = cts.getTokenProperty();
    bool called = false;
    auto reg = token.Register([&] { called = true; });
    EXPECT_FALSE(called);
    cts.Cancel();
    EXPECT_TRUE(called);
    reg.Dispose();
}

TEST(CancellationTokenRegisterTests, Register_AfterAlreadyCancelled_InvokesImmediately) {
    CancellationTokenSource cts;
    cts.Cancel();
    bool called = false;
    auto reg = cts.getTokenProperty().Register([&] { called = true; });
    EXPECT_TRUE(called);
    reg.Dispose();
}

TEST(CancellationTokenRegisterTests, DisposedRegistration_NotInvokedOnCancel) {
    CancellationTokenSource cts;
    CancellationToken token = cts.getTokenProperty();
    bool called = false;
    auto reg = token.Register([&] { called = true; });
    reg.Dispose();
    cts.Cancel();
    EXPECT_FALSE(called);
}

TEST(CancellationTokenRegisterTests, Registration_IsActiveUntilDisposedOrCancelled) {
    CancellationTokenSource cts;
    auto reg = cts.getTokenProperty().Register([] {});
    EXPECT_TRUE(reg.getIsActiveProperty());
    cts.Cancel();
    EXPECT_FALSE(reg.getIsActiveProperty());
}

// ===========================================================================
// LockCookie
// ===========================================================================

TEST(LockCookieTests, DefaultCookies_AreEqual) {
    LockCookie a, b;
    EXPECT_EQ(a, b);
    EXPECT_TRUE(a.Equals(b));
}

// ===========================================================================
// ReaderWriterLock (legacy)
// ===========================================================================

TEST(ReaderWriterLockTests, AcquireReleaseReaderLock_TracksHeldState) {
    ReaderWriterLock lock;
    EXPECT_FALSE(lock.getIsReaderLockHeldProperty());
    lock.AcquireReaderLock(-1);
    EXPECT_TRUE(lock.getIsReaderLockHeldProperty());
    lock.ReleaseReaderLock();
    EXPECT_FALSE(lock.getIsReaderLockHeldProperty());
}

TEST(ReaderWriterLockTests, AcquireReleaseWriterLock_TracksHeldState) {
    ReaderWriterLock lock;
    EXPECT_FALSE(lock.getIsWriterLockHeldProperty());
    lock.AcquireWriterLock(-1);
    EXPECT_TRUE(lock.getIsWriterLockHeldProperty());
    lock.ReleaseWriterLock();
    EXPECT_FALSE(lock.getIsWriterLockHeldProperty());
}

TEST(ReaderWriterLockTests, NestedReaderLock_RequiresMatchingReleases) {
    ReaderWriterLock lock;
    lock.AcquireReaderLock(-1);
    lock.AcquireReaderLock(-1);
    EXPECT_TRUE(lock.getIsReaderLockHeldProperty());
    lock.ReleaseReaderLock();
    EXPECT_TRUE(lock.getIsReaderLockHeldProperty());
    lock.ReleaseReaderLock();
    EXPECT_FALSE(lock.getIsReaderLockHeldProperty());
}

TEST(ReaderWriterLockTests, UpgradeToWriterLock_ThenDowngrade_RestoresReaderLock) {
    ReaderWriterLock lock;
    lock.AcquireReaderLock(-1);
    LockCookie cookie = lock.UpgradeToWriterLock(-1);
    EXPECT_TRUE(lock.getIsWriterLockHeldProperty());
    lock.DowngradeFromWriterLock(cookie);
    EXPECT_FALSE(lock.getIsWriterLockHeldProperty());
    EXPECT_TRUE(lock.getIsReaderLockHeldProperty());
    lock.ReleaseReaderLock();
}

TEST(ReaderWriterLockTests, ReleaseLock_ThenRestoreLock_RoundTrips) {
    ReaderWriterLock lock;
    lock.AcquireWriterLock(-1);
    LockCookie cookie = lock.ReleaseLock();
    EXPECT_FALSE(lock.getIsWriterLockHeldProperty());
    lock.RestoreLock(cookie);
    EXPECT_TRUE(lock.getIsWriterLockHeldProperty());
    lock.ReleaseWriterLock();
}

TEST(ReaderWriterLockTests, WriterSeqNum_IncrementsOnEachRelease) {
    ReaderWriterLock lock;
    SharpRuntime::intcs initial = lock.getWriterSeqNumProperty();
    lock.AcquireWriterLock(-1);
    lock.ReleaseWriterLock();
    EXPECT_GT(lock.getWriterSeqNumProperty(), initial);
}

TEST(ReaderWriterLockTests, AcquireReaderLock_NegativeTimeoutBelowMinusOne_Throws) {
    ReaderWriterLock lock;
    EXPECT_THROW(lock.AcquireReaderLock(-2), System::ArgumentOutOfRangeException);
}

// ===========================================================================
// ReaderWriterLockSlim held-state tracking
// ===========================================================================

TEST(ReaderWriterLockSlimTests, IsReadLockHeld_TracksActualState) {
    ReaderWriterLockSlim rwl;
    EXPECT_FALSE(rwl.getIsReadLockHeldProperty());
    rwl.EnterReadLock();
    EXPECT_TRUE(rwl.getIsReadLockHeldProperty());
    rwl.ExitReadLock();
    EXPECT_FALSE(rwl.getIsReadLockHeldProperty());
}

TEST(ReaderWriterLockSlimTests, IsWriteLockHeld_TracksActualState) {
    ReaderWriterLockSlim rwl;
    EXPECT_FALSE(rwl.getIsWriteLockHeldProperty());
    rwl.EnterWriteLock();
    EXPECT_TRUE(rwl.getIsWriteLockHeldProperty());
    rwl.ExitWriteLock();
    EXPECT_FALSE(rwl.getIsWriteLockHeldProperty());
}

TEST(ReaderWriterLockSlimTests, IsUpgradeableReadLockHeld_TracksActualState) {
    ReaderWriterLockSlim rwl;
    EXPECT_FALSE(rwl.getIsUpgradeableReadLockHeldProperty());
    rwl.EnterUpgradeableReadLock();
    EXPECT_TRUE(rwl.getIsUpgradeableReadLockHeldProperty());
    rwl.ExitUpgradeableReadLock();
    EXPECT_FALSE(rwl.getIsUpgradeableReadLockHeldProperty());
}

// ===========================================================================
// Mutex: initiallyOwned + real timeout
// ===========================================================================

TEST(MutexTests, InitiallyOwned_True_IsAlreadyLockedByConstructingThread) {
    Mutex m(true);
    // Recursive: the constructing thread can re-enter without blocking.
    EXPECT_TRUE(m.WaitOne(0));
    m.ReleaseMutex();
    m.ReleaseMutex();
}

TEST(MutexTests, WaitOne_Timeout_WhenHeldByAnotherThread_WaitsFullDuration) {
    Mutex m;
    m.WaitOne();
    std::atomic<bool> acquired{true};
    auto start = std::chrono::steady_clock::now();
    std::thread t([&] { acquired = m.WaitOne(100); });
    t.join();
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_FALSE(acquired.load());
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 90);
    m.ReleaseMutex();
}

// ===========================================================================
// Barrier: post-phase exception wrapping
// ===========================================================================

TEST(BarrierTests, PostPhaseActionThrows_WrappedInBarrierPostPhaseException) {
    Barrier b(1, [](Barrier&) { throw std::runtime_error("boom"); });
    EXPECT_THROW(b.SignalAndWait(), BarrierPostPhaseException);
}

TEST(BarrierTests, RemoveParticipant_ToZero_Throws) {
    Barrier b(0);
    EXPECT_THROW(b.RemoveParticipant(), System::ArgumentOutOfRangeException);
}

// ===========================================================================
// ExecutionContext
// ===========================================================================

TEST(ExecutionContextTests, Capture_ReturnsNullptr) {
    EXPECT_EQ(ExecutionContext::Capture(), nullptr);
}

TEST(ExecutionContextTests, Run_InvokesCallbackSynchronously) {
    bool called = false;
    ExecutionContext::Run(nullptr, [&](void*) { called = true; }, nullptr);
    EXPECT_TRUE(called);
}

TEST(ExecutionContextTests, IsFlowSuppressed_DefaultsFalse) {
    EXPECT_FALSE(ExecutionContext::IsFlowSuppressed());
}

// ===========================================================================
// IThreadPoolWorkItem / WaitCallback / ThreadPool::UnsafeQueueUserWorkItem
// ===========================================================================

namespace {
    class CountingWorkItem : public IThreadPoolWorkItem {
    public:
        std::atomic<int>* counter;
        explicit CountingWorkItem(std::atomic<int>* c) : counter(c) {}
        void Execute() override { counter->fetch_add(1); }
    };
}

TEST(IThreadPoolWorkItemTests, UnsafeQueueUserWorkItem_ExecutesWorkItem) {
    std::atomic<int> counter{0};
    CountingWorkItem item(&counter);
    ThreadPool::UnsafeQueueUserWorkItem(&item, false);
    for (int i = 0; i < 100 && counter.load() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(counter.load(), 1);
}

// ===========================================================================
// RegisteredWaitHandle / ThreadPool::RegisterWaitForSingleObject
// ===========================================================================

TEST(RegisteredWaitHandleTests, RegisterWaitForSingleObject_WithSemaphore_InvokesCallback) {
    Semaphore sem(1, 1);
    std::atomic<int> callCount{0};
    std::atomic<bool> lastTimedOut{true};
    auto rwh = ThreadPool::RegisterWaitForSingleObject(
        &sem,
        [&](void*, bool timedOut) { callCount.fetch_add(1); lastTimedOut = timedOut; },
        nullptr, 5000, true);
    for (int i = 0; i < 200 && callCount.load() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(callCount.load(), 1);
    EXPECT_FALSE(lastTimedOut.load());
    rwh.Unregister(nullptr);
}

// ===========================================================================
// WaitHandle::WaitAll / WaitAny
// ===========================================================================

TEST(WaitHandleTests, WaitAll_AllSignaled_ReturnsTrue) {
    Semaphore s1(1, 1), s2(1, 1);
    std::vector<WaitHandle*> handles{&s1, &s2};
    EXPECT_TRUE(WaitHandle::WaitAll(handles));
}

TEST(WaitHandleTests, WaitAny_OneSignaled_ReturnsItsIndex) {
    Semaphore s1(0, 1), s2(1, 1);
    std::vector<WaitHandle*> handles{&s1, &s2};
    EXPECT_EQ(WaitHandle::WaitAny(handles), 1);
}

TEST(WaitHandleTests, WaitAny_NoneSignaled_TimesOutWithWaitTimeout) {
    Semaphore s1(0, 1), s2(0, 1);
    std::vector<WaitHandle*> handles{&s1, &s2};
    EXPECT_EQ(WaitHandle::WaitAny(handles, 50), WaitHandle::WaitTimeout);
}
