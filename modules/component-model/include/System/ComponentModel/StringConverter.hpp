// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <string>
#include "System/ComponentModel/TypeConverter.hpp"
#include "System/ComponentModel/detail/ObjectText.hpp"
#include "System/Type.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a type converter to convert string objects to and from other
     *        representations.
     *
     * C++ counterpart of .NET System.ComponentModel.StringConverter: a string converts to
     * itself, an empty value (.NET's `null`) to the empty string, and everything else goes to the
     * base converter.
     */
    class StringConverter : public TypeConverter {
    public:
        using TypeConverter::CanConvertFrom;
        using TypeConverter::ConvertFrom;

        /** @brief Initializes a new instance of the StringConverter class. */
        StringConverter() = default;

        /**
         * @brief Gets a value indicating whether this converter can convert an object in the
         *        given source type to a string using the specified context.
         *
         * C++ counterpart of .NET StringConverter.CanConvertFrom: strings, plus the base.
         */
        [[nodiscard]] bool CanConvertFrom(ITypeDescriptorContext* context, const System::Type& sourceType) const override {
            return detail::ObjectText::IsStringType(sourceType) ||
                   TypeConverter::CanConvertFrom(context, sourceType);
        }

        /**
         * @brief Converts the specified value object to a string object.
         *
         * C++ counterpart of .NET StringConverter.ConvertFrom.
         */
        [[nodiscard]] std::any ConvertFrom(ITypeDescriptorContext* context,
                                           const System::Globalization::CultureInfo* culture,
                                           const std::any& value) const override {
            if (auto text = detail::ObjectText::AsString(value)) return std::any(*text);
            if (!value.has_value()) return std::any(std::string{});
            return TypeConverter::ConvertFrom(context, culture, value);
        }
    };

} // namespace System::ComponentModel
