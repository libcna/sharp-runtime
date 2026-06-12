// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /**
     * @brief Creates and controls a thread, sets its priority, and gets its status.
     *
     * Wraps std::thread. Partial C++ counterpart of .NET System.Threading.Thread.
     *
     * @note The thread does NOT start at construction. Call Start() exactly once.
     *       Calling Start() a second time throws std::invalid_argument.
     *
     * @note Status: Partial — no Priority, no Abort/Interrupt.
     */
    class Thread {
        inline static std::atomic<intcs> nextManagedId_{1};

        std::function<void()> fn_;
        std::thread           thread_;
        std::string           name_;
        intcs                 managedThreadId_;
        bool                  isBackground_ = false;
        std::atomic<bool>     started_{false};

    public:
        explicit Thread(std::function<void()> start)
            : fn_(std::move(start)), managedThreadId_(nextManagedId_.fetch_add(1))
        {}

        ~Thread() {
            if (thread_.joinable()) thread_.detach();
        }

        Thread(const Thread&)            = delete;
        Thread& operator=(const Thread&) = delete;

        /// @brief Starts the thread. Throws std::invalid_argument if called more than once.
        void Start() {
            if (started_.exchange(true))
                throw std::invalid_argument("Thread already started");
            thread_ = std::thread([fn = std::move(fn_)]() { fn(); });
        }

        /// @brief Blocks until the thread completes.
        void Join() { if (thread_.joinable()) thread_.join(); }

        /// @brief Returns true while the OS thread is live (started and not yet joined/detached).
        [[nodiscard]] bool  getIsAliveProperty()         const { return thread_.joinable(); }
        /// @brief Returns the unique managed thread ID assigned at construction.
        [[nodiscard]] intcs getManagedThreadIdProperty()  const { return managedThreadId_; }
        [[nodiscard]] bool  getIsBackgroundProperty()    const { return isBackground_; }
        void setIsBackgroundProperty(bool v) { isBackground_ = v; }
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }
        void setNameProperty(const std::string& name) { name_ = name; }

        /// @brief Suspends the current thread for the given number of milliseconds.
        static void Sleep(intcs milliseconds) {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }

        /// @brief Returns the managed thread ID of the calling thread.
        [[nodiscard]] static intcs getCurrentThreadManagedThreadIdProperty() {
            return static_cast<intcs>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        }

        /// @brief Lightweight proxy returned by CurrentThread().
        struct CurrentThreadProxy {
            [[nodiscard]] intcs getManagedThreadIdProperty() const {
                return static_cast<intcs>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
            }
            [[nodiscard]] bool getIsBackgroundProperty() const { return false; }
            static void Sleep(intcs ms) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
        };

        /// @brief Returns a proxy for the calling thread.
        static CurrentThreadProxy CurrentThread() { return CurrentThreadProxy{}; }
    };

} // namespace System::Threading
