// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Threading/ApartmentState.hpp"
#include "System/Threading/ThreadPriority.hpp"
#include "System/Threading/ThreadState.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /**
     * @brief Creates and controls a thread, sets its priority, and gets its status.
     *
     * C++ counterpart of .NET System.Threading.Thread.
     * Wraps std::thread. The thread does NOT start at construction — call Start()
     * exactly once. Calling Start() a second time throws std::invalid_argument.
     *
     * Obsolete .NET APIs (Abort, Suspend, Resume, VolatileRead/Write) are omitted.
     * Apartment-state and compressed-stack APIs are stubs.
     */
    class Thread {
        inline static std::atomic<intcs> nextManagedId_{1};

        std::function<void()>  fn_;
        std::thread            thread_;
        std::string            name_;
        intcs                  managedThreadId_;
        bool                   isBackground_      = false;
        bool                   isThreadPoolThread_ = false;
        ThreadPriority         priority_          = ThreadPriority::Normal;
        std::atomic<bool>      started_{false};
        std::atomic<bool>      finished_{false};

    public:
        /**
         * @brief Constructs a Thread with the given parameterless start function.
         * @param start Function to execute on the new thread.
         */
        explicit Thread(std::function<void()> start)
            : fn_(std::move(start)), managedThreadId_(nextManagedId_.fetch_add(1))
        {}

        ~Thread() {
            if (thread_.joinable()) thread_.detach();
        }

        Thread(const Thread&)            = delete;
        Thread& operator=(const Thread&) = delete;

        // -----------------------------------------------------------------------
        // Control
        // -----------------------------------------------------------------------

        /**
         * @brief Starts the thread.
         * @throws std::invalid_argument if Start() has already been called.
         */
        void Start() {
            if (started_.exchange(true))
                throw std::invalid_argument("Thread already started");
            thread_ = std::thread([this, fn = std::move(fn_)]() mutable {
                fn();
                finished_.store(true);
            });
        }

        /**
         * @brief Starts the thread, passing @p parameter to a ParameterizedThreadStart function.
         * @param parameter Argument forwarded to the thread function (stored as void*).
         */
        void Start(void* parameter) {
            if (started_.exchange(true))
                throw std::invalid_argument("Thread already started");
            thread_ = std::thread([this, fn = std::move(fn_), parameter]() mutable {
                (void)parameter;
                fn();
                finished_.store(true);
            });
        }

        /**
         * @brief Blocks the calling thread until this thread terminates.
         */
        void Join() { if (thread_.joinable()) thread_.join(); }

        /**
         * @brief Blocks the calling thread until this thread terminates or
         * @p millisecondsTimeout elapses.
         * @param millisecondsTimeout Maximum wait time in milliseconds.
         * @return true if the thread terminated; false if the timeout elapsed.
         */
        bool Join(int millisecondsTimeout) {
            if (!thread_.joinable()) return true;
            auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(millisecondsTimeout);
            while (!finished_.load()) {
                if (std::chrono::steady_clock::now() >= deadline) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (thread_.joinable()) thread_.join();
            return true;
        }

        /** @brief No-op stub — Interrupt() is not supported in this port. */
        void Interrupt() noexcept {}

        // -----------------------------------------------------------------------
        // Properties
        // -----------------------------------------------------------------------

        /**
         * @brief Returns true while the OS thread is live (started and not yet joined).
         * @return true if the thread is running.
         */
        [[nodiscard]] bool getIsAliveProperty() const { return thread_.joinable() && !finished_.load(); }

        /**
         * @brief Returns the unique managed thread ID assigned at construction.
         * @return Managed thread ID.
         */
        [[nodiscard]] intcs getManagedThreadIdProperty() const { return managedThreadId_; }

        /** @brief Returns true if this is a background thread. */
        [[nodiscard]] bool getIsBackgroundProperty() const { return isBackground_; }
        /** @brief Sets the background status of this thread. */
        void setIsBackgroundProperty(bool v) { isBackground_ = v; }

        /** @brief Returns true if this thread was created by the thread pool. Always false for user-created threads. */
        [[nodiscard]] bool getIsThreadPoolThreadProperty() const { return isThreadPoolThread_; }

        /** @brief Returns the name of this thread. */
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }
        /** @brief Sets the name of this thread. */
        void setNameProperty(const std::string& name) { name_ = name; }

        /** @brief Returns the scheduling priority of this thread. */
        [[nodiscard]] ThreadPriority getPriorityProperty() const { return priority_; }
        /** @brief Sets the scheduling priority (stored but not applied to the OS thread). */
        void setPriorityProperty(ThreadPriority p) { priority_ = p; }

        /**
         * @brief Returns the current execution state of this thread.
         * @return ThreadState reflecting started/running/stopped flags.
         */
        [[nodiscard]] ThreadState getThreadStateProperty() const {
            if (!started_.load())          return ThreadState::Unstarted;
            if (finished_.load())          return ThreadState::Stopped;
            if (!thread_.joinable())       return ThreadState::Stopped;
            return isBackground_ ? (ThreadState::Running | ThreadState::Background)
                                 : ThreadState::Running;
        }

        // -----------------------------------------------------------------------
        // Apartment state (stub — COM apartments are not meaningful in C++)
        // -----------------------------------------------------------------------

        /** @brief Returns ApartmentState::Unknown (COM apartments not supported in C++). */
        [[nodiscard]] ApartmentState GetApartmentState() const noexcept {
            return ApartmentState::Unknown;
        }
        /** @brief No-op stub — COM apartment state cannot be set. */
        void SetApartmentState(ApartmentState) noexcept {}
        /** @brief Always returns false — COM apartment state cannot be set. */
        [[nodiscard]] bool TrySetApartmentState(ApartmentState) noexcept { return false; }

        // -----------------------------------------------------------------------
        // Static helpers
        // -----------------------------------------------------------------------

        /**
         * @brief Suspends the current thread for @p milliseconds.
         * @param milliseconds Duration in milliseconds (0 yields the scheduler).
         */
        static void Sleep(intcs milliseconds) {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }

        /**
         * @brief Causes the operating system to change the state of the current
         * instance to WaitSleepJoin for the specified duration.
         * @param timeout Time span specifying the sleep duration.
         */
        static void Sleep(std::chrono::milliseconds timeout) {
            std::this_thread::sleep_for(timeout);
        }

        /**
         * @brief Spins for @p iterations tight-loop iterations.
         * @param iterations Number of spin iterations.
         */
        static void SpinWait(int iterations) {
            for (int i = 0; i < iterations; ++i)
                std::atomic_signal_fence(std::memory_order_seq_cst);
        }

        /** @brief Issues a full memory fence (sequentially consistent). */
        static void MemoryBarrier() {
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }

        /**
         * @brief Causes the current thread to yield execution to another thread.
         * @return true if the operating system switched to another thread.
         */
        static bool Yield() {
            std::this_thread::yield();
            return true;
        }

        /**
         * @brief Returns the ID of the processor on which the current thread is running.
         * @return Processor ID (best-effort; may be stale immediately).
         */
        [[nodiscard]] static int GetCurrentProcessorId() {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
            return static_cast<int>(sched_getcpu());
#else
            return 0;
#endif
        }

        // -----------------------------------------------------------------------
        // CurrentThread proxy
        // -----------------------------------------------------------------------

        /**
         * @brief Lightweight proxy representing the calling thread.
         *
         * Returned by Thread::CurrentThread(). Provides read-only access to
         * properties of the calling thread without owning its lifetime.
         */
        struct CurrentThreadProxy {
            /** @brief Returns the managed thread ID of the calling thread. */
            [[nodiscard]] intcs getManagedThreadIdProperty() const {
                return static_cast<intcs>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
            }
            /** @brief Always returns false — the main thread is not a background thread. */
            [[nodiscard]] bool getIsBackgroundProperty() const { return false; }
            /** @brief Suspends the calling thread for @p ms milliseconds. */
            static void Sleep(intcs ms) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
        };

        /**
         * @brief Returns a proxy for the calling thread.
         * @return CurrentThreadProxy for the thread calling this method.
         */
        static CurrentThreadProxy CurrentThread() { return CurrentThreadProxy{}; }
    };

} // namespace System::Threading
