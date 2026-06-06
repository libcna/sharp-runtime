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
        HashSet() = default;

        /**
         * @brief Adds the specified element to a set.
         * @return true if the element was added; false if it was already present.
         */
        bool Add(const T& item) { return set_.insert(item).second; }
        bool Add(T&& item)      { return set_.insert(std::move(item)).second; }

        /**
         * @brief Removes the specified element from a HashSet.
         * @return true if the element was found and removed; otherwise false.
         */
        bool Remove(const T& item) { return set_.erase(item) > 0; }

        [[nodiscard]] bool Contains(const T& item) const { return set_.count(item) > 0; }
        [[nodiscard]] int  getCountProperty() const      { return static_cast<int>(set_.size()); }

        void Clear() { set_.clear(); }

        void UnionWith(const HashSet<T>& other) {
            for (const auto& item : other.set_) set_.insert(item);
        }

        void IntersectWith(const HashSet<T>& other) {
            for (auto it = set_.begin(); it != set_.end(); ) {
                it = other.Contains(*it) ? ++it : set_.erase(it);
            }
        }

        void ExceptWith(const HashSet<T>& other) {
            for (const auto& item : other.set_) set_.erase(item);
        }

        [[nodiscard]] std::vector<T> ToArray() const {
            return std::vector<T>(set_.begin(), set_.end());
        }

        // Range-for support
        auto begin()       { return set_.begin(); }
        auto end()         { return set_.end(); }
        auto begin() const { return set_.begin(); }
        auto end()   const { return set_.end(); }
    };

} // namespace System::Collections::Generic
