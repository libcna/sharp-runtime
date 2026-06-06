// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <vector>
#include <algorithm>
#include <stdexcept>

#include "System/Collections/Generic/IList.hpp"

namespace System::Collections::Generic
{
    /**
     * @brief A strongly typed list of objects accessible by index.
     *
     * C++ counterpart of the .NET System.Collections.Generic.List<T> class.
     * Backed by std::vector<T>.
     *
     * @tparam T The type of elements in the list.
     */
    template<typename T>
    class List : public IList<T>
    {
    private:
        std::vector<T> items_;

        class Enumerator : public IEnumerator<T>
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
        List() = default;

        explicit List(const std::vector<T>& source) : items_(source) {}

        [[nodiscard]] int getCountProperty() const override
        {
            return static_cast<int>(items_.size());
        }

        [[nodiscard]] bool getIsReadOnlyProperty() const override { return false; }

        void Add(const T& item) override { items_.push_back(item); }

        void Clear() override { items_.clear(); }

        [[nodiscard]] bool Contains(const T& item) const override
        {
            return std::find(items_.begin(), items_.end(), item) != items_.end();
        }

        bool Remove(const T& item) override
        {
            auto it = std::find(items_.begin(), items_.end(), item);
            if (it == items_.end()) return false;
            items_.erase(it);
            return true;
        }

        [[nodiscard]] const T& operator[](int index) const override
        {
            return items_.at(static_cast<std::size_t>(index));
        }

        T& operator[](int index) override
        {
            return items_.at(static_cast<std::size_t>(index));
        }

        [[nodiscard]] int IndexOf(const T& item) const override
        {
            auto it = std::find(items_.begin(), items_.end(), item);
            if (it == items_.end()) return -1;
            return static_cast<int>(it - items_.begin());
        }

        void Insert(int index, const T& item) override
        {
            items_.insert(items_.begin() + index, item);
        }

        void RemoveAt(int index) override
        {
            items_.erase(items_.begin() + index);
        }

        IEnumerator<T>* GetEnumerator() override
        {
            return new Enumerator(items_);
        }

        System::Collections::IEnumerator* System::Collections::IEnumerable::GetEnumerator() override
        {
            return GetEnumerator();
        }

        /// Returns the underlying std::vector for STL interop.
        [[nodiscard]] const std::vector<T>& ToVector() const { return items_; }
        [[nodiscard]] std::vector<T>& ToVector() { return items_; }

        auto begin() { return items_.begin(); }
        auto end()   { return items_.end(); }
        [[nodiscard]] auto begin() const { return items_.cbegin(); }
        [[nodiscard]] auto end()   const { return items_.cend(); }
    };
}
