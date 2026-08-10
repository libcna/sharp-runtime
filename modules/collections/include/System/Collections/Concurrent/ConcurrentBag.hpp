// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <mutex>
#include <utility>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Concurrent/IProducerConsumerCollection.hpp"
#include "System/Collections/Generic/IEnumerator.hpp"

namespace System::Collections::Concurrent {

    using SharpRuntime::intcs;

    /**
     * Represents a thread-safe unordered collection of objects.
     *
     * C++ counterpart of .NET System.Collections.Concurrent.ConcurrentBag<T>.
     *
     * @note .NET uses per-thread work-stealing queues. This implementation serializes operations
     * with one mutex and removes the most recently added item. ConcurrentBag deliberately makes
     * no ordering guarantee, so this preserves its public observable contract at lower complexity.
     */
    template<typename T>
    class ConcurrentBag : public IProducerConsumerCollection<T> {
        mutable std::mutex mutex_;
        std::vector<T> items_;

        class SnapshotEnumerator final : public System::Collections::Generic::IEnumerator<T> {
            std::vector<T> items_;
            intcs index_ = -1;
            System::Collections::detail::EnumeratorState state_;

        public:
            explicit SnapshotEnumerator(std::vector<T> items) : items_(std::move(items)) {}

            bool MoveNext() override {
                if (state_.isAfterLast()) return false;

                const intcs next = index_ + 1;
                if (next < static_cast<intcs>(items_.size())) {
                    index_ = next;
                    state_.setCurrent();
                    return true;
                }

                index_ = static_cast<intcs>(items_.size());
                state_.setAfterLast();
                return false;
            }
            void Reset() override {
                index_ = -1;
                state_.Reset();
            }
            [[nodiscard]] const T& Current() const override {
                state_.requireCurrent();
                return items_[static_cast<std::size_t>(index_)];
            }
        };

    public:
        /** Initializes an empty ConcurrentBag<T>. */
        ConcurrentBag() = default;

        /** Initializes a new bag containing copies of the elements in @p collection. */
        explicit ConcurrentBag(const std::vector<T>& collection) : items_(collection) {}

        /** Initializes a new bag containing copies of the elements in @p collection. */
        explicit ConcurrentBag(System::Collections::Generic::IEnumerable<T>& collection) {
            auto* enumerator = collection.GetEnumerator();
            if (enumerator != nullptr) {
                while (enumerator->MoveNext()) {
                    items_.push_back(enumerator->Current());
                }
                delete enumerator;
            }
        }

        /** Returns the number of items currently in the bag. */
        [[nodiscard]] intcs getCountProperty() const override {
            std::lock_guard<std::mutex> lock(mutex_);
            return static_cast<intcs>(items_.size());
        }

        /** Returns whether the bag is empty. */
        [[nodiscard]] bool getIsEmptyProperty() const override {
            std::lock_guard<std::mutex> lock(mutex_);
            return items_.empty();
        }

        /** Adds an item to the bag. */
        void Add(const T& item) {
            std::lock_guard<std::mutex> lock(mutex_);
            items_.push_back(item);
        }

        /** Attempts to add an item to the bag; always succeeds. */
        bool TryAdd(const T& item) override {
            Add(item);
            return true;
        }

        /** Attempts to remove an item from the bag. */
        bool TryTake(T& result) override {
            std::lock_guard<std::mutex> lock(mutex_);
            if (items_.empty()) {
                return false;
            }
            result = std::move(items_.back());
            items_.pop_back();
            return true;
        }

        /** Attempts to retrieve an item without removing it. */
        bool TryPeek(T& result) const {
            std::lock_guard<std::mutex> lock(mutex_);
            if (items_.empty()) {
                return false;
            }
            result = items_.back();
            return true;
        }

        /** Removes all items from the bag. */
        void Clear() {
            std::lock_guard<std::mutex> lock(mutex_);
            items_.clear();
        }

        /** Returns a point-in-time snapshot of the bag's contents. */
        [[nodiscard]] std::vector<T> ToArray() const override {
            std::lock_guard<std::mutex> lock(mutex_);
            return items_;
        }

        /**
         * Copies a point-in-time snapshot into @p array starting at @p index.
         *
         * @throws System::ArgumentOutOfRangeException if @p index is negative.
         * @throws System::ArgumentException if the destination does not have enough room.
         */
        void CopyTo(std::vector<T>& array, intcs index) override {
            if (index < 0) {
                throw System::ArgumentOutOfRangeException("index");
            }
            const std::vector<T> snapshot = ToArray();
            if (static_cast<std::size_t>(index) + snapshot.size() > array.size()) {
                throw System::ArgumentException("Destination array was not long enough.", "index");
            }
            for (std::size_t i = 0; i < snapshot.size(); ++i) {
                array[static_cast<std::size_t>(index) + i] = snapshot[i];
            }
        }

        /** Returns an enumerator over a point-in-time snapshot of the bag. */
        [[nodiscard]] System::Collections::Generic::IEnumerator<T>* GetEnumerator() override {
            return new SnapshotEnumerator(ToArray());
        }
    };

} // namespace System::Collections::Concurrent
