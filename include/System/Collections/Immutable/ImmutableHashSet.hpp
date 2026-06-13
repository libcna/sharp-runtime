// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <unordered_set>
#include <vector>

namespace System::Collections::Immutable {

    /// An immutable, unordered hash set with no duplicate elements.
    template<typename T>
    class ImmutableHashSet {
        using SetT = std::unordered_set<T>;
        std::shared_ptr<const SetT> data_;

        explicit ImmutableHashSet(std::shared_ptr<const SetT> data) : data_(std::move(data)) {}

    public:
        /// Default-constructs an empty ImmutableHashSet.
        ImmutableHashSet() : data_(std::make_shared<SetT>()) {}

        /// Returns an empty ImmutableHashSet.
        static ImmutableHashSet<T> Empty() { return ImmutableHashSet<T>(); }

        /// Creates an ImmutableHashSet from an initializer list.
        static ImmutableHashSet<T> Create(std::initializer_list<T> items) {
            return ImmutableHashSet<T>(std::make_shared<SetT>(items));
        }

        /// Gets the number of elements in the set.
        [[nodiscard]] int  getCountProperty()  const { return static_cast<int>(data_->size()); }
        /// Returns true if the set contains no elements.
        [[nodiscard]] bool getIsEmptyProperty() const { return data_->empty(); }

        /// Returns true if the set contains the specified item.
        [[nodiscard]] bool Contains(const T& item) const {
            return data_->find(item) != data_->end();
        }

        /// Returns a new set with item added.
        ImmutableHashSet<T> Add(const T& item) const {
            auto s = std::make_shared<SetT>(*data_);
            s->insert(item);
            return ImmutableHashSet<T>(std::move(s));
        }

        /// Returns a new set with item removed.
        ImmutableHashSet<T> Remove(const T& item) const {
            auto s = std::make_shared<SetT>(*data_);
            s->erase(item);
            return ImmutableHashSet<T>(std::move(s));
        }

        /// Returns an empty ImmutableHashSet.
        ImmutableHashSet<T> Clear() const { return Empty(); }

        /// Returns a new set containing all elements from both this set and other.
        ImmutableHashSet<T> Union(const ImmutableHashSet<T>& other) const {
            auto s = std::make_shared<SetT>(*data_);
            for (auto& x : *other.data_) s->insert(x);
            return ImmutableHashSet<T>(std::move(s));
        }

        /// Returns a new set containing only elements present in both sets.
        ImmutableHashSet<T> Intersect(const ImmutableHashSet<T>& other) const {
            auto s = std::make_shared<SetT>();
            for (auto& x : *data_) if (other.Contains(x)) s->insert(x);
            return ImmutableHashSet<T>(std::move(s));
        }

        /// Returns a new set containing elements in this set that are not in other.
        ImmutableHashSet<T> Except(const ImmutableHashSet<T>& other) const {
            auto s = std::make_shared<SetT>();
            for (auto& x : *data_) if (!other.Contains(x)) s->insert(x);
            return ImmutableHashSet<T>(std::move(s));
        }

        /// Returns true if every element of this set is contained in other.
        [[nodiscard]] bool IsSubsetOf(const ImmutableHashSet<T>& other) const {
            for (auto& x : *data_) if (!other.Contains(x)) return false;
            return true;
        }

        /// Returns a const iterator to the beginning of the set.
        auto begin() const { return data_->begin(); }
        /// Returns a const iterator past the end of the set.
        auto end()   const { return data_->end(); }
    };

} // namespace System::Collections::Immutable
