// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <condition_variable>
#include <mutex>
#include <unordered_set>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IDisposable.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Threading/LockRecursionPolicy.hpp"
#include "System/Threading/SynchronizationLockException.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /** A reader/writer lock that allows multiple simultaneous readers or one exclusive writer. */
    class ReaderWriterLockSlim : public System::IDisposable {
        // Hand-rolled monitor rather than std::shared_mutex: std::shared_mutex has no upgrade
        // primitive, and calling lock() on a thread that already holds lock_shared() on the
        // same shared_mutex (the EnterUpgradeableReadLock() -> EnterWriteLock() pattern that is
        // the entire reason to use the upgradeable lock) is undefined behavior. This state
        // machine lets the upgrade-lock owner acquire the write lock by waiting only for other
        // readers to drain, not for itself.
        mutable std::mutex stateMtx_;
        mutable std::condition_variable cv_;
        intcs readers_ = 0;
        bool writerActive_ = false;
        bool upgradeableActive_ = false;
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

        [[nodiscard]] bool isUpgradeOwner() const { return UpgradeableThreadSet().count(this) != 0; }

        void throwIfDisposed() const {
            if (disposed_) throw System::ObjectDisposedException("ReaderWriterLockSlim");
        }

    public:
        /** Constructs a ReaderWriterLockSlim with no-recursion policy. */
        ReaderWriterLockSlim() = default;
        /** Constructs a ReaderWriterLockSlim; the recursionPolicy parameter is accepted but ignored. */
        explicit ReaderWriterLockSlim(LockRecursionPolicy /*recursionPolicy*/) {}

        /** Acquires a shared read lock, blocking until it becomes available. */
        void EnterReadLock() {
            throwIfDisposed();
            std::unique_lock<std::mutex> lk(stateMtx_);
            cv_.wait(lk, [&] { return !writerActive_; });
            ++readers_;
            lk.unlock();
            ReaderThreadSet().insert(this);
        }
        /** Releases a shared read lock. @throws System::Threading::SynchronizationLockException if the calling thread does not hold a read lock. */
        void ExitReadLock() {
            if (!ReaderThreadSet().count(this)) throw System::Threading::SynchronizationLockException();
            ReaderThreadSet().erase(this);
            {
                std::lock_guard<std::mutex> lk(stateMtx_);
                --readers_;
            }
            cv_.notify_all();
        }
        /** Tries to acquire a shared read lock without blocking; returns true on success. */
        bool TryEnterReadLock(intcs /*millisecondsTimeout*/) {
            throwIfDisposed();
            std::unique_lock<std::mutex> lk(stateMtx_);
            if (writerActive_) return false;
            ++readers_;
            lk.unlock();
            ReaderThreadSet().insert(this);
            return true;
        }

        /** Acquires an exclusive write lock, blocking until it becomes available. */
        void EnterWriteLock() {
            throwIfDisposed();
            std::unique_lock<std::mutex> lk(stateMtx_);
            bool owner = isUpgradeOwner();
            cv_.wait(lk, [&] { return !writerActive_ && readers_ == 0 && (!upgradeableActive_ || owner); });
            writerActive_ = true;
            lk.unlock();
            WriterThreadSet().insert(this);
        }
        /** Releases the exclusive write lock. @throws System::Threading::SynchronizationLockException if the calling thread does not hold the write lock. */
        void ExitWriteLock() {
            if (!WriterThreadSet().count(this)) throw System::Threading::SynchronizationLockException();
            WriterThreadSet().erase(this);
            {
                std::lock_guard<std::mutex> lk(stateMtx_);
                writerActive_ = false;
            }
            cv_.notify_all();
        }
        /** Tries to acquire an exclusive write lock without blocking; returns true on success. */
        bool TryEnterWriteLock(intcs /*millisecondsTimeout*/) {
            throwIfDisposed();
            std::unique_lock<std::mutex> lk(stateMtx_);
            bool owner = isUpgradeOwner();
            if (writerActive_ || readers_ != 0 || (upgradeableActive_ && !owner)) return false;
            writerActive_ = true;
            lk.unlock();
            WriterThreadSet().insert(this);
            return true;
        }

        /**
         * @brief Acquires the upgradeable read lock, blocking until it becomes available.
         *
         * At most one thread may hold the upgradeable read lock at a time. While held, the
         * owning thread may subsequently call EnterWriteLock() to upgrade to the exclusive
         * write lock — that call only waits for other readers to drain, not for the
         * upgradeable lock itself, avoiding the self-deadlock that a naive
         * lock_shared()-then-lock() sequence on the same mutex would cause.
         */
        void EnterUpgradeableReadLock() {
            throwIfDisposed();
            std::unique_lock<std::mutex> lk(stateMtx_);
            cv_.wait(lk, [&] { return !writerActive_ && !upgradeableActive_; });
            upgradeableActive_ = true;
            lk.unlock();
            UpgradeableThreadSet().insert(this);
        }
        /** Releases the upgradeable read lock. @throws System::Threading::SynchronizationLockException if the calling thread does not hold it. */
        void ExitUpgradeableReadLock() {
            if (!UpgradeableThreadSet().count(this)) throw System::Threading::SynchronizationLockException();
            UpgradeableThreadSet().erase(this);
            {
                std::lock_guard<std::mutex> lk(stateMtx_);
                upgradeableActive_ = false;
            }
            cv_.notify_all();
        }
        /** Tries to acquire the upgradeable read lock without blocking; returns true on success. */
        bool TryEnterUpgradeableReadLock(intcs /*millisecondsTimeout*/) {
            throwIfDisposed();
            std::unique_lock<std::mutex> lk(stateMtx_);
            if (writerActive_ || upgradeableActive_) return false;
            upgradeableActive_ = true;
            lk.unlock();
            UpgradeableThreadSet().insert(this);
            return true;
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
