// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <unordered_set>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::ObjectModel {

    template<typename T>
    class ReadOnlySet {
        std::shared_ptr<std::unordered_set<T>> set_;

    public:
        explicit ReadOnlySet(std::shared_ptr<std::unordered_set<T>> set)
            : set_(std::move(set)) {}

        [[nodiscard]] SharpRuntime::intcs getCountProperty() const {
            return static_cast<SharpRuntime::intcs>(set_->size());
        }

        [[nodiscard]] bool Contains(const T& item) const { return set_->count(item) > 0; }

        // Range-for support
        auto begin() const { return set_->begin(); }
        auto end()   const { return set_->end(); }
    };

} // namespace System::Collections::ObjectModel
