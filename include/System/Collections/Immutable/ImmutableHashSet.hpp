// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <unordered_set>
#include <vector>

namespace System::Collections::Immutable {

    template<typename T>
    class ImmutableHashSet {
        using SetT = std::unordered_set<T>;
        std::shared_ptr<const SetT> data_;

        explicit ImmutableHashSet(std::shared_ptr<const SetT> data) : data_(std::move(data)) {}

    public:
        ImmutableHashSet() : data_(std::make_shared<SetT>()) {}

        static ImmutableHashSet<T> Empty() { return ImmutableHashSet<T>(); }

        static ImmutableHashSet<T> Create(std::initializer_list<T> items) {
            return ImmutableHashSet<T>(std::make_shared<SetT>(items));
        }

        [[nodiscard]] int  getCountProperty()  const { return static_cast<int>(data_->size()); }
        [[nodiscard]] bool getIsEmptyProperty() const { return data_->empty(); }

        [[nodiscard]] bool Contains(const T& item) const {
            return data_->find(item) != data_->end();
        }

        ImmutableHashSet<T> Add(const T& item) const {
            auto s = std::make_shared<SetT>(*data_);
            s->insert(item);
            return ImmutableHashSet<T>(std::move(s));
        }

        ImmutableHashSet<T> Remove(const T& item) const {
            auto s = std::make_shared<SetT>(*data_);
            s->erase(item);
            return ImmutableHashSet<T>(std::move(s));
        }

        ImmutableHashSet<T> Clear() const { return Empty(); }

        ImmutableHashSet<T> Union(const ImmutableHashSet<T>& other) const {
            auto s = std::make_shared<SetT>(*data_);
            for (auto& x : *other.data_) s->insert(x);
            return ImmutableHashSet<T>(std::move(s));
        }

        ImmutableHashSet<T> Intersect(const ImmutableHashSet<T>& other) const {
            auto s = std::make_shared<SetT>();
            for (auto& x : *data_) if (other.Contains(x)) s->insert(x);
            return ImmutableHashSet<T>(std::move(s));
        }

        ImmutableHashSet<T> Except(const ImmutableHashSet<T>& other) const {
            auto s = std::make_shared<SetT>();
            for (auto& x : *data_) if (!other.Contains(x)) s->insert(x);
            return ImmutableHashSet<T>(std::move(s));
        }

        [[nodiscard]] bool IsSubsetOf(const ImmutableHashSet<T>& other) const {
            for (auto& x : *data_) if (!other.Contains(x)) return false;
            return true;
        }

        auto begin() const { return data_->begin(); }
        auto end()   const { return data_->end(); }
    };

} // namespace System::Collections::Immutable
