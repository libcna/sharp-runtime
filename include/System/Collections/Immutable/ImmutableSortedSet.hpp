// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <set>
#include <vector>

namespace System::Collections::Immutable {

    /** An immutable sorted set with no duplicate elements. */
    template<typename T>
    class ImmutableSortedSet {
        using SetT = std::set<T>;
        std::shared_ptr<const SetT> data_;

        explicit ImmutableSortedSet(std::shared_ptr<const SetT> data) : data_(std::move(data)) {}

    public:
        /** Default-constructs an empty ImmutableSortedSet. */
        ImmutableSortedSet() : data_(std::make_shared<SetT>()) {}

        /** Returns an empty ImmutableSortedSet. */
        static ImmutableSortedSet<T> Empty() { return ImmutableSortedSet<T>(); }

        /** Creates an ImmutableSortedSet from an initializer list. */
        static ImmutableSortedSet<T> Create(std::initializer_list<T> items) {
            return ImmutableSortedSet<T>(std::make_shared<SetT>(items));
        }

        /** Gets the number of elements in the set. */
        [[nodiscard]] int  getCountProperty()  const { return static_cast<int>(data_->size()); }
        /** Returns true if the set contains no elements. */
        [[nodiscard]] bool getIsEmptyProperty() const { return data_->empty(); }

        /** Gets the smallest element in the set. */
        [[nodiscard]] const T& getMinProperty() const { return *data_->begin(); }
        /** Gets the largest element in the set. */
        [[nodiscard]] const T& getMaxProperty() const { return *data_->rbegin(); }

        /** Returns true if the set contains the specified item. */
        [[nodiscard]] bool Contains(const T& item) const {
            return data_->find(item) != data_->end();
        }

        /** Returns a new set with item added. */
        ImmutableSortedSet<T> Add(const T& item) const {
            auto s = std::make_shared<SetT>(*data_);
            s->insert(item);
            return ImmutableSortedSet<T>(std::move(s));
        }

        /** Returns a new set with item removed. */
        ImmutableSortedSet<T> Remove(const T& item) const {
            auto s = std::make_shared<SetT>(*data_);
            s->erase(item);
            return ImmutableSortedSet<T>(std::move(s));
        }

        /** Returns an empty ImmutableSortedSet. */
        ImmutableSortedSet<T> Clear() const { return Empty(); }

        /** Returns a new set containing all elements from both this set and other. */
        ImmutableSortedSet<T> Union(const ImmutableSortedSet<T>& other) const {
            auto s = std::make_shared<SetT>(*data_);
            for (auto& x : *other.data_) s->insert(x);
            return ImmutableSortedSet<T>(std::move(s));
        }

        /** Returns a new set containing only elements present in both sets. */
        ImmutableSortedSet<T> Intersect(const ImmutableSortedSet<T>& other) const {
            auto s = std::make_shared<SetT>();
            for (auto& x : *data_) if (other.Contains(x)) s->insert(x);
            return ImmutableSortedSet<T>(std::move(s));
        }

        /** Returns a new set containing elements in this set that are not in other. */
        ImmutableSortedSet<T> Except(const ImmutableSortedSet<T>& other) const {
            auto s = std::make_shared<SetT>();
            for (auto& x : *data_) if (!other.Contains(x)) s->insert(x);
            return ImmutableSortedSet<T>(std::move(s));
        }

        /** Returns true if every element of this set is contained in other. */
        [[nodiscard]] bool IsSubsetOf(const ImmutableSortedSet<T>& other) const {
            for (auto& x : *data_) if (!other.Contains(x)) return false;
            return true;
        }

        /** Returns a const iterator to the beginning of the set. */
        auto begin() const { return data_->begin(); }
        /** Returns a const iterator past the end of the set. */
        auto end()   const { return data_->end(); }
    };

} // namespace System::Collections::Immutable
