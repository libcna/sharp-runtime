// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IDisposable.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Threading/LockRecursionException.hpp"
#include "System/Threading/LockRecursionPolicy.hpp"
#include "System/Threading/SynchronizationLockException.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /**
     * @brief A reader/writer lock that allows multiple simultaneous readers or one exclusive writer.
     *
     * C++ counterpart of .NET System.Threading.ReaderWriterLockSlim.
     *
     * @note Verified against ReaderWriterLockSlim.cs's TryEnterReadLockCore/TryEnterWriteLockCore/
     * TryEnterUpgradeableReadLockCore/Exit*. This port previously (1) discarded the
     * millisecondsTimeout parameter on all three TryEnter* methods, always making a single
     * non-blocking attempt regardless of what the caller passed; (2) ignored the constructor's
     * LockRecursionPolicy entirely, so same-thread recursive acquisition always **deadlocked**
     * (blocking forever waiting for a lock the same thread already held) instead of throwing
     * LockRecursionException under the .NET-default NoRecursion policy; and (3) tracked
     * reader-lock ownership via set *membership* rather than a count, so a legitimately nested
     * EnterReadLock()/EnterReadLock()/ExitReadLock() sequence's second ExitReadLock() call threw
     * SynchronizationLockException (membership already removed by the first exit) instead of
     * decrementing — worse, this also meant the internal reader tally never reached zero from a
     * genuinely-nested acquisition, which would have permanently starved any waiting writer had
     * recursion not otherwise deadlocked first.
     *
     * Reworked to: use real timed condition-variable waits (matching this session's established
     * Timeout.Infinite-aware pattern); track per-thread reader/writer/upgrade *counts* instead of
     * set membership, keyed by a monotonically-increasing per-instance ID rather than `this`
     * (matching the AsyncLocal<T>/ThreadLocal<T> fix earlier this session — avoids the same
     * address-reuse data-corruption risk `this`-pointer-keying has); and apply
     * LockRecursionPolicy's actual rules: cross-type combinations that are always deadlock-prone
     * (write-after-read, upgrade-after-read, upgrade-after-write) always throw
     * LockRecursionException regardless of policy (verified: real .NET performs these specific
     * checks identically in both its NoRecursion and SupportsRecursion code paths), while
     * same-type recursion (read-after-read, write-after-write, upgrade-after-upgrade) only
     * throws under NoRecursion and is allowed (as a tracked, matched-by-count recursion) under
     * SupportsRecursion. The existing upgrade-lock design (an upgrade owner's EnterWriteLock()
     * call waits only for other readers to drain, not for the upgrade slot itself, avoiding the
     * self-deadlock a naive lock_shared()-then-lock() sequence would cause) is preserved.
     */
    class ReaderWriterLockSlim : public System::IDisposable {
        struct ThreadCounts {
            intcs reader = 0;
            intcs writer = 0;
            intcs upgrade = 0;
            // True if this thread's currently-outstanding read acquisition incremented
            // `readers_`. A read acquisition implied by already holding the write or upgrade
            // lock never increments `readers_` (that reader is already accounted for via
            // writerActive_/upgradeableActive_, and NOT counting it lets an upgrade owner's
            // EnterWriteLock() wait for "other readers drained" without waiting on itself).
            bool readerCountsTowardGlobal = false;
        };

        static std::atomic<std::uint64_t>& nextId() {
            static std::atomic<std::uint64_t> id{0};
            return id;
        }
        std::uint64_t id_ = nextId().fetch_add(1, std::memory_order_relaxed);

        static std::unordered_map<std::uint64_t, ThreadCounts>& threadCounts() {
            thread_local std::unordered_map<std::uint64_t, ThreadCounts> map;
            return map;
        }
        [[nodiscard]] ThreadCounts& myCounts() { return threadCounts()[id_]; }

        // Hand-rolled monitor rather than std::shared_mutex: std::shared_mutex has no upgrade
        // primitive, and calling lock() on a thread that already holds lock_shared() on the
        // same shared_mutex (the EnterUpgradeableReadLock() -> EnterWriteLock() pattern that is
        // the entire reason to use the upgradeable lock) is undefined behavior. This state
        // machine lets the upgrade-lock owner acquire the write lock by waiting only for other
        // readers to drain, not for itself.
        mutable std::mutex stateMtx_;
        mutable std::condition_variable cv_;
        intcs readers_ = 0;
        // Ticket #1957 / SR-AUD-204, cause T-E/2. Without this the read-admission predicate was
        // `!writerActive_` alone, so a steady stream of new readers could enter past a writer
        // already blocked in TryEnterWriteLock and starve it INDEFINITELY.
        //
        // .NET keeps the same signal, packed into its single `_owners` word: WaitOnEvent sets
        // WAITING_WRITERS (and WAITING_UPGRADER) as soon as the first writer begins waiting,
        // under the comment "Setting these bits will prevent new readers from getting in"
        // (ReaderWriterLockSlim.cs:1005-1010). Both bits sit above MAX_READER, so .NET's single
        // admission test `_owners < MAX_READER` refuses a new reader whenever a writer holds the
        // lock OR is waiting for it. This counter is that bit, spelled separately because this
        // port keeps its state in named fields rather than one packed word.
        //
        // BOTH kinds of writer count, exactly as both .NET bits do: a plain writer and an
        // upgrade-to-write. .NET clears each in WaitOnEvent's `finally`
        // (ReaderWriterLockSlim.cs:1039-1042), so a writer that TIMES OUT stops blocking readers;
        // the RAII guard in TryEnterWriteLock does the same here.
        intcs waitingWriters_ = 0;
        // Ticket #2389. .NET's Dispose refuses a lock that has WAITERS as well as one with a
        // held mode, and it counts all three kinds (ReaderWriterLockSlim.cs:1254). #1956
        // implemented the held-mode check only, because these two counters did not exist; they
        // do now, on the same RAII pattern waitingWriters_ uses.
        //
        // These are NOT consulted by any admission predicate -- only writer-waiting affects
        // admission, which is #1957/SR-AUD-204's rule and .NET's. They exist for Dispose.
        intcs waitingReaders_ = 0;
        intcs waitingUpgraders_ = 0;

        /// Increments a waiter count for the duration of a wait and restores it on every exit.
        struct WaiterCountGuard {
            intcs&                   count;
            std::condition_variable& cv;
            bool                     notifyOnLast;
            ~WaiterCountGuard() {
                if (--count == 0 && notifyOnLast) cv.notify_all();
            }
        };
        bool writerActive_ = false;
        bool upgradeableActive_ = false;
        // Ticket #1955 / cause T-A of docs/ThreadingNamespaceReviewPlan.md. This was an
        // ordinary `bool`, written by Dispose() and read by the guard below with no
        // synchronisation between them. Mixing synchronised and unsynchronised access to the
        // same object is a data race and therefore undefined behaviour, and ThreadSanitizer
        // confirmed it both at audit time and again in
        // build-probe/1955_probe1_shared_state_races.cpp. std::atomic<bool> is 1 byte and
        // 1-byte aligned on every supported target -- measured before and after in
        // build-probe/1955_probe1_layout_{before,after}.log -- so the flag's type change is
        // layout-neutral and the header stays consumer-compatible.
        std::atomic<bool> disposed_{false};
        LockRecursionPolicy recursionPolicy_ = LockRecursionPolicy::NoRecursion;

        void throwIfDisposed() const {
            if (disposed_.load(std::memory_order_acquire))
                throw System::ObjectDisposedException("ReaderWriterLockSlim");
        }

        [[nodiscard]] bool isReentrant() const { return recursionPolicy_ == LockRecursionPolicy::SupportsRecursion; }

        // Waits for pred() while holding lk, honoring .NET's Timeout.Infinite (-1) convention.
        // Returns false (without throwing) if millisecondsTimeout elapses before pred() is true.
        template<typename Pred>
        bool waitFor(std::unique_lock<std::mutex>& lk, intcs millisecondsTimeout, Pred pred) {
            if (millisecondsTimeout == -1) {
                cv_.wait(lk, pred);
                return true;
            }
            return cv_.wait_for(lk, std::chrono::milliseconds(millisecondsTimeout), pred);
        }

        static void validateTimeout(intcs millisecondsTimeout) {
            System::ArgumentOutOfRangeException::ThrowIfLessThan(millisecondsTimeout, -1, "millisecondsTimeout");
        }

    public:
        /** Constructs a ReaderWriterLockSlim with no-recursion policy. */
        ReaderWriterLockSlim() = default;
        /**
         * @brief Constructs a ReaderWriterLockSlim with the specified recursion policy.
         *
         * Any value other than `LockRecursionPolicy::SupportsRecursion` is **normalised** to
         * `NoRecursion` rather than rejected, because that is precisely what .NET does:
         * `ReaderWriterLockSlim(LockRecursionPolicy)` stores only
         * `_fIsReentrant = (recursionPolicy == LockRecursionPolicy.SupportsRecursion)` and
         * `RecursionPolicy` is derived from that bool, so no undeclared value can survive to
         * be read back. This port stored the raw value and reflected it verbatim
         * (SR-AUD-205, ticket #1954).
         *
         * The behavioural half was already correct -- `isReentrant()` tests for
         * `SupportsRecursion`, so an undeclared policy already locked like `NoRecursion` and
         * already threw `LockRecursionException` on recursive entry; only the property lied.
         * Normalising at construction makes the stored state and the reported state agree
         * with each other and with the behaviour, in one place.
         *
         * `EventWaitHandle` in the same ticket **rejects** its undeclared enum instead. That
         * asymmetry is .NET's own and is deliberately not unified.
         */
        explicit ReaderWriterLockSlim(LockRecursionPolicy recursionPolicy)
            : recursionPolicy_(recursionPolicy == LockRecursionPolicy::SupportsRecursion
                                   ? LockRecursionPolicy::SupportsRecursion
                                   : LockRecursionPolicy::NoRecursion) {}

        /** Releases this instance's slot in the current thread's per-thread tracking on destruction. */
        ~ReaderWriterLockSlim() override { threadCounts().erase(id_); }

        /**
         * @brief Returns this instance's recursion policy.
         *
         * Always `NoRecursion` or `SupportsRecursion`: an undeclared constructor argument is
         * normalised to `NoRecursion`, matching .NET's derived-from-a-bool property.
         */
        [[nodiscard]] LockRecursionPolicy getRecursionPolicyProperty() const { return recursionPolicy_; }

        /**
         * @brief Acquires a shared read lock, blocking until it becomes available.
         * @throws LockRecursionException per LockRecursionPolicy — see class doc comment.
         */
        void EnterReadLock() { TryEnterReadLock(-1); }

        /** Releases a shared read lock. @throws System::Threading::SynchronizationLockException if the calling thread does not hold a read lock. */
        void ExitReadLock() {
            auto& map = threadCounts();
            auto it = map.find(id_);
            if (it == map.end() || it->second.reader < 1)
                throw System::Threading::SynchronizationLockException("The read lock is being released without being held.");
            ThreadCounts& counts = it->second;
            --counts.reader;
            if (counts.reader == 0 && counts.readerCountsTowardGlobal) {
                {
                    std::lock_guard<std::mutex> lk(stateMtx_);
                    --readers_;
                }
                counts.readerCountsTowardGlobal = false;
                cv_.notify_all();
            }
        }

        /**
         * @brief Tries to acquire a shared read lock, blocking up to millisecondsTimeout milliseconds (-1 = infinite).
         * @throws LockRecursionException per LockRecursionPolicy — see class doc comment.
         */
        bool TryEnterReadLock(intcs millisecondsTimeout) {
            throwIfDisposed();
            validateTimeout(millisecondsTimeout);
            ThreadCounts& counts = myCounts();

            if (counts.reader > 0) {
                if (!isReentrant())
                    throw LockRecursionException("Recursive read lock acquisitions not allowed in this mode.");
                ++counts.reader;
                return true;
            }
            if (!isReentrant() && counts.writer > 0)
                throw LockRecursionException("A read lock may not be acquired with the write lock held in this mode.");

            // Holding the write or upgrade lock already implies read access (and, under
            // SupportsRecursion, is allowed even for the write-lock case) -- never blocks.
            if (counts.writer > 0 || counts.upgrade > 0) {
                counts.reader = 1;
                counts.readerCountsTowardGlobal = false;
                return true;
            }

            std::unique_lock<std::mutex> lk(stateMtx_);
            // #1957/SR-AUD-204: `waitingWriters_ == 0` is the new term. A thread that already
            // holds the read, write or upgrade lock never reaches here -- every one of those
            // cases returned above -- so writer preference can only delay a genuinely NEW
            // reader, which is precisely .NET's contract and cannot deadlock a recursive one.
            // #2389: counted for Dispose's benefit only -- a waiting reader blocks nothing.
            ++waitingReaders_;
            WaiterCountGuard readerWaitGuard{waitingReaders_, cv_, /*notifyOnLast=*/false};
            if (!waitFor(lk, millisecondsTimeout,
                         [&] { return !writerActive_ && waitingWriters_ == 0; }))
                return false;
            ++readers_;
            counts.reader = 1;
            counts.readerCountsTowardGlobal = true;
            return true;
        }

        /**
         * @brief Acquires an exclusive write lock, blocking until it becomes available.
         * @throws LockRecursionException per LockRecursionPolicy — see class doc comment.
         */
        void EnterWriteLock() { TryEnterWriteLock(-1); }

        /** Releases the exclusive write lock. @throws System::Threading::SynchronizationLockException if the calling thread does not hold the write lock. */
        void ExitWriteLock() {
            auto& map = threadCounts();
            auto it = map.find(id_);
            if (it == map.end() || it->second.writer < 1)
                throw System::Threading::SynchronizationLockException("The write lock is being released without being held.");
            ThreadCounts& counts = it->second;
            if (--counts.writer == 0) {
                {
                    std::lock_guard<std::mutex> lk(stateMtx_);
                    writerActive_ = false;
                }
                cv_.notify_all();
            }
        }

        /**
         * @brief Tries to acquire an exclusive write lock, blocking up to millisecondsTimeout milliseconds (-1 = infinite).
         * @throws LockRecursionException per LockRecursionPolicy — see class doc comment.
         */
        bool TryEnterWriteLock(intcs millisecondsTimeout) {
            throwIfDisposed();
            validateTimeout(millisecondsTimeout);
            ThreadCounts& counts = myCounts();

            if (counts.writer > 0) {
                if (!isReentrant())
                    throw LockRecursionException("Recursive write lock acquisitions not allowed in this mode.");
                ++counts.writer;
                return true;
            }

            bool upgradingToWrite = counts.upgrade > 0;
            // Write-after-read is always disallowed (deadlock-prone), regardless of policy --
            // verified: real .NET performs this exact check in both its NoRecursion and
            // SupportsRecursion branches.
            if (!upgradingToWrite && counts.reader > 0)
                throw LockRecursionException(
                    "Write lock may not be acquired with read lock held. This pattern is prone to "
                    "deadlocks. Please ensure that read locks are released before taking a write "
                    "lock. If an upgrade is necessary, use an upgrade lock in place of the read lock.");

            std::unique_lock<std::mutex> lk(stateMtx_);

            // Announce the wait BEFORE waiting, so readers arriving from now on are refused --
            // .NET sets its bit at the same point, before the wait rather than after it. The
            // guard decrements on every exit (acquired, timed out, or thrown) and wakes the
            // readers it was holding back, which is what .NET's `finally` does via
            // ClearWritersWaiting + ExitAndWakeUpAppropriateReadWaiters.
            //
            // Declared after `lk` so it is destroyed BEFORE the lock is released: the decrement
            // and the notify both happen under the mutex.
            ++waitingWriters_;
            WaiterCountGuard waitingWriterGuard{waitingWriters_, cv_, /*notifyOnLast=*/true};

            bool acquired = upgradingToWrite
                ? waitFor(lk, millisecondsTimeout, [&] { return readers_ == 0; })
                : waitFor(lk, millisecondsTimeout, [&] { return !writerActive_ && readers_ == 0 && !upgradeableActive_; });
            if (!acquired) return false;
            writerActive_ = true;
            ++counts.writer;
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
         * @throws LockRecursionException per LockRecursionPolicy — see class doc comment.
         */
        void EnterUpgradeableReadLock() { TryEnterUpgradeableReadLock(-1); }

        /** Releases the upgradeable read lock. @throws System::Threading::SynchronizationLockException if the calling thread does not hold it. */
        void ExitUpgradeableReadLock() {
            auto& map = threadCounts();
            auto it = map.find(id_);
            if (it == map.end() || it->second.upgrade < 1)
                throw System::Threading::SynchronizationLockException("The upgradeable lock is being released without being held.");
            ThreadCounts& counts = it->second;
            if (--counts.upgrade == 0) {
                {
                    std::lock_guard<std::mutex> lk(stateMtx_);
                    upgradeableActive_ = false;
                }
                cv_.notify_all();
            }
        }

        /**
         * @brief Tries to acquire the upgradeable read lock, blocking up to millisecondsTimeout milliseconds (-1 = infinite).
         * @throws LockRecursionException per LockRecursionPolicy — see class doc comment.
         */
        bool TryEnterUpgradeableReadLock(intcs millisecondsTimeout) {
            throwIfDisposed();
            validateTimeout(millisecondsTimeout);
            ThreadCounts& counts = myCounts();

            if (counts.upgrade > 0) {
                if (!isReentrant())
                    throw LockRecursionException("Recursive upgradeable lock acquisitions not allowed in this mode.");
                ++counts.upgrade;
                return true;
            }
            if (counts.writer > 0) {
                if (!isReentrant())
                    throw LockRecursionException(
                        "Upgradeable lock may not be acquired with write lock held in this mode. "
                        "Acquiring Upgradeable lock gives the ability to read along with an option "
                        "to upgrade to a writer.");
                // Reentrant: the write lock already held implies exclusive access, so the
                // upgrade slot is granted immediately with no wait.
                std::lock_guard<std::mutex> lk(stateMtx_);
                upgradeableActive_ = true;
                ++counts.upgrade;
                return true;
            }
            // Upgrade-after-read is always disallowed, regardless of policy -- verified: real
            // .NET performs this exact check in both its NoRecursion and SupportsRecursion
            // branches.
            if (counts.reader > 0)
                throw LockRecursionException("Upgradeable lock may not be acquired with read lock held.");

            std::unique_lock<std::mutex> lk(stateMtx_);
            // #2389: counted for Dispose's benefit only.
            ++waitingUpgraders_;
            WaiterCountGuard upgraderWaitGuard{waitingUpgraders_, cv_, /*notifyOnLast=*/false};
            if (!waitFor(lk, millisecondsTimeout, [&] { return !writerActive_ && !upgradeableActive_; })) return false;
            upgradeableActive_ = true;
            ++counts.upgrade;
            return true;
        }

        /**
         * @brief Disposes the lock.
         * @throws System::Threading::SynchronizationLockException if the calling thread still
         *         holds the read, write or upgradeable-read mode.
         *
         * Ticket #1956 / cause T-G (SR-AUD-203, dispose-while-held half). This used to set the
         * flag unconditionally, so disposing with a lock held succeeded and left the holder
         * owning a mode on a disposed object. .NET refuses
         * (`ReaderWriterLockSlim.cs:1250-1258`).
         *
         * @throws System::Threading::SynchronizationLockException if any thread is WAITING to
         *         acquire the lock.
         *
         * Both checks are .NET's, in .NET's order (`ReaderWriterLockSlim.cs:1250-1258`):
         * @code
         * if (WaitingReadCount > 0 || WaitingUpgradeCount > 0 || WaitingWriteCount > 0) throw ...;
         * if (IsReadLockHeld || IsUpgradeableReadLockHeld || IsWriteLockHeld)          throw ...;
         * @endcode
         * #1956 landed the second; ticket **#2389** added the waiter counts and the first.
         *
         * @note The order is transcribed rather than chosen. Both arms raise the same message
         * here, so which one fires is currently unobservable -- but the ordering is .NET's and a
         * test pins it, so it cannot be inverted casually if the messages ever diverge.
         */
        void Dispose() override {
            std::unique_lock<std::mutex> lk(stateMtx_);
            if (waitingReaders_ > 0 || waitingUpgraders_ > 0 || waitingWriters_ > 0) {
                throw System::Threading::SynchronizationLockException(
                    "The lock is being disposed while still being used. It either is being held "
                    "by a thread and/or has active waiters waiting to acquire the lock.");
            }
            lk.unlock();

            auto& map = threadCounts();
            auto it = map.find(id_);
            if (it != map.end() &&
                (it->second.reader > 0 || it->second.writer > 0 || it->second.upgrade > 0)) {
                throw System::Threading::SynchronizationLockException(
                    "The lock is being disposed while still being used. It either is being held "
                    "by a thread and/or has active waiters waiting to acquire the lock.");
            }
            disposed_.store(true, std::memory_order_release);
        }

        /** Returns whether the current thread holds a read lock. */
        [[nodiscard]] bool getIsReadLockHeldProperty() const {
            auto& map = threadCounts();
            auto it = map.find(id_);
            return it != map.end() && it->second.reader > 0;
        }
        /** Returns whether the current thread holds the write lock. */
        [[nodiscard]] bool getIsWriteLockHeldProperty() const {
            auto& map = threadCounts();
            auto it = map.find(id_);
            return it != map.end() && it->second.writer > 0;
        }
        /** Returns whether the current thread holds the upgradeable read lock. */
        [[nodiscard]] bool getIsUpgradeableReadLockHeldProperty() const {
            auto& map = threadCounts();
            auto it = map.find(id_);
            return it != map.end() && it->second.upgrade > 0;
        }
    };

} // namespace System::Threading
