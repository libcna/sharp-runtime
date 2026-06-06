// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
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
     * @note Status: Partial
     */
    class Thread {
        std::thread thread_;
        std::string name_;
        bool isBackground_ = false;
        bool started_ = false;
    public:
        explicit Thread(std::function<void()> start) : thread_() {
            thread_ = std::thread([fn = std::move(start)]() { fn(); });
            started_ = true;
        }

        ~Thread() {
            if (thread_.joinable()) thread_.detach();
        }

        Thread(const Thread&) = delete;
        Thread& operator=(const Thread&) = delete;

        void Join() { if (thread_.joinable()) thread_.join(); }

        [[nodiscard]] bool getIsAliveProperty() const { return started_ && thread_.joinable(); }
        [[nodiscard]] bool getIsBackgroundProperty() const { return isBackground_; }
        void setIsBackgroundProperty(bool v) { isBackground_ = v; }
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }
        void setNameProperty(const std::string& name) { name_ = name; }

        static void Sleep(intcs milliseconds) {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }

        [[nodiscard]] static intcs getCurrentThreadManagedThreadIdProperty() {
            return static_cast<intcs>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        }
    };

} // namespace System::Threading
