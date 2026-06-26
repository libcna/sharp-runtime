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
     * @brief Represents a non-generic first-in, first-out (FIFO) collection of objects.
     *
     * Wraps std::deque<void*>. Partial C++ counterpart of .NET System.Collections.Queue.
     * Prefer System::Collections::Generic::Queue<T> for type-safe code.
     *
     * @note Status: Stub
     */
    class Queue {
        std::deque<void*> q_;
    public:
        /** Default-constructs an empty Queue. */
        Queue() = default;

        /** Gets the number of elements contained in the Queue. */
        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(q_.size()); }

        /** Adds an object to the end of the Queue. */
        void Enqueue(void* item) { q_.push_back(item); }

        /** Removes and returns the object at the beginning of the Queue. */
        void* Dequeue() {
            if (q_.empty()) throw std::invalid_argument("Queue is empty.");
            void* v = q_.front(); q_.pop_front(); return v;
        }

        /** Returns the object at the beginning of the Queue without removing it. */
        [[nodiscard]] void* Peek() const {
            if (q_.empty()) throw std::invalid_argument("Queue is empty.");
            return q_.front();
        }

        /** Returns true if the Queue contains the specified object. */
        [[nodiscard]] bool Contains(void* item) const {
            for (auto* p : q_) if (p == item) return true;
            return false;
        }

        /** Removes all objects from the Queue. */
        void Clear() { q_.clear(); }

        /** Returns an iterator to the beginning of the Queue. */
        auto begin() const { return q_.begin(); }
        /** Returns an iterator past the end of the Queue. */
        auto end()   const { return q_.end(); }
    };

} // namespace System::Collections
