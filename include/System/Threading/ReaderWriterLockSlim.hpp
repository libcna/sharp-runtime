// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <chrono>
#include <shared_mutex>
#include <unordered_set>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IDisposable.hpp"
#include "System/Threading/LockRecursionPolicy.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /** A reader/writer lock that allows multiple simultaneous readers or one exclusive writer. */
    class ReaderWriterLockSlim : public System::IDisposable {
        mutable std::shared_mutex mtx_;
        bool disposed_ = false;

        // .NET queries lock ownership per-thread via the thread's sync state; C++ has no
        // equivalent, so ownership is tracked here via thread_local sets keyed on `this`.
        // Deliberate simplification: correct for the common EnterX/ExitX-on-the-same-thread
        // usage pattern, not a full recursion-count model.
        static std::unordered_set<const ReaderWriterLockSlim*>& ReaderThreadSet() {
            thread_local std::unordered_set<const ReaderWriterLockSlim*> set;
            return set;
        }
        static std::unordered_set<const ReaderWriterLockSlim*>& UpgradeableThreadSet() {
            thread_local std::unordered_set<const ReaderWriterLockSlim*> set;
            return set;
        }
        static std::unordered_set<const ReaderWriterLockSlim*>& WriterThreadSet() {
            thread_local std::unordered_set<const ReaderWriterLockSlim*> set;
            return set;
        }

    public:
        /** Constructs a ReaderWriterLockSlim with no-recursion policy. */
        ReaderWriterLockSlim() = default;
        /** Constructs a ReaderWriterLockSlim; the recursionPolicy parameter is accepted but ignored. */
        explicit ReaderWriterLockSlim(LockRecursionPolicy /*recursionPolicy*/) {}

        /** Acquires a shared read lock, blocking until it becomes available. */
        void EnterReadLock() { mtx_.lock_shared(); ReaderThreadSet().insert(this); }
        /** Releases a shared read lock. */
        void ExitReadLock() { mtx_.unlock_shared(); ReaderThreadSet().erase(this); }
        /** Tries to acquire a shared read lock without blocking; returns true on success. */
        bool TryEnterReadLock(intcs /*millisecondsTimeout*/) {
            bool ok = mtx_.try_lock_shared();
            if (ok) ReaderThreadSet().insert(this);
            return ok;
        }

        /** Acquires an exclusive write lock, blocking until it becomes available. */
        void EnterWriteLock() { mtx_.lock(); WriterThreadSet().insert(this); }
        /** Releases the exclusive write lock. */
        void ExitWriteLock() { WriterThreadSet().erase(this); mtx_.unlock(); }
        /** Tries to acquire an exclusive write lock without blocking; returns true on success. */
        bool TryEnterWriteLock(intcs /*millisecondsTimeout*/) {
            bool ok = mtx_.try_lock();
            if (ok) WriterThreadSet().insert(this);
            return ok;
        }

        /** Acquires the upgradeable read lock (backed by a shared lock in this implementation). */
        void EnterUpgradeableReadLock() { mtx_.lock_shared(); UpgradeableThreadSet().insert(this); }
        /** Releases the upgradeable read lock. */
        void ExitUpgradeableReadLock() { mtx_.unlock_shared(); UpgradeableThreadSet().erase(this); }
        /** Tries to acquire the upgradeable read lock without blocking; returns true on success. */
        bool TryEnterUpgradeableReadLock(intcs /*millisecondsTimeout*/) {
            bool ok = mtx_.try_lock_shared();
            if (ok) UpgradeableThreadSet().insert(this);
            return ok;
        }

        /** Releases all resources held by the lock. */
        void Dispose() override { disposed_ = true; }

        /** Returns whether the current thread holds a read lock. */
        [[nodiscard]] bool getIsReadLockHeldProperty() const { return ReaderThreadSet().count(this) != 0; }
        /** Returns whether the current thread holds the write lock. */
        [[nodiscard]] bool getIsWriteLockHeldProperty() const { return WriterThreadSet().count(this) != 0; }
        /** Returns whether the current thread holds the upgradeable read lock. */
        [[nodiscard]] bool getIsUpgradeableReadLockHeldProperty() const { return UpgradeableThreadSet().count(this) != 0; }
    };

} // namespace System::Threading
