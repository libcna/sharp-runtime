// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <unordered_set>
#include <vector>

namespace System::Collections::Generic {

    /**
     * @brief Represents a set of values with no duplicate elements.
     * Provides O(1) average-case Add/Remove/Contains operations.
     *
     * @tparam T The type of elements in the set.
     *
     * @note Status: Implemented
     */
    template<typename T>
    class HashSet {
    private:
        std::unordered_set<T> set_;

    public:
        /// Default-constructs an empty HashSet.
        HashSet() = default;

        /**
         * @brief Adds the specified element to a set.
         * @return true if the element was added; false if it was already present.
         */
        /// Adds a copy of item; returns true if added, false if already present.
        bool Add(const T& item) { return set_.insert(item).second; }
        /// Adds item by move; returns true if added, false if already present.
        bool Add(T&& item)      { return set_.insert(std::move(item)).second; }

        /**
         * @brief Removes the specified element from a HashSet.
         * @return true if the element was found and removed; otherwise false.
         */
        bool Remove(const T& item) { return set_.erase(item) > 0; }

        /// Returns true if the HashSet contains the specified element.
        [[nodiscard]] bool Contains(const T& item) const { return set_.count(item) > 0; }
        /// Gets the number of elements contained in the HashSet.
        [[nodiscard]] int  getCountProperty() const      { return static_cast<int>(set_.size()); }

        /// Removes all elements from the HashSet.
        void Clear() { set_.clear(); }

        /// Modifies the HashSet to contain all elements in itself and the specified collection.
        void UnionWith(const HashSet<T>& other) {
            for (const auto& item : other.set_) set_.insert(item);
        }

        /// Modifies the HashSet to contain only elements that are also in the specified collection.
        void IntersectWith(const HashSet<T>& other) {
            for (auto it = set_.begin(); it != set_.end(); ) {
                it = other.Contains(*it) ? ++it : set_.erase(it);
            }
        }

        /// Removes all elements in the specified collection from the HashSet.
        void ExceptWith(const HashSet<T>& other) {
            for (const auto& item : other.set_) set_.erase(item);
        }

        /// Copies the HashSet elements to a new vector.
        [[nodiscard]] std::vector<T> ToArray() const {
            return std::vector<T>(set_.begin(), set_.end());
        }

        /// Returns an iterator to the beginning of the HashSet.
        auto begin()       { return set_.begin(); }
        /// Returns an iterator past the end of the HashSet.
        auto end()         { return set_.end(); }
        /// Returns a const iterator to the beginning of the HashSet.
        auto begin() const { return set_.begin(); }
        /// Returns a const iterator past the end of the HashSet.
        auto end()   const { return set_.end(); }
    };

} // namespace System::Collections::Generic
