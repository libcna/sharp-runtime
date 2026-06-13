// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <vector>
#include <algorithm>
#include <stdexcept>

#include "System/Collections/Generic/IList.hpp"

namespace System::Collections::ObjectModel
{
    /**
     * @brief Provides the base class for a generic collection.
     *
     * C++ counterpart of the .NET System.Collections.ObjectModel.Collection<T> class.
     * Backed by std::vector<T>.
     *
     * @tparam T The type of elements in the collection.
     */
    template<typename T>
    class Collection : public Generic::IList<T>
    {
    private:
        class Enumerator : public Generic::IEnumerator<T>
        {
            const std::vector<T>& items_;
            int index_ = -1;
        public:
            explicit Enumerator(const std::vector<T>& items) : items_(items) {}
            bool MoveNext() override { return ++index_ < static_cast<int>(items_.size()); }
            void Reset() override { index_ = -1; }
            [[nodiscard]] const T& Current() const override { return items_[index_]; }
        };

    protected:
        /// The underlying storage for collection items.
        std::vector<T> items_;

        /// Inserts item at the given index into items_; override to customise insertion.
        virtual void InsertItem(int index, const T& item)
        {
            items_.insert(items_.begin() + index, item);
        }

        /// Removes the item at the given index from items_; override to customise removal.
        virtual void RemoveItem(int index)
        {
            items_.erase(items_.begin() + index);
        }

        /// Removes all items from items_; override to customise clearing.
        virtual void ClearItems()
        {
            items_.clear();
        }

        /// Replaces the item at the given index; override to customise replacement.
        virtual void SetItem(int index, const T& item)
        {
            items_[index] = item;
        }

    public:
        /// Default-constructs an empty Collection.
        Collection() = default;
        /// Destroys the collection.
        ~Collection() override = default;

        /// Gets the number of elements in the collection.
        [[nodiscard]] int getCountProperty() const override
        {
            return static_cast<int>(items_.size());
        }

        /// Returns false because this collection allows modifications.
        [[nodiscard]] bool getIsReadOnlyProperty() const override { return false; }

        /// Adds item to the end of the collection.
        void Add(const T& item) override
        {
            InsertItem(static_cast<int>(items_.size()), item);
        }

        /// Removes all elements from the collection.
        void Clear() override { ClearItems(); }

        /// Returns true if the collection contains the specified item.
        [[nodiscard]] bool Contains(const T& item) const override
        {
            return std::find(items_.begin(), items_.end(), item) != items_.end();
        }

        /// Removes the first occurrence of item; returns true if removed.
        bool Remove(const T& item) override
        {
            auto it = std::find(items_.begin(), items_.end(), item);
            if (it == items_.end()) return false;
            RemoveItem(static_cast<int>(it - items_.begin()));
            return true;
        }

        /// Returns a const reference to the element at the specified index.
        [[nodiscard]] const T& operator[](int index) const override { return items_.at(index); }
        /// Returns a reference to the element at the specified index.
        T& operator[](int index) override { return items_.at(index); }

        /// Returns the zero-based index of the first occurrence of item, or -1 if not found.
        [[nodiscard]] int IndexOf(const T& item) const override
        {
            auto it = std::find(items_.begin(), items_.end(), item);
            if (it == items_.end()) return -1;
            return static_cast<int>(it - items_.begin());
        }

        /// Inserts item at the specified index.
        void Insert(int index, const T& item) override { InsertItem(index, item); }

        /// Removes the element at the specified index.
        void RemoveAt(int index) override { RemoveItem(index); }

        /// Returns a new enumerator that iterates through the collection.
        Generic::IEnumerator<T>* GetEnumerator() override
        {
            return new Enumerator(items_);
        }

        /// Returns an iterator to the beginning of the collection.
        auto begin()        { return items_.begin(); }
        /// Returns an iterator past the end of the collection.
        auto end()          { return items_.end(); }
        /// Returns a const iterator to the beginning of the collection.
        [[nodiscard]] auto begin() const { return items_.cbegin(); }
        /// Returns a const iterator past the end of the collection.
        [[nodiscard]] auto end()   const { return items_.cend(); }
    };
}
