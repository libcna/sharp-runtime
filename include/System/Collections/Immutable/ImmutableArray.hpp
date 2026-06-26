// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <vector>

namespace System::Collections::Immutable {

    /** An immutable array with value semantics and structural-sharing mutations. */
    template<typename T>
    class ImmutableArray {
        std::shared_ptr<const std::vector<T>> data_;

        explicit ImmutableArray(std::shared_ptr<const std::vector<T>> data) : data_(std::move(data)) {}

    public:
        /** Default-constructs an empty ImmutableArray. */
        ImmutableArray() : data_(std::make_shared<std::vector<T>>()) {}

        /** Returns an empty ImmutableArray. */
        static ImmutableArray<T> Empty() { return ImmutableArray<T>(); }

        /** Creates an ImmutableArray from an initializer list. */
        static ImmutableArray<T> Create(std::initializer_list<T> items) {
            return ImmutableArray<T>(std::make_shared<std::vector<T>>(items));
        }

        /** Creates an ImmutableArray from an existing vector. */
        static ImmutableArray<T> Create(const std::vector<T>& items) {
            return ImmutableArray<T>(std::make_shared<std::vector<T>>(items));
        }

        /** Gets the number of elements in the array. */
        [[nodiscard]] int  getLengthProperty()   const { return static_cast<int>(data_->size()); }
        /** Returns true if the array contains no elements. */
        [[nodiscard]] bool getIsEmptyProperty()  const { return data_->empty(); }
        /** Returns true if the array has not been initialized (default-constructed with null data). */
        [[nodiscard]] bool getIsDefaultProperty()const { return !data_; }

        /** Returns a const reference to the element at the given zero-based index. */
        const T& operator[](int index) const {
            if (index < 0 || index >= getLengthProperty())
                throw std::out_of_range("Index out of range.");
            return (*data_)[static_cast<size_t>(index)];
        }

        /** Returns a new array with item appended at the end. */
        ImmutableArray<T> Add(const T& item) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            v->push_back(item);
            return ImmutableArray<T>(std::move(v));
        }

        /** Returns a new array with all elements from items appended. */
        ImmutableArray<T> AddRange(const std::vector<T>& items) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            for (auto& i : items) v->push_back(i);
            return ImmutableArray<T>(std::move(v));
        }

        /** Returns a new array with the first occurrence of item removed. */
        ImmutableArray<T> Remove(const T& item) const {
            auto v = std::make_shared<std::vector<T>>();
            for (auto& x : *data_) if (!(x == item)) v->push_back(x);
            return ImmutableArray<T>(std::move(v));
        }

        /** Returns a new array with the element at the given index removed. */
        ImmutableArray<T> RemoveAt(int index) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            v->erase(v->begin() + index);
            return ImmutableArray<T>(std::move(v));
        }

        /** Returns a new array with item inserted at the given index. */
        ImmutableArray<T> Insert(int index, const T& item) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            v->insert(v->begin() + index, item);
            return ImmutableArray<T>(std::move(v));
        }

        /** Returns a new array with the element at index replaced by item. */
        ImmutableArray<T> SetItem(int index, const T& item) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            (*v)[static_cast<size_t>(index)] = item;
            return ImmutableArray<T>(std::move(v));
        }

        /** Returns a new array with elements sorted in ascending order. */
        ImmutableArray<T> Sort() const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            std::sort(v->begin(), v->end());
            return ImmutableArray<T>(std::move(v));
        }

        /** Returns true if the array contains the specified item. */
        [[nodiscard]] bool Contains(const T& item) const {
            return std::find(data_->begin(), data_->end(), item) != data_->end();
        }

        /** Returns the zero-based index of the first occurrence of item, or -1 if not found. */
        [[nodiscard]] int IndexOf(const T& item) const {
            auto it = std::find(data_->begin(), data_->end(), item);
            return it == data_->end() ? -1 : static_cast<int>(it - data_->begin());
        }

        /** Copies the array elements to a new std::vector. */
        [[nodiscard]] std::vector<T> ToVector() const { return *data_; }

        /** Returns a const iterator to the beginning of the array. */
        auto begin() const { return data_->begin(); }
        /** Returns a const iterator past the end of the array. */
        auto end()   const { return data_->end(); }

        /** Returns true if both arrays contain the same elements. */
        bool operator==(const ImmutableArray<T>& o) const { return data_ == o.data_ || *data_ == *o.data_; }
        /** Returns true if the arrays do not contain the same elements. */
        bool operator!=(const ImmutableArray<T>& o) const { return !(*this == o); }
    };

} // namespace System::Collections::Immutable
