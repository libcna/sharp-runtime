// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <deque>
#include <stdexcept>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IEnumerator.hpp"

namespace System::Collections {

using SharpRuntime::intcs;

/**
 * @brief Represents a non-generic first-in, first-out (FIFO) collection of objects.
 *
 * C++ counterpart of .NET System.Collections.Queue.
 * Elements are stored as void* pointers. Prefer Generic::Queue&lt;T&gt; for type-safe code.
 */
class Queue : public ICollection {
public:
    /** @brief Constructs an empty Queue. */
    Queue() = default;

    /**
     * @brief Constructs an empty Queue with a reserved capacity hint.
     *
     * C++ counterpart of .NET Queue(int capacity).
     * @param capacity Initial capacity hint; ignored in this implementation.
     */
    explicit Queue(intcs /*capacity*/) {}

    /**
     * @brief Constructs a Queue populated with all elements from @p col.
     *
     * C++ counterpart of .NET Queue(ICollection col).
     * Each element is the raw void* returned by IEnumerator::getCurrent().
     * @param col Source collection.
     */
    explicit Queue(ICollection& col) {
        IEnumerator* e = col.GetEnumerator();
        if (e) { while (e->MoveNext()) q_.push_back(e->getCurrent()); delete e; }
    }

    // -----------------------------------------------------------------------
    // ICollection
    // -----------------------------------------------------------------------

    /**
     * @brief Gets the number of elements in the Queue.
     *
     * C++ counterpart of .NET Queue.Count.
     */
    [[nodiscard]] intcs getCountProperty() const override {
        return static_cast<intcs>(q_.size());
    }

    /**
     * @brief Copies all elements into a void*[] destination array.
     *
     * C++ counterpart of .NET Queue.CopyTo(Array, int).
     * @param array Pointer to a void*[] buffer.
     * @param index Zero-based index at which copying begins.
     */
    void CopyTo(void* array, int index) override {
        auto** dest = static_cast<void**>(array);
        int i = index;
        for (void* p : q_) dest[i++] = p;
    }

    /** @brief Returns false; Queue is not synchronized. */
    [[nodiscard]] bool getIsSynchronizedProperty() const override { return false; }

    /** @brief Returns a synchronization root for this Queue. */
    [[nodiscard]] const void* getSyncRootProperty() const override { return this; }

    /** @brief Returns a heap-allocated enumerator; stub returns nullptr. */
    IEnumerator* GetEnumerator() override { return nullptr; }

    // -----------------------------------------------------------------------
    // Queue operations
    // -----------------------------------------------------------------------

    /**
     * @brief Adds an object to the end of the Queue.
     *
     * C++ counterpart of .NET Queue.Enqueue(object?).
     * @param item The object to add.
     */
    void Enqueue(void* item) { q_.push_back(item); }

    /**
     * @brief Removes and returns the object at the beginning of the Queue.
     *
     * C++ counterpart of .NET Queue.Dequeue().
     * @throws std::invalid_argument if the Queue is empty.
     */
    void* Dequeue() {
        if (q_.empty()) throw std::invalid_argument("Queue is empty.");
        void* v = q_.front(); q_.pop_front(); return v;
    }

    /**
     * @brief Returns the object at the beginning of the Queue without removing it.
     *
     * C++ counterpart of .NET Queue.Peek().
     * @throws std::invalid_argument if the Queue is empty.
     */
    [[nodiscard]] void* Peek() const {
        if (q_.empty()) throw std::invalid_argument("Queue is empty.");
        return q_.front();
    }

    /**
     * @brief Determines whether an element is in the Queue.
     *
     * C++ counterpart of .NET Queue.Contains(object?).
     * @param item The object to locate.
     * @return true if @p item is found; otherwise false.
     */
    [[nodiscard]] bool Contains(void* item) const {
        for (void* p : q_) if (p == item) return true;
        return false;
    }

    /**
     * @brief Removes all objects from the Queue.
     *
     * C++ counterpart of .NET Queue.Clear().
     */
    void Clear() { q_.clear(); }

    /**
     * @brief Copies the Queue elements to a new void*[] array.
     *
     * C++ counterpart of .NET Queue.ToArray().
     * @return Vector of void* elements in FIFO order.
     */
    [[nodiscard]] std::vector<void*> ToArray() const {
        return std::vector<void*>(q_.begin(), q_.end());
    }

    /**
     * @brief Returns a shallow copy of the Queue.
     *
     * C++ counterpart of .NET Queue.Clone().
     */
    [[nodiscard]] Queue Clone() const {
        Queue copy;
        copy.q_ = q_;
        return copy;
    }

    /**
     * @brief Sets the capacity to the actual number of elements; no-op in this implementation.
     *
     * C++ counterpart of .NET Queue.TrimToSize().
     */
    void TrimToSize() {}

    /** @brief Returns an iterator to the beginning of the Queue. */
    auto begin() const { return q_.begin(); }
    /** @brief Returns an iterator past the end of the Queue. */
    auto end()   const { return q_.end(); }

private:
    std::deque<void*> q_;
};

} // namespace System::Collections
