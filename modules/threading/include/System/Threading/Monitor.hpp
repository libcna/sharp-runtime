// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Threading/SynchronizationLockException.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /**
     * @brief Provides a mechanism that synchronizes access to objects.
     *
     * Partial C++ counterpart of .NET System.Threading.Monitor. .NET associates a monitor with
     * an object's identity via a per-object sync block in the object header; C++ has no object
     * header, so this port keys a global registry on the caller-supplied pointer's identity
     * instead. Each distinct pointer gets its own recursive_timed_mutex + condition_variable_any
     * pair, created lazily on first use and never freed — a documented, deliberate simplification
     * (matching .NET's lock(obj){...} re-entrancy semantics for the common case, at the cost of a
     * registry entry leak for pointers that go out of use during the process lifetime).
     *
     * @note Status: Implemented (registry-backed); not a no-op stub — Enter/Exit/TryEnter/Wait/
     * Pulse/PulseAll provide real mutual exclusion and wait-set semantics keyed by pointer identity.
     */
    class Monitor {
        struct State {
            std::recursive_timed_mutex mutex;
            std::condition_variable_any cv;
            std::atomic<std::thread::id> owner{};
            std::atomic<int> depth{0};

            void onAcquired() {
                if (depth.fetch_add(1, std::memory_order_relaxed) == 0)
                    owner.store(std::this_thread::get_id(), std::memory_order_relaxed);
            }
            void onReleasing() {
                if (depth.fetch_sub(1, std::memory_order_relaxed) == 1)
                    owner.store(std::thread::id(), std::memory_order_relaxed);
            }
            [[nodiscard]] bool heldByCurrentThread() const {
                return owner.load(std::memory_order_relaxed) == std::this_thread::get_id();
            }
        };

        static std::mutex& RegistryMutex() {
            static std::mutex m;
            return m;
        }

        static std::unordered_map<const void*, std::shared_ptr<State>>& Registry() {
            static std::unordered_map<const void*, std::shared_ptr<State>> registry;
            return registry;
        }

        static std::shared_ptr<State> GetOrCreate(const void* obj) {
            std::lock_guard<std::mutex> lock(RegistryMutex());
            auto& slot = Registry()[obj];
            if (!slot) slot = std::make_shared<State>();
            return slot;
        }

        /**
         * @brief Shared body of both Wait overloads; millisecondsTimeout == -1 waits indefinitely.
         *
         * .NET's Monitor.Wait releases the *whole* recursive count held by the calling thread and
         * restores it on reacquisition.  std::condition_variable_any::wait releases the supplied
         * lock exactly once, which for a std::recursive_timed_mutex means only one level, so the
         * levels beyond the first are released here and reacquired afterwards.  Ticket #2341 /
         * SR-AUD-202: without that, a caller at depth n >= 2 kept n-1 levels locked for the whole
         * wait, so no other thread could Enter() the monitor to Pulse() it and the wait never ended.
         *
         * The registry's owner/depth bookkeeping is written to its absolute values rather than
         * stepped by one, because the whole depth changes at once here.  Every one of those stores
         * happens while this thread holds the mutex, so no other thread can be inside
         * onAcquired()/onReleasing() concurrently; publishing a zero depth before waiting is what
         * lets a different thread's Enter() during the wait window claim ownership.
         */
        static bool WaitCore(const void* obj, intcs millisecondsTimeout) {
            auto state = GetOrCreate(obj);
            if (!state->heldByCurrentThread()) throw System::Threading::SynchronizationLockException();

            const int depth = state->depth.load(std::memory_order_relaxed);
            const int extra = depth > 0 ? depth - 1 : 0;
            for (int i = 0; i < extra; ++i) state->mutex.unlock();

            std::unique_lock<std::recursive_timed_mutex> lock(state->mutex, std::adopt_lock);
            // Clearing the owner keeps the registry invariant "owner is empty whenever depth
            // is zero" true for the whole wait.  It is not independently observable -- the
            // zero depth alone already makes the next Enter() claim ownership, and ticket
            // #2341 measured that dropping this store kills no test -- but a half-true
            // invariant is what made the original bug hard to see.
            state->owner.store(std::thread::id(), std::memory_order_relaxed);
            state->depth.store(0, std::memory_order_relaxed);

            bool ok;
            if (millisecondsTimeout == -1) {
                state->cv.wait(lock);
                ok = true;
            } else {
                ok = state->cv.wait_for(lock, std::chrono::milliseconds(millisecondsTimeout)) == std::cv_status::no_timeout;
            }

            // No exception path guards the restoration below, deliberately.  Both
            // condition_variable_any::wait and ::wait_for reacquire the caller's lock from a
            // destructor whose postcondition is "lock is held", so a failure to reacquire
            // calls std::terminate rather than unwinding through here; there is no reachable
            // route on which this function returns or propagates without holding one level.
            for (int i = 0; i < extra; ++i) state->mutex.lock();
            state->owner.store(std::this_thread::get_id(), std::memory_order_relaxed);
            state->depth.store(depth, std::memory_order_relaxed);
            lock.release();
            return ok;
        }

    public:
        /** Prevents instantiation — all members are static. */
        Monitor() = delete;

        /** Acquires an exclusive lock on obj, blocking until it is available. Re-entrant on the same thread. */
        static void Enter(const void* obj) { auto s = GetOrCreate(obj); s->mutex.lock(); s->onAcquired(); }
        /** Acquires a lock on obj and sets lockTaken to true once acquired. */
        static void Enter(const void* obj, bool& lockTaken) {
            auto s = GetOrCreate(obj);
            s->mutex.lock();
            s->onAcquired();
            lockTaken = true;
        }

        /**
         * @brief Releases one level of the exclusive lock on obj.
         * @throws System::Threading::SynchronizationLockException if the calling thread does not hold the lock.
         */
        static void Exit(const void* obj) {
            auto s = GetOrCreate(obj);
            if (!s->heldByCurrentThread()) throw System::Threading::SynchronizationLockException();
            s->onReleasing();
            s->mutex.unlock();
        }

        /** Attempts to acquire an exclusive lock on obj without blocking; returns true on success. */
        static bool TryEnter(const void* obj) {
            auto s = GetOrCreate(obj);
            bool ok = s->mutex.try_lock();
            if (ok) s->onAcquired();
            return ok;
        }
        /** Attempts to acquire a lock on obj without blocking; sets lockTaken and returns the same value. */
        static void TryEnter(const void* obj, bool& lockTaken) { lockTaken = TryEnter(obj); }
        /**
         * @brief Attempts to acquire an exclusive lock on obj within the given timeout; returns true on success.
         *
         * Verified against Lock.cs's doc comment: -1 (Timeout.Infinite) waits indefinitely,
         * matching every other timed-wait method in this codebase. std::chrono's own
         * try_lock_for treats a negative duration as already-expired, so -1 must be
         * special-cased to an untimed lock() rather than passed straight through.
         */
        static bool TryEnter(const void* obj, intcs millisecondsTimeout) {
            auto s = GetOrCreate(obj);
            bool ok;
            if (millisecondsTimeout == -1) {
                s->mutex.lock();
                ok = true;
            } else {
                ok = s->mutex.try_lock_for(std::chrono::milliseconds(millisecondsTimeout));
            }
            if (ok) s->onAcquired();
            return ok;
        }

        /**
         * @brief Releases the lock on obj and blocks until it is reacquired via Pulse/PulseAll.
         * @note Every recursion level the calling thread holds is released for the duration of
         * the wait and restored before this returns, matching .NET Monitor.Wait. Ticket #2341 /
         * SR-AUD-202: this previously released a single level, so a caller at depth >= 2 kept the
         * mutex locked while it waited and the signalling thread could never Enter/Pulse.
         * @throws System::Threading::SynchronizationLockException if the calling thread does not hold the lock.
         */
        static bool Wait(const void* obj) { return WaitCore(obj, -1); }

        /**
         * @brief Releases the lock on obj and blocks until reacquired or millisecondsTimeout elapses.
         * @note Releases and restores the full recursion depth, exactly as the untimed overload does.
         * @throws System::Threading::SynchronizationLockException if the calling thread does not hold the lock.
         */
        static bool Wait(const void* obj, intcs millisecondsTimeout) { return WaitCore(obj, millisecondsTimeout); }

        /**
         * @brief Notifies a thread in the waiting queue of a change in the locked object's state.
         * @throws System::Threading::SynchronizationLockException if the calling thread does not hold the lock.
         */
        static void Pulse(const void* obj) {
            auto s = GetOrCreate(obj);
            if (!s->heldByCurrentThread()) throw System::Threading::SynchronizationLockException();
            s->cv.notify_one();
        }
        /**
         * @brief Notifies all waiting threads of a change in the locked object's state.
         * @throws System::Threading::SynchronizationLockException if the calling thread does not hold the lock.
         */
        static void PulseAll(const void* obj) {
            auto s = GetOrCreate(obj);
            if (!s->heldByCurrentThread()) throw System::Threading::SynchronizationLockException();
            s->cv.notify_all();
        }
    };

} // namespace System::Threading
