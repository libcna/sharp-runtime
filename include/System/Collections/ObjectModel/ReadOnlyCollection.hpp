#pragma once

#include <vector>
#include <stdexcept>

#include "System/Collections/Generic/IList.hpp"

namespace System::Collections::ObjectModel
{
    /**
     * @brief Provides a read-only wrapper around a generic list.
     *
     * C++ counterpart of the .NET System.Collections.ObjectModel.ReadOnlyCollection<T> class.
     *
     * @tparam T The type of elements in the collection.
     */
    template<typename T>
    class ReadOnlyCollection : public Generic::IList<T>
    {
    private:
        std::vector<T> items_;

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

    public:
        ReadOnlyCollection() = default;

        explicit ReadOnlyCollection(const std::vector<T>& source) : items_(source) {}
        explicit ReadOnlyCollection(std::vector<T>&& source) : items_(std::move(source)) {}

        ~ReadOnlyCollection() override = default;

        [[nodiscard]] int getCountProperty() const override
        {
            return static_cast<int>(items_.size());
        }

        [[nodiscard]] bool getIsReadOnlyProperty() const override { return true; }

        [[nodiscard]] const T& operator[](int index) const override
        {
            return items_.at(static_cast<std::size_t>(index));
        }

        T& operator[](int index) override
        {
            throw std::runtime_error("Collection is read-only.");
        }

        [[nodiscard]] int IndexOf(const T& item) const override
        {
            for (int i = 0; i < static_cast<int>(items_.size()); ++i)
                if (items_[i] == item) return i;
            return -1;
        }

        [[nodiscard]] bool Contains(const T& item) const override
        {
            return IndexOf(item) >= 0;
        }

        void Add(const T&) override
        {
            throw std::runtime_error("Collection is read-only.");
        }

        void Clear() override
        {
            throw std::runtime_error("Collection is read-only.");
        }

        bool Remove(const T&) override
        {
            throw std::runtime_error("Collection is read-only.");
        }

        void Insert(int, const T&) override
        {
            throw std::runtime_error("Collection is read-only.");
        }

        void RemoveAt(int) override
        {
            throw std::runtime_error("Collection is read-only.");
        }

        Generic::IEnumerator<T>* GetEnumerator() override
        {
            return new Enumerator(items_);
        }

        System::Collections::IEnumerator* System::Collections::IEnumerable::GetEnumerator() override
        {
            return GetEnumerator();
        }

        auto begin()        { return items_.begin(); }
        auto end()          { return items_.end(); }
        [[nodiscard]] auto begin() const { return items_.cbegin(); }
        [[nodiscard]] auto end()   const { return items_.cend(); }
    };
}
