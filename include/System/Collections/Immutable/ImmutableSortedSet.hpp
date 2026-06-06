// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <set>
#include <vector>

namespace System::Collections::Immutable {

    template<typename T>
    class ImmutableSortedSet {
        using SetT = std::set<T>;
        std::shared_ptr<const SetT> data_;

        explicit ImmutableSortedSet(std::shared_ptr<const SetT> data) : data_(std::move(data)) {}

    public:
        ImmutableSortedSet() : data_(std::make_shared<SetT>()) {}

        static ImmutableSortedSet<T> Empty() { return ImmutableSortedSet<T>(); }

        static ImmutableSortedSet<T> Create(std::initializer_list<T> items) {
            return ImmutableSortedSet<T>(std::make_shared<SetT>(items));
        }

        [[nodiscard]] int  getCountProperty()  const { return static_cast<int>(data_->size()); }
        [[nodiscard]] bool getIsEmptyProperty() const { return data_->empty(); }

        [[nodiscard]] const T& getMinProperty() const { return *data_->begin(); }
        [[nodiscard]] const T& getMaxProperty() const { return *data_->rbegin(); }

        [[nodiscard]] bool Contains(const T& item) const {
            return data_->find(item) != data_->end();
        }

        ImmutableSortedSet<T> Add(const T& item) const {
            auto s = std::make_shared<SetT>(*data_);
            s->insert(item);
            return ImmutableSortedSet<T>(std::move(s));
        }

        ImmutableSortedSet<T> Remove(const T& item) const {
            auto s = std::make_shared<SetT>(*data_);
            s->erase(item);
            return ImmutableSortedSet<T>(std::move(s));
        }

        ImmutableSortedSet<T> Clear() const { return Empty(); }

        ImmutableSortedSet<T> Union(const ImmutableSortedSet<T>& other) const {
            auto s = std::make_shared<SetT>(*data_);
            for (auto& x : *other.data_) s->insert(x);
            return ImmutableSortedSet<T>(std::move(s));
        }

        ImmutableSortedSet<T> Intersect(const ImmutableSortedSet<T>& other) const {
            auto s = std::make_shared<SetT>();
            for (auto& x : *data_) if (other.Contains(x)) s->insert(x);
            return ImmutableSortedSet<T>(std::move(s));
        }

        ImmutableSortedSet<T> Except(const ImmutableSortedSet<T>& other) const {
            auto s = std::make_shared<SetT>();
            for (auto& x : *data_) if (!other.Contains(x)) s->insert(x);
            return ImmutableSortedSet<T>(std::move(s));
        }

        [[nodiscard]] bool IsSubsetOf(const ImmutableSortedSet<T>& other) const {
            for (auto& x : *data_) if (!other.Contains(x)) return false;
            return true;
        }

        auto begin() const { return data_->begin(); }
        auto end()   const { return data_->end(); }
    };

} // namespace System::Collections::Immutable
