// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <list>
#include <algorithm>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Generic {

    using SharpRuntime::intcs;

    /**
     * @brief Represents a doubly linked list.
     *
     * Wraps std::list. Partial C++ counterpart of .NET System.Collections.Generic.LinkedList<T>.
     *
     * @note Status: Partial
     */
    template<typename T>
    class LinkedList {
        std::list<T> list_;
    public:
        /// Initializes an empty linked list.
        LinkedList() = default;

        /// Returns the number of nodes in the list.
        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(list_.size()); }

        /// Inserts a new node containing value at the start of the list.
        void AddFirst(const T& value) { list_.push_front(value); }
        /// Inserts a new node containing value at the end of the list.
        void AddLast(const T& value)  { list_.push_back(value); }

        /// Removes the node at the start of the list. No-op if the list is empty.
        void RemoveFirst() { if (!list_.empty()) list_.pop_front(); }
        /// Removes the node at the end of the list. No-op if the list is empty.
        void RemoveLast()  { if (!list_.empty()) list_.pop_back(); }

        /// Removes the first occurrence of value. Returns true if removed, false if not found.
        bool Remove(const T& value) {
            auto it = std::find(list_.begin(), list_.end(), value);
            if (it == list_.end()) return false;
            list_.erase(it);
            return true;
        }

        /// Removes all nodes from the list.
        void Clear() { list_.clear(); }

        /// Returns true if value is present in the list.
        [[nodiscard]] bool Contains(const T& value) const {
            return std::find(list_.begin(), list_.end(), value) != list_.end();
        }

        /// Returns a reference to the value of the first node.
        [[nodiscard]] const T& getFirstProperty() const { return list_.front(); }
        /// Returns a reference to the value of the last node.
        [[nodiscard]] const T& getLastProperty()  const { return list_.back(); }

        /// Returns an iterator to the first node.
        auto begin()       { return list_.begin(); }
        /// Returns an iterator past the last node.
        auto end()         { return list_.end(); }
        /// Returns a const iterator to the first node.
        auto begin() const { return list_.cbegin(); }
        /// Returns a const iterator past the last node.
        auto end()   const { return list_.cend(); }
    };

} // namespace System::Collections::Generic
