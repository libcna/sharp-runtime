// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <unordered_set>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::ObjectModel {

    /// A read-only wrapper around an unordered set.
    template<typename T>
    class ReadOnlySet {
        std::shared_ptr<std::unordered_set<T>> set_;

    public:
        /// Constructs a ReadOnlySet that wraps the given shared set.
        explicit ReadOnlySet(std::shared_ptr<std::unordered_set<T>> set)
            : set_(std::move(set)) {}

        /// Gets the number of elements in the set.
        [[nodiscard]] SharpRuntime::intcs getCountProperty() const {
            return static_cast<SharpRuntime::intcs>(set_->size());
        }

        /// Returns true if the set contains the specified item.
        [[nodiscard]] bool Contains(const T& item) const { return set_->count(item) > 0; }

        /// Returns a const iterator to the beginning of the set.
        auto begin() const { return set_->begin(); }
        /// Returns a const iterator past the end of the set.
        auto end()   const { return set_->end(); }
    };

} // namespace System::Collections::ObjectModel
