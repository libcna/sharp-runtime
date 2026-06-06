// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <stdexcept>
#include <vector>

namespace System::Collections::Immutable {

    template<typename T>
    class ImmutableStack {
        // Stored as vector; top is the last element (index size-1)
        using VecT = std::vector<T>;
        std::shared_ptr<const VecT> data_;

        explicit ImmutableStack(std::shared_ptr<const VecT> data) : data_(std::move(data)) {}

    public:
        ImmutableStack() : data_(std::make_shared<VecT>()) {}

        static ImmutableStack<T> Empty() { return ImmutableStack<T>(); }

        [[nodiscard]] bool getIsEmptyProperty() const { return data_->empty(); }
        [[nodiscard]] int  getCountProperty()   const { return static_cast<int>(data_->size()); }

        [[nodiscard]] const T& Peek() const {
            if (data_->empty()) throw std::out_of_range("Stack is empty.");
            return data_->back();
        }

        [[nodiscard]] ImmutableStack<T> Push(const T& item) const {
            auto v = std::make_shared<VecT>(*data_);
            v->push_back(item);
            return ImmutableStack<T>(std::move(v));
        }

        [[nodiscard]] ImmutableStack<T> Pop() const {
            if (data_->empty()) throw std::out_of_range("Stack is empty.");
            auto v = std::make_shared<VecT>(*data_);
            v->pop_back();
            return ImmutableStack<T>(std::move(v));
        }

        [[nodiscard]] ImmutableStack<T> Pop(T& value) const {
            if (data_->empty()) throw std::out_of_range("Stack is empty.");
            value = data_->back();
            auto v = std::make_shared<VecT>(*data_);
            v->pop_back();
            return ImmutableStack<T>(std::move(v));
        }

        [[nodiscard]] ImmutableStack<T> Clear() const { return Empty(); }

        // Iterates top-to-bottom (reverse order)
        auto begin() const { return data_->rbegin(); }
        auto end()   const { return data_->rend(); }
    };

} // namespace System::Collections::Immutable
