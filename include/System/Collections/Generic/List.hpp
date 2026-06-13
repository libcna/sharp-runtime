// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <vector>

#include "System/Collections/Generic/IList.hpp"

namespace System::Collections::Generic
{
    /**
     * @brief A strongly typed list of objects accessible by index.
     *
     * C++ counterpart of the .NET System.Collections.Generic.List<T> class.
     * Backed by std::vector<T>.
     *
     * @tparam T The type of elements in the list.
     */
    template<typename T>
    class List : public IList<T>
    {
    private:
        std::vector<T> items_;

        class Enumerator : public IEnumerator<T>
        {
            const std::vector<T>& items_;
            int index_ = -1;
        public:
            explicit Enumerator(const std::vector<T>& items) : items_(items) {}
            bool MoveNext() override { return ++index_ < static_cast<int>(items_.size()); }
            void Reset() override { index_ = -1; }
            [[nodiscard]] const T& Current() const override { return items_[index_]; }
        };

    public:
        List() = default;

        explicit List(const std::vector<T>& source) : items_(source) {}

        [[nodiscard]] int getCountProperty() const override
        {
            return static_cast<int>(items_.size());
        }

        [[nodiscard]] bool getIsReadOnlyProperty() const override { return false; }

        void Add(const T& item) override { items_.push_back(item); }

        void Clear() override { items_.clear(); }

        [[nodiscard]] bool Contains(const T& item) const override
        {
            return std::find(items_.begin(), items_.end(), item) != items_.end();
        }

        bool Remove(const T& item) override
        {
            auto it = std::find(items_.begin(), items_.end(), item);
            if (it == items_.end()) return false;
            items_.erase(it);
            return true;
        }

        [[nodiscard]] const T& operator[](int index) const override
        {
            return items_.at(static_cast<std::size_t>(index));
        }

        T& operator[](int index) override
        {
            return items_.at(static_cast<std::size_t>(index));
        }

        [[nodiscard]] int IndexOf(const T& item) const override
        {
            auto it = std::find(items_.begin(), items_.end(), item);
            if (it == items_.end()) return -1;
            return static_cast<int>(it - items_.begin());
        }

        void Insert(int index, const T& item) override
        {
            items_.insert(items_.begin() + index, item);
        }

        void RemoveAt(int index) override
        {
            items_.erase(items_.begin() + index);
        }

        IEnumerator<T>* GetEnumerator() override
        {
            return new Enumerator(items_);
        }

        /// Returns the underlying std::vector for STL interop.
        [[nodiscard]] const std::vector<T>& ToVector() const { return items_; }
        [[nodiscard]] std::vector<T>& ToVector() { return items_; }

        auto begin() { return items_.begin(); }
        auto end()   { return items_.end(); }
        [[nodiscard]] auto begin() const { return items_.cbegin(); }
        [[nodiscard]] auto end()   const { return items_.cend(); }

        /// @brief Appends all elements of @p collection to the end of the list.
        void AddRange(const std::vector<T>& collection) {
            items_.insert(items_.end(), collection.begin(), collection.end());
        }

        /// @brief Appends all elements of another List to the end of this list.
        void AddRange(const List<T>& other) { AddRange(other.items_); }

        /// @brief Inserts all elements of @p collection at @p index.
        void InsertRange(int index, const std::vector<T>& collection) {
            items_.insert(items_.begin() + index, collection.begin(), collection.end());
        }

        /// @brief Returns a new List containing @p count elements starting at @p index.
        [[nodiscard]] List<T> GetRange(int index, int count) const {
            if (index < 0 || count < 0 || index + count > static_cast<int>(items_.size()))
                throw std::out_of_range("GetRange: index or count out of range.");
            return List<T>(std::vector<T>(items_.begin() + index, items_.begin() + index + count));
        }

        /// @brief Returns a copy of all elements as a std::vector (equivalent to .NET ToArray()).
        [[nodiscard]] std::vector<T> ToArray() const { return items_; }

        /// @brief Sorts the list in ascending order using operator<.
        void Sort() {
            std::sort(items_.begin(), items_.end());
        }

        /// @brief Sorts the list using a comparison function (returns negative/zero/positive).
        void Sort(std::function<int(const T&, const T&)> comparison) {
            std::sort(items_.begin(), items_.end(),
                      [&](const T& a, const T& b) { return comparison(a, b) < 0; });
        }

        /// @brief Reverses the order of elements in the list in place.
        void Reverse() {
            std::reverse(items_.begin(), items_.end());
        }

        /// @brief Returns the first element satisfying @p predicate, or default T{} if none.
        [[nodiscard]] T Find(std::function<bool(const T&)> predicate) const {
            for (const auto& item : items_)
                if (predicate(item)) return item;
            return T{};
        }

        /// @brief Returns a new List of all elements satisfying @p predicate.
        [[nodiscard]] List<T> FindAll(std::function<bool(const T&)> predicate) const {
            List<T> result;
            for (const auto& item : items_)
                if (predicate(item)) result.Add(item);
            return result;
        }

        /// @brief Returns the index of the first element satisfying @p predicate, or -1 if none.
        [[nodiscard]] int FindIndex(std::function<bool(const T&)> predicate) const {
            for (int i = 0; i < static_cast<int>(items_.size()); ++i)
                if (predicate(items_[i])) return i;
            return -1;
        }

        /// @brief Returns the index of the last element satisfying @p predicate, or -1 if none.
        [[nodiscard]] int FindLastIndex(std::function<bool(const T&)> predicate) const {
            for (int i = static_cast<int>(items_.size()) - 1; i >= 0; --i)
                if (predicate(items_[i])) return i;
            return -1;
        }

        /// @brief Removes all elements satisfying @p predicate; returns count removed.
        int RemoveAll(std::function<bool(const T&)> predicate) {
            auto it = std::remove_if(items_.begin(), items_.end(), predicate);
            int count = static_cast<int>(items_.end() - it);
            items_.erase(it, items_.end());
            return count;
        }

        /// @brief Applies @p action to every element.
        void ForEach(std::function<void(const T&)> action) const {
            for (const auto& item : items_) action(item);
        }

        /// @brief Returns true if any element satisfies @p predicate.
        [[nodiscard]] bool Exists(std::function<bool(const T&)> predicate) const {
            for (const auto& item : items_)
                if (predicate(item)) return true;
            return false;
        }

        /// @brief Returns true if all elements satisfy @p predicate (true for empty list).
        [[nodiscard]] bool TrueForAll(std::function<bool(const T&)> predicate) const {
            for (const auto& item : items_)
                if (!predicate(item)) return false;
            return true;
        }

        /// @brief Performs a binary search on a sorted list; returns index or bitwise complement of insertion point.
        [[nodiscard]] int BinarySearch(const T& item) const {
            auto it = std::lower_bound(items_.begin(), items_.end(), item);
            if (it != items_.end() && *it == item)
                return static_cast<int>(it - items_.begin());
            return ~static_cast<int>(it - items_.begin());
        }
    };
}
