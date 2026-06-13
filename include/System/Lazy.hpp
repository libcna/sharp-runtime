// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace System {

    /// <summary>
    /// Provides thread-safe lazy initialization (ExecutionAndPublication mode by default).
    ///
    /// The value is created exactly once on the first access to getValueProperty() or Value().
    /// </summary>
    template<typename T>
    class Lazy {
        mutable std::once_flag flag_;
        mutable std::optional<T> value_;
        std::function<T()> factory_;
        mutable bool isValueCreated_ = false;

    public:
        /// Constructs a Lazy&lt;T&gt; that uses the default constructor T() as its factory.
        Lazy() : factory_([]() { return T{}; }) {}

        /// Constructs a Lazy&lt;T&gt; with the specified value factory function.
        /// @param valueFactory Callable that produces the value on first access.
        explicit Lazy(std::function<T()> valueFactory)
            : factory_(std::move(valueFactory)) {}

        /// Gets the lazily initialized value, invoking the factory on the first call.
        /// Thread-safe: the factory is called at most once.
        /// @return Const reference to the initialized value.
        [[nodiscard]] const T& getValueProperty() const {
            std::call_once(flag_, [this]() {
                value_ = factory_();
                isValueCreated_ = true;
            });
            return *value_;
        }

        /// Returns true if the value has already been created.
        [[nodiscard]] bool getIsValueCreatedProperty() const noexcept {
            return isValueCreated_;
        }

        /// Gets the lazily initialized value; equivalent to getValueProperty().
        [[nodiscard]] const T& Value() const { return getValueProperty(); }
    };

} // namespace System
