// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <optional>
#include <stdexcept>

namespace System
{
    /**
     * @brief Represents a value type that can be assigned null.
     *
     * C++ counterpart of the .NET System.Nullable<T> struct.
     * Backed by std::optional<T>.
     *
     * @tparam T The underlying value type.
     */
    template<typename T>
    class Nullable
    {
    private:
        std::optional<T> value_;

    public:
        Nullable() = default;

        Nullable(const T& value) : value_(value) {}   // NOLINT(google-explicit-constructor)
        Nullable(T&& value) : value_(std::move(value)) {}  // NOLINT(google-explicit-constructor)

        /// Returns true if this instance holds a value.
        [[nodiscard]] bool getHasValueProperty() const { return value_.has_value(); }

        /// Returns the held value. Throws if HasValue is false.
        [[nodiscard]] const T& getValueProperty() const
        {
            if (!value_) throw std::runtime_error("Nullable object must have a value.");
            return *value_;
        }

        /// Returns the held value, or a default if no value is present.
        [[nodiscard]] T GetValueOrDefault() const
        {
            return value_.value_or(T{});
        }

        [[nodiscard]] T GetValueOrDefault(const T& defaultValue) const
        {
            return value_.value_or(defaultValue);
        }

        explicit operator bool() const { return value_.has_value(); }

        bool operator==(const Nullable<T>& other) const { return value_ == other.value_; }
        bool operator!=(const Nullable<T>& other) const { return value_ != other.value_; }

        bool operator==(std::nullopt_t) const { return !value_.has_value(); }
        bool operator!=(std::nullopt_t) const { return value_.has_value(); }
    };
}
