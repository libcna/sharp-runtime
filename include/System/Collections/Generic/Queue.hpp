// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <queue>
#include <stdexcept>
#include <vector>

namespace System::Collections::Generic {

    /**
     * @brief Represents a first-in, first-out (FIFO) collection of objects.
     *
     * @tparam T The type of elements in the queue.
     *
     * @note Status: Implemented
     */
    template<typename T>
    class Queue {
    private:
        std::queue<T> queue_;

    public:
        /** Default-constructs an empty Queue. */
        Queue() = default;

        /**
         * @brief Adds an object to the end of the Queue.
         */
        /** Adds an object to the end of the Queue (copy). */
        void Enqueue(const T& item) { queue_.push(item); }
        /** Adds an object to the end of the Queue (move). */
        void Enqueue(T&& item)      { queue_.push(std::move(item)); }

        /**
         * @brief Removes and returns the object at the beginning of the Queue.
         */
        T Dequeue() {
            if (queue_.empty()) throw std::runtime_error("Queue is empty.");
            T val = std::move(queue_.front());
            queue_.pop();
            return val;
        }

        /**
         * @brief Returns the object at the beginning of the Queue without removing it.
         */
        [[nodiscard]] const T& Peek() const {
            if (queue_.empty()) throw std::runtime_error("Queue is empty.");
            return queue_.front();
        }

        /** Gets the number of elements contained in the Queue. */
        [[nodiscard]] int getCountProperty() const { return static_cast<int>(queue_.size()); }

        /** Returns true if the Queue contains the specified element. */
        [[nodiscard]] bool Contains(const T& item) const {
            std::queue<T> copy = queue_;
            while (!copy.empty()) {
                if (copy.front() == item) return true;
                copy.pop();
            }
            return false;
        }

        /** Removes all objects from the Queue. */
        void Clear() { while (!queue_.empty()) queue_.pop(); }

        /** Copies the Queue elements to a new vector. */
        [[nodiscard]] std::vector<T> ToArray() const {
            std::vector<T> result;
            std::queue<T> copy = queue_;
            while (!copy.empty()) {
                result.push_back(copy.front());
                copy.pop();
            }
            return result;
        }

        /** Tries to remove and return the object at the beginning; returns false if empty. */
        bool TryDequeue(T& result) {
            if (queue_.empty()) return false;
            result = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        /** Tries to return the object at the beginning without removing it; returns false if empty. */
        bool TryPeek(T& result) const {
            if (queue_.empty()) return false;
            result = queue_.front();
            return true;
        }
    };

} // namespace System::Collections::Generic
