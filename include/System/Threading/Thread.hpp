// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <functional>
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
     * @note In this implementation the thread starts immediately at construction.
     *       Start() is provided for .NET API compatibility but is a no-op.
     *
     * @note Status: Partial — no Priority, no Abort/Interrupt.
     */
    class Thread {
        inline static std::atomic<intcs> nextManagedId_{1};

        std::thread thread_;
        std::string name_;
        intcs       managedThreadId_;
        bool        isBackground_ = false;
        bool        started_      = false;

    public:
        explicit Thread(std::function<void()> start)
            : thread_(), managedThreadId_(nextManagedId_.fetch_add(1))
        {
            thread_ = std::thread([fn = std::move(start)]() { fn(); });
            started_ = true;
        }

        ~Thread() {
            if (thread_.joinable()) thread_.detach();
        }

        Thread(const Thread&)            = delete;
        Thread& operator=(const Thread&) = delete;

        /// @brief No-op: the thread already starts in the constructor.
        ///        Present for .NET API compatibility — ported code calls Start() after construction.
        void Start() {}

        /// @brief Blocks until the thread completes.
        void Join() { if (thread_.joinable()) thread_.join(); }

        /// @brief Returns true while the thread is running (not yet joined/detached).
        [[nodiscard]] bool  getIsAliveProperty()        const { return started_ && thread_.joinable(); }
        /// @brief Returns the unique managed thread ID assigned at construction.
        [[nodiscard]] intcs getManagedThreadIdProperty() const { return managedThreadId_; }
        [[nodiscard]] bool  getIsBackgroundProperty()   const { return isBackground_; }
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
