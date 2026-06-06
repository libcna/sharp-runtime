// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>
#include <stack>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Concurrent {

    using SharpRuntime::intcs;

    /**
     * @brief A thread-safe last-in, first-out (LIFO) collection.
     *
     * Wraps std::stack with std::mutex.
     * Partial C++ counterpart of .NET System.Collections.Concurrent.ConcurrentStack<T>.
     *
     * @note Status: Partial
     */
    template<typename T>
    class ConcurrentStack {
        mutable std::mutex mutex_;
        std::stack<T>      stack_;
    public:
        ConcurrentStack() = default;

        void Push(const T& item) {
            std::lock_guard<std::mutex> lk(mutex_);
            stack_.push(item);
        }

        bool TryPop(T& result) {
            std::lock_guard<std::mutex> lk(mutex_);
            if (stack_.empty()) return false;
            result = stack_.top();
            stack_.pop();
            return true;
        }

        bool TryPeek(T& result) const {
            std::lock_guard<std::mutex> lk(mutex_);
            if (stack_.empty()) return false;
            result = stack_.top();
            return true;
        }

        intcs TryPopRange(std::vector<T>& items, intcs count) {
            std::lock_guard<std::mutex> lk(mutex_);
            intcs popped = 0;
            while (!stack_.empty() && popped < count) {
                items.push_back(stack_.top());
                stack_.pop();
                ++popped;
            }
            return popped;
        }

        [[nodiscard]] bool getIsEmptyProperty() const {
            std::lock_guard<std::mutex> lk(mutex_);
            return stack_.empty();
        }

        [[nodiscard]] intcs getCountProperty() const {
            std::lock_guard<std::mutex> lk(mutex_);
            return static_cast<intcs>(stack_.size());
        }

        void Clear() {
            std::lock_guard<std::mutex> lk(mutex_);
            while (!stack_.empty()) stack_.pop();
        }
    };

} // namespace System::Collections::Concurrent
