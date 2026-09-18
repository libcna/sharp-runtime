// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <optional>
#include <string>
#include <utility>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Attribute.hpp"
#include "System/Type.hpp"

namespace System::ComponentModel {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /** Specifies the default value for a property. */
    class DefaultValueAttribute : public System::Attribute {
        std::any value_;

        static bool anyEquals(const std::any& left, const std::any& right) {
            if (!left.has_value() || !right.has_value()) {
                return left.has_value() == right.has_value();
            }
            if (left.type() != right.type()) {
                return false;
            }

            if (const auto* value = std::any_cast<bool>(&left))
                return *value == *std::any_cast<bool>(&right);
            if (const auto* value = std::any_cast<char>(&left))
                return *value == *std::any_cast<char>(&right);
            if (const auto* value = std::any_cast<SharpRuntime::bytecs>(&left))
                return *value == *std::any_cast<SharpRuntime::bytecs>(&right);
            if (const auto* value = std::any_cast<SharpRuntime::sbytecs>(&left))
                return *value == *std::any_cast<SharpRuntime::sbytecs>(&right);
            if (const auto* value = std::any_cast<SharpRuntime::shortcs>(&left))
                return *value == *std::any_cast<SharpRuntime::shortcs>(&right);
            if (const auto* value = std::any_cast<SharpRuntime::ushortcs>(&left))
                return *value == *std::any_cast<SharpRuntime::ushortcs>(&right);
            if (const auto* value = std::any_cast<SharpRuntime::intcs>(&left))
                return *value == *std::any_cast<SharpRuntime::intcs>(&right);
            if (const auto* value = std::any_cast<SharpRuntime::uintcs>(&left))
                return *value == *std::any_cast<SharpRuntime::uintcs>(&right);
            if (const auto* value = std::any_cast<SharpRuntime::longcs>(&left))
                return *value == *std::any_cast<SharpRuntime::longcs>(&right);
            if (const auto* value = std::any_cast<SharpRuntime::ulongcs>(&left))
                return *value == *std::any_cast<SharpRuntime::ulongcs>(&right);
            if (const auto* value = std::any_cast<float>(&left))
                return *value == *std::any_cast<float>(&right);
            if (const auto* value = std::any_cast<double>(&left))
                return *value == *std::any_cast<double>(&right);
            if (const auto* value = std::any_cast<std::string>(&left))
                return *value == *std::any_cast<std::string>(&right);

            // std::any has no general equality operation. Avoid claiming equal values for
            // arbitrary erased C++ objects whose equality contract cannot be inspected.
            return false;
        }

    public:
        /** @param v Default value as bool. */
        explicit DefaultValueAttribute(bool v)        : value_(v) {}
        /** @param v Default value as byte. */
        explicit DefaultValueAttribute(SharpRuntime::bytecs v) : value_(v) {}
        /** @param v Default value as signed byte. */
        explicit DefaultValueAttribute(SharpRuntime::sbytecs v) : value_(v) {}
        /** @param v Default value as short. */
        explicit DefaultValueAttribute(SharpRuntime::shortcs v) : value_(v) {}
        /** @param v Default value as unsigned short. */
        explicit DefaultValueAttribute(SharpRuntime::ushortcs v) : value_(v) {}
        /** @param v Default value as int. */
        explicit DefaultValueAttribute(intcs v)       : value_(v) {}
        /** @param v Default value as unsigned int. */
        explicit DefaultValueAttribute(SharpRuntime::uintcs v) : value_(v) {}
        /** @param v Default value as long. */
        explicit DefaultValueAttribute(longcs v)      : value_(v) {}
        /** @param v Default value as unsigned long. */
        explicit DefaultValueAttribute(SharpRuntime::ulongcs v) : value_(v) {}
        /** @param v Default value as double. */
        explicit DefaultValueAttribute(double v)      : value_(v) {}
        /** @param v Default value as float. */
        explicit DefaultValueAttribute(float v)       : value_(v) {}
        /** @param v Default value as char. */
        explicit DefaultValueAttribute(char v)        : value_(v) {}
        /** @param v Default value as string. */
        explicit DefaultValueAttribute(std::string v) : value_(std::move(v)) {}
        /** @param v Default value as a nullable string. */
        explicit DefaultValueAttribute(std::optional<std::string> v)
            : value_(v ? std::any(std::move(*v)) : std::any{}) {}
        /** @param v Default value as a type-erased std::any. */
        explicit DefaultValueAttribute(std::any v) : value_(std::move(v)) {}

        /**
         * @brief Initializes a new instance, converting the specified value to the specified
         *        type, and using an invariant culture as the translation context.
         *
         * C++ counterpart of .NET DefaultValueAttribute(Type, string): the value is
         * `TypeDescriptor::GetConverter(type)->ConvertFromInvariantString(value)`. As in .NET, a
         * conversion that fails leaves the attribute holding an empty value rather than
         * throwing (`DefaultValueAttribute.cs` swallows the exception), and a null string (an
         * empty optional) stores an empty value without consulting the converter.
         * @param type The type the value is converted to.
         * @param value The invariant text of the value, or `std::nullopt` for .NET's `null`.
         */
        DefaultValueAttribute(const System::Type& type, std::optional<std::string> value);

        /** @return The default value as a type-erased std::any. */
        [[nodiscard]] virtual const std::any& getValueProperty() const noexcept { return value_; }

        /** Compares supported default values by their stored value and type. */
        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const DefaultValueAttribute*>(&other);
            return attribute != nullptr && anyEquals(value_, attribute->value_);
        }

    protected:
        /** Sets the stored default value for a derived attribute. */
        void SetValue(std::any value) { value_ = std::move(value); }
    };

} // namespace System::ComponentModel
