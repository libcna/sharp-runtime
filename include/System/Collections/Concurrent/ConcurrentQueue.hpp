// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Concurrent {

    using SharpRuntime::intcs;

    /**
     * @brief A thread-safe first-in, first-out (FIFO) collection.
     *
     * Wraps std::queue with std::mutex.
     * Partial C++ counterpart of .NET System.Collections.Concurrent.ConcurrentQueue<T>.
     *
     * @note Status: Partial
     */
    template<typename T>
    class ConcurrentQueue {
        mutable std::mutex mutex_;
        std::queue<T>      queue_;
    public:
        /// Default-constructs an empty ConcurrentQueue.
        ConcurrentQueue() = default;

        /// Thread-safely adds item to the back of the queue.
        void Enqueue(const T& item) {
            std::lock_guard<std::mutex> lk(mutex_);
            queue_.push(item);
        }

        /// Thread-safely removes and returns the front element; returns false if empty.
        bool TryDequeue(T& result) {
            std::lock_guard<std::mutex> lk(mutex_);
            if (queue_.empty()) return false;
            result = queue_.front();
            queue_.pop();
            return true;
        }

        /// Thread-safely returns the front element without removing it; returns false if empty.
        bool TryPeek(T& result) const {
            std::lock_guard<std::mutex> lk(mutex_);
            if (queue_.empty()) return false;
            result = queue_.front();
            return true;
        }

        /// Returns true if the queue contains no elements (thread-safe).
        [[nodiscard]] bool getIsEmptyProperty() const {
            std::lock_guard<std::mutex> lk(mutex_);
            return queue_.empty();
        }

        /// Gets the number of elements in the queue (thread-safe).
        [[nodiscard]] intcs getCountProperty() const {
            std::lock_guard<std::mutex> lk(mutex_);
            return static_cast<intcs>(queue_.size());
        }

        /// Thread-safely removes all elements from the queue.
        void Clear() {
            std::lock_guard<std::mutex> lk(mutex_);
            while (!queue_.empty()) queue_.pop();
        }
    };

} // namespace System::Collections::Concurrent
