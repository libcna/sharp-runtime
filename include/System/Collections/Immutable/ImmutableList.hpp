// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <initializer_list>
#include <memory>
#include <vector>

namespace System::Collections::Immutable {

    template<typename T>
    class ImmutableList {
        std::shared_ptr<const std::vector<T>> data_;

        explicit ImmutableList(std::shared_ptr<const std::vector<T>> data) : data_(std::move(data)) {}

    public:
        ImmutableList() : data_(std::make_shared<std::vector<T>>()) {}

        static ImmutableList<T> Empty() { return ImmutableList<T>(); }

        static ImmutableList<T> Create(std::initializer_list<T> items) {
            return ImmutableList<T>(std::make_shared<std::vector<T>>(items));
        }
        static ImmutableList<T> Create(const std::vector<T>& items) {
            return ImmutableList<T>(std::make_shared<std::vector<T>>(items));
        }

        [[nodiscard]] int  getCountProperty() const { return static_cast<int>(data_->size()); }
        [[nodiscard]] bool getIsEmptyProperty()const { return data_->empty(); }

        const T& operator[](int index) const {
            return (*data_)[static_cast<size_t>(index)];
        }

        ImmutableList<T> Add(const T& item) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            v->push_back(item);
            return ImmutableList<T>(std::move(v));
        }

        ImmutableList<T> AddRange(const std::vector<T>& items) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            for (auto& i : items) v->push_back(i);
            return ImmutableList<T>(std::move(v));
        }

        ImmutableList<T> Remove(const T& item) const {
            auto v = std::make_shared<std::vector<T>>();
            for (auto& x : *data_) if (!(x == item)) v->push_back(x);
            return ImmutableList<T>(std::move(v));
        }

        ImmutableList<T> RemoveAt(int index) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            v->erase(v->begin() + index);
            return ImmutableList<T>(std::move(v));
        }

        ImmutableList<T> Insert(int index, const T& item) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            v->insert(v->begin() + index, item);
            return ImmutableList<T>(std::move(v));
        }

        ImmutableList<T> SetItem(int index, const T& item) const {
            auto v = std::make_shared<std::vector<T>>(*data_);
            (*v)[static_cast<size_t>(index)] = item;
            return ImmutableList<T>(std::move(v));
        }

        [[nodiscard]] bool Contains(const T& item) const {
            return std::find(data_->begin(), data_->end(), item) != data_->end();
        }

        [[nodiscard]] int IndexOf(const T& item) const {
            auto it = std::find(data_->begin(), data_->end(), item);
            return it == data_->end() ? -1 : static_cast<int>(it - data_->begin());
        }

        auto begin() const { return data_->begin(); }
        auto end()   const { return data_->end(); }
    };

} // namespace System::Collections::Immutable
