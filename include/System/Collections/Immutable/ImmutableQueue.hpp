// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <deque>
#include <memory>
#include <stdexcept>

namespace System::Collections::Immutable {

    /** An immutable first-in, first-out (FIFO) queue. */
    template<typename T>
    class ImmutableQueue {
        using DequeT = std::deque<T>;
        std::shared_ptr<const DequeT> data_;

        explicit ImmutableQueue(std::shared_ptr<const DequeT> data) : data_(std::move(data)) {}

    public:
        /** Default-constructs an empty ImmutableQueue. */
        ImmutableQueue() : data_(std::make_shared<DequeT>()) {}

        /** Returns an empty ImmutableQueue. */
        static ImmutableQueue<T> Empty() { return ImmutableQueue<T>(); }

        /** Returns true if the queue contains no elements. */
        [[nodiscard]] bool getIsEmptyProperty() const { return data_->empty(); }
        /** Gets the number of elements in the queue. */
        [[nodiscard]] int  getCountProperty()   const { return static_cast<int>(data_->size()); }

        /** Returns the element at the front of the queue without removing it. */
        [[nodiscard]] const T& Peek() const {
            if (data_->empty()) throw std::out_of_range("Queue is empty.");
            return data_->front();
        }

        /** Returns a new queue with item added to the back. */
        [[nodiscard]] ImmutableQueue<T> Enqueue(const T& item) const {
            auto d = std::make_shared<DequeT>(*data_);
            d->push_back(item);
            return ImmutableQueue<T>(std::move(d));
        }

        /** Returns a new queue with the front element removed. */
        [[nodiscard]] ImmutableQueue<T> Dequeue() const {
            if (data_->empty()) throw std::out_of_range("Queue is empty.");
            auto d = std::make_shared<DequeT>(*data_);
            d->pop_front();
            return ImmutableQueue<T>(std::move(d));
        }

        /** Returns a new queue with the front element removed, and outputs that element via value. */
        [[nodiscard]] ImmutableQueue<T> Dequeue(T& value) const {
            if (data_->empty()) throw std::out_of_range("Queue is empty.");
            value = data_->front();
            auto d = std::make_shared<DequeT>(*data_);
            d->pop_front();
            return ImmutableQueue<T>(std::move(d));
        }

        /** Returns an empty ImmutableQueue. */
        [[nodiscard]] ImmutableQueue<T> Clear() const { return Empty(); }

        /** Returns a const iterator to the front of the queue. */
        auto begin() const { return data_->begin(); }
        /** Returns a const iterator past the back of the queue. */
        auto end()   const { return data_->end(); }
    };

} // namespace System::Collections::Immutable
