// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <vector>
#include <stdexcept>

#include "System/Collections/Generic/IList.hpp"

namespace System::Collections::ObjectModel
{
    /**
     * @brief Provides a read-only wrapper around a generic list.
     *
     * C++ counterpart of the .NET System.Collections.ObjectModel.ReadOnlyCollection<T> class.
     *
     * @tparam T The type of elements in the collection.
     */
    template<typename T>
    class ReadOnlyCollection : public Generic::IList<T>
    {
    private:
        std::vector<T> items_;

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

    public:
        /// Default-constructs an empty ReadOnlyCollection.
        ReadOnlyCollection() = default;

        /// Constructs a ReadOnlyCollection wrapping a copy of source.
        explicit ReadOnlyCollection(const std::vector<T>& source) : items_(source) {}
        /// Constructs a ReadOnlyCollection by moving source.
        explicit ReadOnlyCollection(std::vector<T>&& source) : items_(std::move(source)) {}

        /// Destroys the collection.
        ~ReadOnlyCollection() override = default;

        /// Gets the number of elements in the collection.
        [[nodiscard]] int getCountProperty() const override
        {
            return static_cast<int>(items_.size());
        }

        /// Returns true because this collection does not allow modifications.
        [[nodiscard]] bool getIsReadOnlyProperty() const override { return true; }

        /// Returns a const reference to the element at the specified index.
        [[nodiscard]] const T& operator[](int index) const override
        {
            return items_.at(static_cast<std::size_t>(index));
        }

        /// Throws std::runtime_error because the collection is read-only.
        T& operator[](int index) override
        {
            throw std::runtime_error("Collection is read-only.");
        }

        /// Returns the zero-based index of the first occurrence of item, or -1 if not found.
        [[nodiscard]] int IndexOf(const T& item) const override
        {
            for (int i = 0; i < static_cast<int>(items_.size()); ++i)
                if (items_[i] == item) return i;
            return -1;
        }

        /// Returns true if the collection contains the specified item.
        [[nodiscard]] bool Contains(const T& item) const override
        {
            return IndexOf(item) >= 0;
        }

        /// Throws std::runtime_error because the collection is read-only.
        void Add(const T&) override
        {
            throw std::runtime_error("Collection is read-only.");
        }

        /// Throws std::runtime_error because the collection is read-only.
        void Clear() override
        {
            throw std::runtime_error("Collection is read-only.");
        }

        /// Throws std::runtime_error because the collection is read-only.
        bool Remove(const T&) override
        {
            throw std::runtime_error("Collection is read-only.");
        }

        /// Throws std::runtime_error because the collection is read-only.
        void Insert(int, const T&) override
        {
            throw std::runtime_error("Collection is read-only.");
        }

        /// Throws std::runtime_error because the collection is read-only.
        void RemoveAt(int) override
        {
            throw std::runtime_error("Collection is read-only.");
        }

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
