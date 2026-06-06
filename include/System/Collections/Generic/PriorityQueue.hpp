// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace System::Collections::Generic {

    // Min-heap priority queue: lower priority value = higher priority (same as .NET).
    template<typename TElement, typename TPriority>
    class PriorityQueue {
        struct Entry {
            TElement element;
            TPriority priority;
            bool operator>(const Entry& o) const { return priority > o.priority; }
        };

        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> heap_;
        int count_ = 0;

    public:
        PriorityQueue() = default;

        [[nodiscard]] int getCountProperty() const { return count_; }

        void Enqueue(const TElement& element, const TPriority& priority) {
            heap_.push({element, priority});
            ++count_;
        }

        [[nodiscard]] TElement Dequeue() {
            if (heap_.empty()) throw std::invalid_argument("Queue is empty.");
            TElement el = heap_.top().element;
            heap_.pop();
            --count_;
            return el;
        }

        [[nodiscard]] TElement Peek() const {
            if (heap_.empty()) throw std::invalid_argument("Queue is empty.");
            return heap_.top().element;
        }

        [[nodiscard]] bool TryDequeue(TElement& element, TPriority& priority) {
            if (heap_.empty()) return false;
            element  = heap_.top().element;
            priority = heap_.top().priority;
            heap_.pop();
            --count_;
            return true;
        }

        [[nodiscard]] bool TryPeek(TElement& element, TPriority& priority) const {
            if (heap_.empty()) return false;
            element  = heap_.top().element;
            priority = heap_.top().priority;
            return true;
        }

        void EnqueueRange(const std::vector<std::pair<TElement, TPriority>>& items) {
            for (auto& p : items) Enqueue(p.first, p.second);
        }

        void Clear() {
            while (!heap_.empty()) heap_.pop();
            count_ = 0;
        }
    };

} // namespace System::Collections::Generic
