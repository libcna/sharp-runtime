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

    template<typename T>
    class ImmutableArray {
        std::shared_ptr<const std::vector<T>> data_;

        explicit ImmutableArray(std::shared_ptr<const std::vector<T>> data) : data_(std::move(data)) {}

    public:
        ImmutableArray() : data_(std::make_shared<std::vector<T>>()) {}

        static ImmutableArray<T> Empty() { return ImmutableArray<T>(); }

        static ImmutableArray<T> Create(std::initializer_list<T> items) {
            return ImmutableArray<T>(std::make_shared<std::vector<T>>(items));
        }

        static ImmutableArray<T> Create(const std::vector<T>& items) {
            return ImmutableArray<T>(std::make_shared<std::vector<T>>(items));
        }

        [[nodiscard]] int  getLengthProperty()   const { return static_cast<int>(data_->size()); }
        [[nodiscard]] bool getIsEmptyProperty()  const { return data_->empty(); }
        [[nodiscard]] bool getIsDefaultProperty()const { return !data_; }

        const T& operator[](int index) const {
            if (index < 0 || index >= getLengthProperty())
                throw std::out_of_range("Index out of range.");
            return (*data_)[static_cast<size_t>(index)];
        }

        ImmutableArray<T> Add(const T& item) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            v->push_back(item);
            return ImmutableArray<T>(std::move(v));
        }

        ImmutableArray<T> AddRange(const std::vector<T>& items) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            for (auto& i : items) v->push_back(i);
            return ImmutableArray<T>(std::move(v));
        }

        ImmutableArray<T> Remove(const T& item) const {
            auto v = std::make_shared<std::vector<T>>();
            for (auto& x : *data_) if (!(x == item)) v->push_back(x);
            return ImmutableArray<T>(std::move(v));
        }

        ImmutableArray<T> RemoveAt(int index) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            v->erase(v->begin() + index);
            return ImmutableArray<T>(std::move(v));
        }

        ImmutableArray<T> Insert(int index, const T& item) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            v->insert(v->begin() + index, item);
            return ImmutableArray<T>(std::move(v));
        }

        ImmutableArray<T> SetItem(int index, const T& item) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            (*v)[static_cast<size_t>(index)] = item;
            return ImmutableArray<T>(std::move(v));
        }

        ImmutableArray<T> Sort() const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            std::sort(v->begin(), v->end());
            return ImmutableArray<T>(std::move(v));
        }

        [[nodiscard]] bool Contains(const T& item) const {
            return std::find(data_->begin(), data_->end(), item) != data_->end();
        }

        [[nodiscard]] int IndexOf(const T& item) const {
            auto it = std::find(data_->begin(), data_->end(), item);
            return it == data_->end() ? -1 : static_cast<int>(it - data_->begin());
        }

        [[nodiscard]] std::vector<T> ToVector() const { return *data_; }

        auto begin() const { return data_->begin(); }
        auto end()   const { return data_->end(); }

        bool operator==(const ImmutableArray<T>& o) const { return data_ == o.data_ || *data_ == *o.data_; }
        bool operator!=(const ImmutableArray<T>& o) const { return !(*this == o); }
    };

} // namespace System::Collections::Immutable
