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
 * @brief Represents a non-generic last-in, first-out (LIFO) collection of objects.
 *
 * C++ counterpart of .NET System.Collections.Stack.
 * Elements are stored as void* pointers. Prefer Generic::Stack&lt;T&gt; for type-safe code.
 */
class Stack : public ICollection {
public:
    /** @brief Constructs an empty Stack. */
    Stack() = default;

    /**
     * @brief Constructs an empty Stack with a reserved capacity hint.
     *
     * C++ counterpart of .NET Stack(int initialCapacity).
     * @param initialCapacity Capacity hint; ignored in this implementation.
     */
    explicit Stack(intcs /*initialCapacity*/) {}

    /**
     * @brief Constructs a Stack populated with all elements from @p col.
     *
     * C++ counterpart of .NET Stack(ICollection col).
     * Each element is the raw void* returned by IEnumerator::getCurrent().
     * @param col Source collection.
     */
    explicit Stack(ICollection& col) {
        IEnumerator* e = col.GetEnumerator();
        if (e) { while (e->MoveNext()) s_.push_back(e->getCurrent()); delete e; }
    }

    // -----------------------------------------------------------------------
    // ICollection
    // -----------------------------------------------------------------------

    /**
     * @brief Gets the number of elements in the Stack.
     *
     * C++ counterpart of .NET Stack.Count.
     */
    [[nodiscard]] intcs getCountProperty() const override {
        return static_cast<intcs>(s_.size());
    }

    /**
     * @brief Copies all elements into a void*[] destination array (top-to-bottom order).
     *
     * C++ counterpart of .NET Stack.CopyTo(Array, int).
     * @param array Pointer to a void*[] buffer.
     * @param index Zero-based index at which copying begins.
     */
    void CopyTo(void* array, int index) override {
        auto** dest = static_cast<void**>(array);
        int i = index;
        for (auto it = s_.rbegin(); it != s_.rend(); ++it) dest[i++] = *it;
    }

    /** @brief Returns false; Stack is not synchronized. */
    [[nodiscard]] bool getIsSynchronizedProperty() const override { return false; }

    /** @brief Returns a synchronization root for this Stack. */
    [[nodiscard]] const void* getSyncRootProperty() const override { return this; }

    /** @brief Returns a heap-allocated enumerator; stub returns nullptr. */
    IEnumerator* GetEnumerator() override { return nullptr; }

    // -----------------------------------------------------------------------
    // Stack operations
    // -----------------------------------------------------------------------

    /**
     * @brief Inserts an object at the top of the Stack.
     *
     * C++ counterpart of .NET Stack.Push(object?).
     * @param item The object to push.
     */
    void Push(void* item) { s_.push_back(item); }

    /**
     * @brief Removes and returns the object at the top of the Stack.
     *
     * C++ counterpart of .NET Stack.Pop().
     * @throws std::invalid_argument if the Stack is empty.
     */
    void* Pop() {
        if (s_.empty()) throw std::invalid_argument("Stack is empty.");
        void* v = s_.back(); s_.pop_back(); return v;
    }

    /**
     * @brief Returns the object at the top of the Stack without removing it.
     *
     * C++ counterpart of .NET Stack.Peek().
     * @throws std::invalid_argument if the Stack is empty.
     */
    [[nodiscard]] void* Peek() const {
        if (s_.empty()) throw std::invalid_argument("Stack is empty.");
        return s_.back();
    }

    /**
     * @brief Determines whether an element is in the Stack.
     *
     * C++ counterpart of .NET Stack.Contains(object?).
     * @param item The object to locate.
     * @return true if @p item is found; otherwise false.
     */
    [[nodiscard]] bool Contains(void* item) const {
        for (void* p : s_) if (p == item) return true;
        return false;
    }

    /**
     * @brief Removes all objects from the Stack.
     *
     * C++ counterpart of .NET Stack.Clear().
     */
    void Clear() { s_.clear(); }

    /**
     * @brief Copies the Stack elements to a new void*[] array (top element first).
     *
     * C++ counterpart of .NET Stack.ToArray().
     * @return Vector of void* elements with top element first.
     */
    [[nodiscard]] std::vector<void*> ToArray() const {
        return std::vector<void*>(s_.rbegin(), s_.rend());
    }

    /**
     * @brief Returns a shallow copy of the Stack.
     *
     * C++ counterpart of .NET Stack.Clone().
     */
    [[nodiscard]] Stack Clone() const {
        Stack copy;
        copy.s_ = s_;
        return copy;
    }

private:
    std::deque<void*> s_;
};

} // namespace System::Collections
