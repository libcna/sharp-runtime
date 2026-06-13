// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <initializer_list>
#include <memory>
#include <vector>

namespace System::Collections::Immutable {

    /// An immutable list of elements that provides O(n) structural-sharing mutations.
    template<typename T>
    class ImmutableList {
        std::shared_ptr<const std::vector<T>> data_;

        explicit ImmutableList(std::shared_ptr<const std::vector<T>> data) : data_(std::move(data)) {}

    public:
        /// Default-constructs an empty ImmutableList.
        ImmutableList() : data_(std::make_shared<std::vector<T>>()) {}

        /// Returns an empty ImmutableList.
        static ImmutableList<T> Empty() { return ImmutableList<T>(); }

        /// Creates an ImmutableList from an initializer list.
        static ImmutableList<T> Create(std::initializer_list<T> items) {
            return ImmutableList<T>(std::make_shared<std::vector<T>>(items));
        }
        /// Creates an ImmutableList from an existing vector.
        static ImmutableList<T> Create(const std::vector<T>& items) {
            return ImmutableList<T>(std::make_shared<std::vector<T>>(items));
        }

        /// Gets the number of elements in the list.
        [[nodiscard]] int  getCountProperty() const { return static_cast<int>(data_->size()); }
        /// Returns true if the list contains no elements.
        [[nodiscard]] bool getIsEmptyProperty()const { return data_->empty(); }

        /// Returns a const reference to the element at the given zero-based index.
        const T& operator[](int index) const {
            return (*data_)[static_cast<size_t>(index)];
        }

        /// Returns a new list with item appended at the end.
        ImmutableList<T> Add(const T& item) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            v->push_back(item);
            return ImmutableList<T>(std::move(v));
        }

        /// Returns a new list with all elements from items appended.
        ImmutableList<T> AddRange(const std::vector<T>& items) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            for (auto& i : items) v->push_back(i);
            return ImmutableList<T>(std::move(v));
        }

        /// Returns a new list with the first occurrence of item removed.
        ImmutableList<T> Remove(const T& item) const {
            auto v = std::make_shared<std::vector<T>>();
            for (auto& x : *data_) if (!(x == item)) v->push_back(x);
            return ImmutableList<T>(std::move(v));
        }

        /// Returns a new list with the element at the given index removed.
        ImmutableList<T> RemoveAt(int index) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            v->erase(v->begin() + index);
            return ImmutableList<T>(std::move(v));
        }

        /// Returns a new list with item inserted at the given index.
        ImmutableList<T> Insert(int index, const T& item) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            v->insert(v->begin() + index, item);
            return ImmutableList<T>(std::move(v));
        }

        /// Returns a new list with the element at index replaced by item.
        ImmutableList<T> SetItem(int index, const T& item) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            (*v)[static_cast<size_t>(index)] = item;
            return ImmutableList<T>(std::move(v));
        }

        /// Returns true if the list contains the specified item.
        [[nodiscard]] bool Contains(const T& item) const {
            return std::find(data_->begin(), data_->end(), item) != data_->end();
        }

        /// Returns the zero-based index of the first occurrence of item, or -1 if not found.
        [[nodiscard]] int IndexOf(const T& item) const {
            auto it = std::find(data_->begin(), data_->end(), item);
            return it == data_->end() ? -1 : static_cast<int>(it - data_->begin());
        }

        /// Returns a const iterator to the beginning of the list.
        auto begin() const { return data_->begin(); }
        /// Returns a const iterator past the end of the list.
        auto end()   const { return data_->end(); }
    };

} // namespace System::Collections::Immutable
