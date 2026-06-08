// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <vector>
#include <algorithm>
#include <stdexcept>

#include "System/Collections/Generic/IList.hpp"

namespace System::Collections::ObjectModel
{
    /**
     * @brief Provides the base class for a generic collection.
     *
     * C++ counterpart of the .NET System.Collections.ObjectModel.Collection<T> class.
     * Backed by std::vector<T>.
     *
     * @tparam T The type of elements in the collection.
     */
    template<typename T>
    class Collection : public Generic::IList<T>
    {
    private:
        class Enumerator : public Generic::IEnumerator<T>
        {
            const std::vector<T>& items_;
            int index_ = -1;
        public:
            explicit Enumerator(const std::vector<T>& items) : items_(items) {}
            bool MoveNext() override { return ++index_ < static_cast<int>(items_.size()); }
            void Reset() override { index_ = -1; }
            [[nodiscard]] const T& Current() const override { return items_[index_]; }
        };

    protected:
        std::vector<T> items_;

        virtual void InsertItem(int index, const T& item)
        {
            items_.insert(items_.begin() + index, item);
        }

        virtual void RemoveItem(int index)
        {
            items_.erase(items_.begin() + index);
        }

        virtual void ClearItems()
        {
            items_.clear();
        }

        virtual void SetItem(int index, const T& item)
        {
            items_[index] = item;
        }

    public:
        Collection() = default;
        ~Collection() override = default;

        [[nodiscard]] int getCountProperty() const override
        {
            return static_cast<int>(items_.size());
        }

        [[nodiscard]] bool getIsReadOnlyProperty() const override { return false; }

        void Add(const T& item) override
        {
            InsertItem(static_cast<int>(items_.size()), item);
        }

        void Clear() override { ClearItems(); }

        [[nodiscard]] bool Contains(const T& item) const override
        {
            return std::find(items_.begin(), items_.end(), item) != items_.end();
        }

        bool Remove(const T& item) override
        {
            auto it = std::find(items_.begin(), items_.end(), item);
            if (it == items_.end()) return false;
            RemoveItem(static_cast<int>(it - items_.begin()));
            return true;
        }

        [[nodiscard]] const T& operator[](int index) const override { return items_.at(index); }
        T& operator[](int index) override { return items_.at(index); }

        [[nodiscard]] int IndexOf(const T& item) const override
        {
            auto it = std::find(items_.begin(), items_.end(), item);
            if (it == items_.end()) return -1;
            return static_cast<int>(it - items_.begin());
        }

        void Insert(int index, const T& item) override { InsertItem(index, item); }

        void RemoveAt(int index) override { RemoveItem(index); }

        Generic::IEnumerator<T>* GetEnumerator() override
        {
            return new Enumerator(items_);
        }

        auto begin()        { return items_.begin(); }
        auto end()          { return items_.end(); }
        [[nodiscard]] auto begin() const { return items_.cbegin(); }
        [[nodiscard]] auto end()   const { return items_.cend(); }
    };
}
