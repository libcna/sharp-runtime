// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <list>
#include <algorithm>
#include <optional>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Generic {

    using SharpRuntime::intcs;

    template<typename T> class LinkedList;

    /**
     * @brief Represents a node in a LinkedList<T>.
     *
     * Wraps a std::list<T> iterator. A default-constructed node is null (no value).
     * Implicit conversion to T allows use in contexts expecting the value directly.
     */
    template<typename T>
    class LinkedListNode {
        using iter_t = typename std::list<T>::iterator;
        std::optional<iter_t> opt_;
        explicit LinkedListNode(iter_t it) : opt_(it) {}
        friend class LinkedList<T>;
    public:
        LinkedListNode() = default;

        /** Returns false if this node is null (default-constructed or removed). */
        explicit operator bool() const { return opt_.has_value(); }

        /** Returns the value stored in this node. */
        T& getValueProperty()             { return **opt_; }
        const T& getValueProperty() const { return **opt_; }

        /** Implicit conversion to T — allows existing code that expects T directly. */
        operator T() const { return **opt_; }

        bool operator==(std::nullptr_t) const { return !opt_.has_value(); }
        bool operator!=(std::nullptr_t) const { return  opt_.has_value(); }

        bool operator==(const T& other) const { return opt_.has_value() && **opt_ == other; }
        bool operator!=(const T& other) const { return !(*this == other); }
    };

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
        /** Initializes an empty linked list. */
        LinkedList() = default;

        /** Returns the number of nodes in the list. */
        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(list_.size()); }

        /** Returns a node wrapping the first element, or a null node if the list is empty. */
        [[nodiscard]] LinkedListNode<T> getFirstProperty() const {
            if (list_.empty()) return {};
            return LinkedListNode<T>(const_cast<std::list<T>&>(list_).begin());
        }

        /** Returns a node wrapping the last element, or a null node if the list is empty. */
        [[nodiscard]] LinkedListNode<T> getLastProperty() const {
            if (list_.empty()) return {};
            auto it = const_cast<std::list<T>&>(list_).end();
            --it;
            return LinkedListNode<T>(it);
        }

        /** Inserts value at the start of the list and returns the new node. */
        LinkedListNode<T> AddFirst(const T& value) {
            list_.push_front(value);
            return LinkedListNode<T>(list_.begin());
        }

        /** Inserts value at the end of the list and returns the new node. */
        LinkedListNode<T> AddLast(const T& value) {
            list_.push_back(value);
            auto it = list_.end(); --it;
            return LinkedListNode<T>(it);
        }

        /** Inserts value before the given node and returns the new node. */
        LinkedListNode<T> AddBefore(LinkedListNode<T> node, const T& value) {
            auto it = list_.insert(*node.opt_, value);
            return LinkedListNode<T>(it);
        }

        /** Inserts value after the given node and returns the new node. */
        LinkedListNode<T> AddAfter(LinkedListNode<T> node, const T& value) {
            auto it = *node.opt_;
            ++it;
            it = list_.insert(it, value);
            return LinkedListNode<T>(it);
        }

        /** Removes the node at the start of the list. No-op if the list is empty. */
        void RemoveFirst() { if (!list_.empty()) list_.pop_front(); }

        /** Removes the node at the end of the list. No-op if the list is empty. */
        void RemoveLast()  { if (!list_.empty()) list_.pop_back(); }

        /** Removes the first occurrence of value. Returns true if removed, false if not found. */
        bool Remove(const T& value) {
            auto it = std::find(list_.begin(), list_.end(), value);
            if (it == list_.end()) return false;
            list_.erase(it);
            return true;
        }

        /** Removes the specified node from the list. */
        void Remove(LinkedListNode<T> node) {
            if (node) list_.erase(*node.opt_);
        }

        /** Removes all nodes from the list. */
        void Clear() { list_.clear(); }

        /** Returns true if value is present in the list. */
        [[nodiscard]] bool Contains(const T& value) const {
            return std::find(list_.begin(), list_.end(), value) != list_.end();
        }

        /** Returns the first node containing value, or a null node if not found. */
        [[nodiscard]] LinkedListNode<T> Find(const T& value) {
            auto it = std::find(list_.begin(), list_.end(), value);
            if (it == list_.end()) return {};
            return LinkedListNode<T>(it);
        }

        /** Returns the last node containing value, or a null node if not found. */
        [[nodiscard]] LinkedListNode<T> FindLast(const T& value) {
            for (auto it = list_.end(); it != list_.begin(); ) {
                --it;
                if (*it == value) return LinkedListNode<T>(it);
            }
            return {};
        }

        /** Returns an iterator to the first node. */
        auto begin()       { return list_.begin(); }
        /** Returns an iterator past the last node. */
        auto end()         { return list_.end(); }
        /** Returns a const iterator to the first node. */
        auto begin() const { return list_.cbegin(); }
        /** Returns a const iterator past the last node. */
        auto end()   const { return list_.cend(); }
    };

} // namespace System::Collections::Generic
