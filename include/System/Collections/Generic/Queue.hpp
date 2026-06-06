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
        Queue() = default;

        /**
         * @brief Adds an object to the end of the Queue.
         */
        void Enqueue(const T& item) { queue_.push(item); }
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

        [[nodiscard]] int getCountProperty() const { return static_cast<int>(queue_.size()); }

        [[nodiscard]] bool Contains(const T& item) const {
            std::queue<T> copy = queue_;
            while (!copy.empty()) {
                if (copy.front() == item) return true;
                copy.pop();
            }
            return false;
        }

        void Clear() { while (!queue_.empty()) queue_.pop(); }

        [[nodiscard]] std::vector<T> ToArray() const {
            std::vector<T> result;
            std::queue<T> copy = queue_;
            while (!copy.empty()) {
                result.push_back(copy.front());
                copy.pop();
            }
            return result;
        }
    };

} // namespace System::Collections::Generic
