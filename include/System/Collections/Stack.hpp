// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <deque>
#include <stdexcept>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections {

    using SharpRuntime::intcs;

    /**
     * @brief Represents a non-generic last-in, first-out (LIFO) collection of objects.
     *
     * Wraps std::deque<void*>. Partial C++ counterpart of .NET System.Collections.Stack.
     * Prefer System::Collections::Generic::Stack<T> for type-safe code.
     *
     * @note Status: Stub
     */
    class Stack {
        std::deque<void*> s_;
    public:
        /// Default-constructs an empty Stack.
        Stack() = default;

        /// Gets the number of elements contained in the Stack.
        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(s_.size()); }

        /// Inserts an object at the top of the Stack.
        void Push(void* item) { s_.push_back(item); }

        /// Removes and returns the object at the top of the Stack.
        void* Pop() {
            if (s_.empty()) throw std::invalid_argument("Stack is empty.");
            void* v = s_.back(); s_.pop_back(); return v;
        }

        /// Returns the object at the top of the Stack without removing it.
        [[nodiscard]] void* Peek() const {
            if (s_.empty()) throw std::invalid_argument("Stack is empty.");
            return s_.back();
        }

        /// Returns true if the Stack contains the specified object.
        [[nodiscard]] bool Contains(void* item) const {
            for (auto* p : s_) if (p == item) return true;
            return false;
        }

        /// Removes all objects from the Stack.
        void Clear() { s_.clear(); }
    };

} // namespace System::Collections
