// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

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

    public:
        /** Prevents instantiation — all members are static. */
        Monitor() = delete;

        /** Acquires an exclusive lock on obj, blocking until it is available. Re-entrant on the same thread. */
        static void Enter(const void* obj) { GetOrCreate(obj)->mutex.lock(); }
        /** Acquires a lock on obj and sets lockTaken to true once acquired. */
        static void Enter(const void* obj, bool& lockTaken) { GetOrCreate(obj)->mutex.lock(); lockTaken = true; }

        /** Releases one level of the exclusive lock on obj. */
        static void Exit(const void* obj) { GetOrCreate(obj)->mutex.unlock(); }

        /** Attempts to acquire an exclusive lock on obj without blocking; returns true on success. */
        static bool TryEnter(const void* obj) { return GetOrCreate(obj)->mutex.try_lock(); }
        /** Attempts to acquire a lock on obj without blocking; sets lockTaken and returns the same value. */
        static void TryEnter(const void* obj, bool& lockTaken) { lockTaken = GetOrCreate(obj)->mutex.try_lock(); }
        /** Attempts to acquire an exclusive lock on obj within the given timeout; returns true on success. */
        static bool TryEnter(const void* obj, intcs millisecondsTimeout) {
            return GetOrCreate(obj)->mutex.try_lock_for(std::chrono::milliseconds(millisecondsTimeout));
        }

        /**
         * @brief Releases the lock on obj and blocks until it is reacquired via Pulse/PulseAll.
         * @note Assumes the calling thread holds the lock at recursion depth 1 (the common case for
         * lock(obj){ Monitor.Wait(obj); } patterns); deeper recursion is not unwound and reacquired.
         */
        static bool Wait(const void* obj) {
            auto state = GetOrCreate(obj);
            std::unique_lock<std::recursive_timed_mutex> lock(state->mutex, std::adopt_lock);
            state->cv.wait(lock);
            lock.release();
            return true;
        }

        /** Releases the lock on obj and blocks until reacquired or millisecondsTimeout elapses. */
        static bool Wait(const void* obj, intcs millisecondsTimeout) {
            auto state = GetOrCreate(obj);
            std::unique_lock<std::recursive_timed_mutex> lock(state->mutex, std::adopt_lock);
            bool ok = state->cv.wait_for(lock, std::chrono::milliseconds(millisecondsTimeout)) == std::cv_status::no_timeout;
            lock.release();
            return ok;
        }

        /** Notifies a thread in the waiting queue of a change in the locked object's state. */
        static void Pulse(const void* obj) { GetOrCreate(obj)->cv.notify_one(); }
        /** Notifies all waiting threads of a change in the locked object's state. */
        static void PulseAll(const void* obj) { GetOrCreate(obj)->cv.notify_all(); }
    };

} // namespace System::Threading
