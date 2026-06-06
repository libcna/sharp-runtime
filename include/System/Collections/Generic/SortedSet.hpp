// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <set>
#include <vector>

namespace System::Collections::Generic {

    template<typename T>
    class SortedSet {
        std::set<T> data_;

    public:
        SortedSet() = default;

        explicit SortedSet(std::initializer_list<T> items) : data_(items) {}

        [[nodiscard]] int  getCountProperty()  const { return static_cast<int>(data_.size()); }
        [[nodiscard]] bool getIsEmptyProperty() const { return data_.empty(); }

        [[nodiscard]] const T& getMinProperty() const { return *data_.begin(); }
        [[nodiscard]] const T& getMaxProperty() const { return *data_.rbegin(); }

        bool Add(const T& item) { return data_.insert(item).second; }

        bool Remove(const T& item) { return data_.erase(item) > 0; }

        [[nodiscard]] bool Contains(const T& item) const {
            return data_.find(item) != data_.end();
        }

        void Clear() { data_.clear(); }

        void UnionWith(const SortedSet<T>& other) {
            for (auto& x : other.data_) data_.insert(x);
        }

        void IntersectWith(const SortedSet<T>& other) {
            std::set<T> result;
            for (auto& x : data_) if (other.Contains(x)) result.insert(x);
            data_ = std::move(result);
        }

        void ExceptWith(const SortedSet<T>& other) {
            for (auto& x : other.data_) data_.erase(x);
        }

        [[nodiscard]] bool IsSubsetOf(const SortedSet<T>& other) const {
            for (auto& x : data_) if (!other.Contains(x)) return false;
            return true;
        }

        [[nodiscard]] bool IsSupersetOf(const SortedSet<T>& other) const {
            return other.IsSubsetOf(*this);
        }

        [[nodiscard]] SortedSet<T> GetViewBetween(const T& lower, const T& upper) const {
            SortedSet<T> view;
            for (auto it = data_.lower_bound(lower); it != data_.end() && *it <= upper; ++it)
                view.Add(*it);
            return view;
        }

        [[nodiscard]] std::vector<T> ToVector() const {
            return std::vector<T>(data_.begin(), data_.end());
        }

        auto begin() const { return data_.begin(); }
        auto end()   const { return data_.end(); }
    };

} // namespace System::Collections::Generic
