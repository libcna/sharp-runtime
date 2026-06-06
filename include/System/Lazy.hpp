// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace System {

    // Thread-safe lazy initialization (ExecutionAndPublication mode by default).
    template<typename T>
    class Lazy {
        mutable std::once_flag flag_;
        mutable std::optional<T> value_;
        std::function<T()> factory_;
        bool isValueCreated_ = false;

    public:
        // Default-constructible T: uses T() as factory.
        Lazy() : factory_([]() { return T{}; }) {}

        explicit Lazy(std::function<T()> valueFactory)
            : factory_(std::move(valueFactory)) {}

        [[nodiscard]] const T& getValueProperty() const {
            std::call_once(flag_, [this]() {
                value_ = factory_();
                isValueCreated_ = true;
            });
            return *value_;
        }

        [[nodiscard]] bool getIsValueCreatedProperty() const noexcept {
            return isValueCreated_;
        }

        [[nodiscard]] const T& Value() const { return getValueProperty(); }
    };

} // namespace System
