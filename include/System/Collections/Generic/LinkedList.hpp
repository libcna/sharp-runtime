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
        LinkedList() = default;

        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(list_.size()); }

        void AddFirst(const T& value) { list_.push_front(value); }
        void AddLast(const T& value)  { list_.push_back(value); }

        void RemoveFirst() { if (!list_.empty()) list_.pop_front(); }
        void RemoveLast()  { if (!list_.empty()) list_.pop_back(); }

        bool Remove(const T& value) {
            auto it = std::find(list_.begin(), list_.end(), value);
            if (it == list_.end()) return false;
            list_.erase(it);
            return true;
        }

        void Clear() { list_.clear(); }

        [[nodiscard]] bool Contains(const T& value) const {
            return std::find(list_.begin(), list_.end(), value) != list_.end();
        }

        [[nodiscard]] const T& getFirstProperty() const { return list_.front(); }
        [[nodiscard]] const T& getLastProperty()  const { return list_.back(); }

        auto begin()       { return list_.begin(); }
        auto end()         { return list_.end(); }
        auto begin() const { return list_.cbegin(); }
        auto end()   const { return list_.cend(); }
    };

} // namespace System::Collections::Generic
