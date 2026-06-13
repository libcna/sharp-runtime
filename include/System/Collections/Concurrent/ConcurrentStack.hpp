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
        /// Default-constructs an empty ConcurrentStack.
        ConcurrentStack() = default;

        /// Thread-safely pushes item onto the top of the stack.
        void Push(const T& item) {
            std::lock_guard<std::mutex> lk(mutex_);
            stack_.push(item);
        }

        /// Thread-safely removes and returns the top element; returns false if empty.
        bool TryPop(T& result) {
            std::lock_guard<std::mutex> lk(mutex_);
            if (stack_.empty()) return false;
            result = stack_.top();
            stack_.pop();
            return true;
        }

        /// Thread-safely returns the top element without removing it; returns false if empty.
        bool TryPeek(T& result) const {
            std::lock_guard<std::mutex> lk(mutex_);
            if (stack_.empty()) return false;
            result = stack_.top();
            return true;
        }

        /// Thread-safely pops up to count elements into items; returns the number actually popped.
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

        /// Returns true if the stack contains no elements (thread-safe).
        [[nodiscard]] bool getIsEmptyProperty() const {
            std::lock_guard<std::mutex> lk(mutex_);
            return stack_.empty();
        }

        /// Gets the number of elements in the stack (thread-safe).
        [[nodiscard]] intcs getCountProperty() const {
            std::lock_guard<std::mutex> lk(mutex_);
            return static_cast<intcs>(stack_.size());
        }

        /// Thread-safely removes all elements from the stack.
        void Clear() {
            std::lock_guard<std::mutex> lk(mutex_);
            while (!stack_.empty()) stack_.pop();
        }
    };

} // namespace System::Collections::Concurrent
