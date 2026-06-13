// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <set>
#include <vector>

namespace System::Collections::Generic {

    /// Represents a collection of objects maintained in sorted order with no duplicate elements.
    template<typename T>
    class SortedSet {
        std::set<T> data_;

    public:
        /// Default-constructs an empty SortedSet.
        SortedSet() = default;

        /// Constructs a SortedSet from an initializer list.
        explicit SortedSet(std::initializer_list<T> items) : data_(items) {}

        /// Gets the number of elements in the SortedSet.
        [[nodiscard]] int  getCountProperty()  const { return static_cast<int>(data_.size()); }
        /// Returns true if the SortedSet contains no elements.
        [[nodiscard]] bool getIsEmptyProperty() const { return data_.empty(); }

        /// Gets the minimum element in the SortedSet.
        [[nodiscard]] const T& getMinProperty() const { return *data_.begin(); }
        /// Gets the maximum element in the SortedSet.
        [[nodiscard]] const T& getMaxProperty() const { return *data_.rbegin(); }

        /// Adds the specified element; returns true if it was added, false if already present.
        bool Add(const T& item) { return data_.insert(item).second; }

        /// Removes the specified element; returns true if it was found and removed.
        bool Remove(const T& item) { return data_.erase(item) > 0; }

        /// Returns true if the SortedSet contains the specified element.
        [[nodiscard]] bool Contains(const T& item) const {
            return data_.find(item) != data_.end();
        }

        /// Removes all elements from the SortedSet.
        void Clear() { data_.clear(); }

        /// Modifies the SortedSet to contain all elements in itself and the specified collection.
        void UnionWith(const SortedSet<T>& other) {
            for (auto& x : other.data_) data_.insert(x);
        }

        /// Modifies the SortedSet to contain only elements also present in the specified collection.
        void IntersectWith(const SortedSet<T>& other) {
            std::set<T> result;
            for (auto& x : data_) if (other.Contains(x)) result.insert(x);
            data_ = std::move(result);
        }

        /// Removes all elements in the specified collection from the SortedSet.
        void ExceptWith(const SortedSet<T>& other) {
            for (auto& x : other.data_) data_.erase(x);
        }

        /// Returns true if every element of this set is contained in the other set.
        [[nodiscard]] bool IsSubsetOf(const SortedSet<T>& other) const {
            for (auto& x : data_) if (!other.Contains(x)) return false;
            return true;
        }

        /// Returns true if every element of the other set is contained in this set.
        [[nodiscard]] bool IsSupersetOf(const SortedSet<T>& other) const {
            return other.IsSubsetOf(*this);
        }

        /// Returns a view of elements that fall within the specified inclusive range [lower, upper].
        [[nodiscard]] SortedSet<T> GetViewBetween(const T& lower, const T& upper) const {
            SortedSet<T> view;
            for (auto it = data_.lower_bound(lower); it != data_.end() && *it <= upper; ++it)
                view.Add(*it);
            return view;
        }

        /// Copies the SortedSet elements to a new vector.
        [[nodiscard]] std::vector<T> ToVector() const {
            return std::vector<T>(data_.begin(), data_.end());
        }

        /// Returns a const iterator to the beginning of the SortedSet.
        auto begin() const { return data_.begin(); }
        /// Returns a const iterator past the end of the SortedSet.
        auto end()   const { return data_.end(); }
    };

} // namespace System::Collections::Generic
