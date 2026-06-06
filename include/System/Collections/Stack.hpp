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
        Stack() = default;

        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(s_.size()); }

        void Push(void* item) { s_.push_back(item); }

        void* Pop() {
            if (s_.empty()) throw std::invalid_argument("Stack is empty.");
            void* v = s_.back(); s_.pop_back(); return v;
        }

        [[nodiscard]] void* Peek() const {
            if (s_.empty()) throw std::invalid_argument("Stack is empty.");
            return s_.back();
        }

        [[nodiscard]] bool Contains(void* item) const {
            for (auto* p : s_) if (p == item) return true;
            return false;
        }

        void Clear() { s_.clear(); }
    };

} // namespace System::Collections
