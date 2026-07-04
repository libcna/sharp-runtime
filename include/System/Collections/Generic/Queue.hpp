// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <queue>
#include <stdexcept>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/InvalidOperationException.hpp"

namespace System::Collections::Generic {

using SharpRuntime::intcs;

/**
 * @brief Represents a first-in, first-out (FIFO) collection of objects.
 *
 * C++ counterpart of .NET System.Collections.Generic.Queue<T>.
 * Backed by std::queue<T>; provides O(1) amortized Enqueue and Dequeue.
 *
 * @tparam T The type of elements in the queue.
 */
template<typename T>
class Queue {
    std::queue<T> queue_;

public:
    /** @brief Initializes a new empty Queue<T>. */
    Queue() = default;

    /**
     * @brief Gets the number of elements contained in the Queue.
     *
     * C++ counterpart of .NET Queue<T>.Count.
     * @return The number of elements.
     */
    [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(queue_.size()); }

    /**
     * @brief Adds an object to the end of the Queue (copy).
     *
     * C++ counterpart of .NET Queue<T>.Enqueue(T).
     * @param item The object to add.
     */
    void Enqueue(const T& item) { queue_.push(item); }

    /**
     * @brief Adds an object to the end of the Queue (move).
     *
     * C++ counterpart of .NET Queue<T>.Enqueue(T).
     * @param item The object to add (moved).
     */
    void Enqueue(T&& item) { queue_.push(std::move(item)); }

    /**
     * @brief Removes and returns the object at the beginning of the Queue.
     *
     * C++ counterpart of .NET Queue<T>.Dequeue().
     * @return The object that was removed from the beginning of the Queue.
     * @throws System::InvalidOperationException if the Queue is empty.
     */
    T Dequeue() {
        if (queue_.empty()) throw System::InvalidOperationException("Queue empty.");
        T val = std::move(queue_.front());
        queue_.pop();
        return val;
    }

    /**
     * @brief Returns the object at the beginning of the Queue without removing it.
     *
     * C++ counterpart of .NET Queue<T>.Peek().
     * @return A const reference to the object at the front.
     * @throws System::InvalidOperationException if the Queue is empty.
     */
    [[nodiscard]] const T& Peek() const {
        if (queue_.empty()) throw System::InvalidOperationException("Queue empty.");
        return queue_.front();
    }

    /**
     * @brief Tries to remove and return the object at the beginning of the Queue.
     *
     * C++ counterpart of .NET Queue<T>.TryDequeue(out T).
     * @param result Receives the dequeued object if successful.
     * @return true if an element was removed; false if the Queue was empty.
     */
    bool TryDequeue(T& result) {
        if (queue_.empty()) return false;
        result = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    /**
     * @brief Tries to return the object at the beginning of the Queue without removing it.
     *
     * C++ counterpart of .NET Queue<T>.TryPeek(out T).
     * @param result Receives the front object if successful.
     * @return true if the Queue is non-empty; otherwise false.
     */
    bool TryPeek(T& result) const {
        if (queue_.empty()) return false;
        result = queue_.front();
        return true;
    }

    /**
     * @brief Determines whether the Queue contains the specified element.
     *
     * C++ counterpart of .NET Queue<T>.Contains(T).
     * @param item The object to locate.
     * @return true if the element is found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const {
        std::queue<T> copy = queue_;
        while (!copy.empty()) {
            if (copy.front() == item) return true;
            copy.pop();
        }
        return false;
    }

    /**
     * @brief Removes all objects from the Queue.
     *
     * C++ counterpart of .NET Queue<T>.Clear().
     */
    void Clear() { while (!queue_.empty()) queue_.pop(); }

    /**
     * @brief Copies the Queue elements to a new vector in FIFO order.
     *
     * C++ counterpart of .NET Queue<T>.ToArray().
     * @return A std::vector<T> containing all elements in queue order.
     */
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
