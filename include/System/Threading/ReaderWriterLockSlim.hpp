// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <shared_mutex>
#include "System/IDisposable.hpp"
#include "System/Threading/LockRecursionPolicy.hpp"

namespace System::Threading {

    /// A reader/writer lock that allows multiple simultaneous readers or one exclusive writer.
    class ReaderWriterLockSlim : public System::IDisposable {
        mutable std::shared_mutex mtx_;
        bool disposed_ = false;

    public:
        /// Constructs a ReaderWriterLockSlim with no-recursion policy.
        ReaderWriterLockSlim() = default;
        /// Constructs a ReaderWriterLockSlim; the recursionPolicy parameter is accepted but ignored.
        explicit ReaderWriterLockSlim(LockRecursionPolicy /*recursionPolicy*/) {}

        /// Acquires a shared read lock, blocking until it becomes available.
        void EnterReadLock()  { mtx_.lock_shared(); }
        /// Releases a shared read lock.
        void ExitReadLock()   { mtx_.unlock_shared(); }
        /// Tries to acquire a shared read lock without blocking; returns true on success.
        bool TryEnterReadLock(int /*millisecondsTimeout*/) { return mtx_.try_lock_shared(); }

        /// Acquires an exclusive write lock, blocking until it becomes available.
        void EnterWriteLock() { mtx_.lock(); }
        /// Releases the exclusive write lock.
        void ExitWriteLock()  { mtx_.unlock(); }
        /// Tries to acquire an exclusive write lock without blocking; returns true on success.
        bool TryEnterWriteLock(int /*millisecondsTimeout*/) { return mtx_.try_lock(); }

        /// Acquires the upgradeable read lock (backed by a shared lock in this implementation).
        void EnterUpgradeableReadLock()  { mtx_.lock_shared(); }
        /// Releases the upgradeable read lock.
        void ExitUpgradeableReadLock()   { mtx_.unlock_shared(); }

        /// Releases all resources held by the lock.
        void Dispose() override { disposed_ = true; }

        /// Returns whether the current thread holds a read lock (always false; cannot be queried).
        [[nodiscard]] bool getIsReadLockHeldProperty() const  { return false; } // can't query
        /// Returns whether the current thread holds the write lock (always false; cannot be queried).
        [[nodiscard]] bool getIsWriteLockHeldProperty() const { return false; }
    };

} // namespace System::Threading
