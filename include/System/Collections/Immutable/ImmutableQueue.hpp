// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <deque>
#include <memory>
#include <stdexcept>

namespace System::Collections::Immutable {

    template<typename T>
    class ImmutableQueue {
        using DequeT = std::deque<T>;
        std::shared_ptr<const DequeT> data_;

        explicit ImmutableQueue(std::shared_ptr<const DequeT> data) : data_(std::move(data)) {}

    public:
        ImmutableQueue() : data_(std::make_shared<DequeT>()) {}

        static ImmutableQueue<T> Empty() { return ImmutableQueue<T>(); }

        [[nodiscard]] bool getIsEmptyProperty() const { return data_->empty(); }
        [[nodiscard]] int  getCountProperty()   const { return static_cast<int>(data_->size()); }

        [[nodiscard]] const T& Peek() const {
            if (data_->empty()) throw std::out_of_range("Queue is empty.");
            return data_->front();
        }

        [[nodiscard]] ImmutableQueue<T> Enqueue(const T& item) const {
            auto d = std::make_shared<DequeT>(*data_);
            d->push_back(item);
            return ImmutableQueue<T>(std::move(d));
        }

        [[nodiscard]] ImmutableQueue<T> Dequeue() const {
            if (data_->empty()) throw std::out_of_range("Queue is empty.");
            auto d = std::make_shared<DequeT>(*data_);
            d->pop_front();
            return ImmutableQueue<T>(std::move(d));
        }

        [[nodiscard]] ImmutableQueue<T> Dequeue(T& value) const {
            if (data_->empty()) throw std::out_of_range("Queue is empty.");
            value = data_->front();
            auto d = std::make_shared<DequeT>(*data_);
            d->pop_front();
            return ImmutableQueue<T>(std::move(d));
        }

        [[nodiscard]] ImmutableQueue<T> Clear() const { return Empty(); }

        auto begin() const { return data_->begin(); }
        auto end()   const { return data_->end(); }
    };

} // namespace System::Collections::Immutable
