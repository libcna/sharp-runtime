// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>
#include <vector>
#include <stdexcept>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Concurrent {

using SharpRuntime::intcs;

/**
 * @brief Represents a thread-safe last-in, first-out (LIFO) collection.
 *
 * C++ counterpart of .NET System.Collections.Concurrent.ConcurrentStack<T>.
 * All public members are thread-safe and may be used concurrently from multiple threads.
 * Backed by std::vector with push/pop at the back for O(1) amortised LIFO operations.
 *
 * @tparam T Specifies the type of elements in the stack.
 */
template<typename T>
class ConcurrentStack {
    mutable std::mutex mutex_;
    std::vector<T>    data_;

public:
    /**
     * @brief Initializes a new instance of ConcurrentStack that is empty.
     *
     * C++ counterpart of .NET ConcurrentStack<T>().
     */
    ConcurrentStack() = default;

    /**
     * @brief Initializes a new instance with elements copied from the specified collection.
     *
     * C++ counterpart of .NET ConcurrentStack<T>(IEnumerable<T>).
     * Elements are pushed in iteration order so the last element ends up on top.
     * @param collection The collection whose elements are copied to the stack.
     */
    explicit ConcurrentStack(const std::vector<T>& collection) : data_(collection) {}

    /**
     * @brief Gets a value indicating whether the stack is empty.
     *
     * C++ counterpart of .NET ConcurrentStack<T>.IsEmpty.
     */
    [[nodiscard]] bool getIsEmptyProperty() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return data_.empty();
    }

    /**
     * @brief Gets the number of elements in the stack.
     *
     * C++ counterpart of .NET ConcurrentStack<T>.Count.
     */
    [[nodiscard]] intcs getCountProperty() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return static_cast<intcs>(data_.size());
    }

    /**
     * @brief Removes all elements from the stack.
     *
     * C++ counterpart of .NET ConcurrentStack<T>.Clear().
     */
    void Clear() {
        std::lock_guard<std::mutex> lk(mutex_);
        data_.clear();
    }

    /**
     * @brief Inserts an object at the top of the stack.
     *
     * C++ counterpart of .NET ConcurrentStack<T>.Push(T).
     * @param item The object to push onto the stack.
     */
    void Push(const T& item) {
        std::lock_guard<std::mutex> lk(mutex_);
        data_.push_back(item);
    }

    /**
     * @brief Inserts multiple objects at the top of the stack atomically.
     *
     * C++ counterpart of .NET ConcurrentStack<T>.PushRange(T[]).
     * Elements are pushed in order so items.back() ends up on top.
     * @param items The objects to push onto the stack.
     */
    void PushRange(const std::vector<T>& items) {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& item : items) {
            data_.push_back(item);
        }
    }

    /**
     * @brief Inserts a sub-range of elements at the top of the stack atomically.
     *
     * C++ counterpart of .NET ConcurrentStack<T>.PushRange(T[], int, int).
     * @param items      Source array of items.
     * @param startIndex Zero-based starting index in items.
     * @param count      Number of elements to push.
     * @throws std::out_of_range if startIndex or count is out of range.
     */
    void PushRange(const std::vector<T>& items, intcs startIndex, intcs count) {
        if (startIndex < 0 || count < 0 ||
            startIndex + count > static_cast<intcs>(items.size())) {
            throw std::out_of_range("startIndex or count out of range");
        }
        std::lock_guard<std::mutex> lk(mutex_);
        for (intcs i = startIndex; i < startIndex + count; ++i) {
            data_.push_back(items[static_cast<std::size_t>(i)]);
        }
    }

    /**
     * @brief Attempts to pop and return the object at the top of the stack.
     *
     * C++ counterpart of .NET ConcurrentStack<T>.TryPop(out T).
     * @param result Receives the removed object if successful.
     * @return true if an element was removed; false if the stack is empty.
     */
    bool TryPop(T& result) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (data_.empty()) return false;
        result = std::move(data_.back());
        data_.pop_back();
        return true;
    }

    /**
     * @brief Attempts to return the object at the top of the stack without removing it.
     *
     * C++ counterpart of .NET ConcurrentStack<T>.TryPeek(out T).
     * @param result Receives the top element if successful.
     * @return true if the stack is not empty; otherwise false.
     */
    bool TryPeek(T& result) const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (data_.empty()) return false;
        result = data_.back();
        return true;
    }

    /**
     * @brief Attempts to pop and return multiple objects from the top of the stack.
     *
     * C++ counterpart of .NET ConcurrentStack<T>.TryPopRange(T[]).
     * Elements are stored in pop order: items[0] is the topmost popped element.
     * @param items Destination vector; elements are appended.
     * @param count Maximum number of elements to pop.
     * @return The number of elements actually popped.
     */
    intcs TryPopRange(std::vector<T>& items, intcs count) {
        std::lock_guard<std::mutex> lk(mutex_);
        intcs popped = 0;
        while (!data_.empty() && popped < count) {
            items.push_back(std::move(data_.back()));
            data_.pop_back();
            ++popped;
        }
        return popped;
    }

    /**
     * @brief Attempts to pop and return up to count objects into a subrange of a vector.
     *
     * C++ counterpart of .NET ConcurrentStack<T>.TryPopRange(T[], int, int).
     * @param items      Destination vector (must be large enough: startIndex + count).
     * @param startIndex Zero-based index in items at which to begin storing popped elements.
     * @param count      Maximum number of elements to pop.
     * @return The number of elements actually popped.
     */
    intcs TryPopRange(std::vector<T>& items, intcs startIndex, intcs count) {
        if (startIndex < 0 || count < 0) throw std::out_of_range("invalid startIndex or count");
        std::lock_guard<std::mutex> lk(mutex_);
        intcs popped = 0;
        while (!data_.empty() && popped < count) {
            items[static_cast<std::size_t>(startIndex + popped)] = std::move(data_.back());
            data_.pop_back();
            ++popped;
        }
        return popped;
    }

    /**
     * @brief Copies the elements to a new vector in top-to-bottom order.
     *
     * C++ counterpart of .NET ConcurrentStack<T>.ToArray().
     * @return A new vector with the top element at index 0.
     */
    [[nodiscard]] std::vector<T> ToArray() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<T> result(data_.rbegin(), data_.rend());
        return result;
    }

    /**
     * @brief Copies elements to the destination vector starting at index, in top-to-bottom order.
     *
     * C++ counterpart of .NET ConcurrentStack<T>.CopyTo(T[], int).
     * @param array Destination vector (must have capacity for index + Count elements).
     * @param index Zero-based starting index in array.
     */
    void CopyTo(std::vector<T>& array, intcs index) const {
        std::lock_guard<std::mutex> lk(mutex_);
        intcs i = index;
        for (auto it = data_.rbegin(); it != data_.rend(); ++it, ++i) {
            if (static_cast<std::size_t>(i) < array.size()) {
                array[static_cast<std::size_t>(i)] = *it;
            } else {
                array.push_back(*it);
            }
        }
    }
};

} // namespace System::Collections::Concurrent
