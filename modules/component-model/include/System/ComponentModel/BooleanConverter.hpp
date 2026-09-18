// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <string>
#include <vector>
#include "System/Boolean.hpp"
#include "System/ComponentModel/TypeConverter.hpp"
#include "System/ComponentModel/detail/ObjectText.hpp"
#include "System/FormatException.hpp"
#include "System/Type.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a type converter to convert Boolean objects to and from various other
     *        representations.
     *
     * C++ counterpart of .NET System.ComponentModel.BooleanConverter: parses `"true"`/`"false"`
     * case-insensitively (after trimming), offers `true` and `false` as its exclusive standard
     * values, and formats through the base converter (`"True"`/`"False"`).
     */
    class BooleanConverter : public TypeConverter {
    public:
        using TypeConverter::CanConvertFrom;
        using TypeConverter::ConvertFrom;
        using TypeConverter::GetStandardValues;
        using TypeConverter::GetStandardValuesExclusive;
        using TypeConverter::GetStandardValuesSupported;

        /** @brief Initializes a new instance of the BooleanConverter class. */
        BooleanConverter() = default;

        /**
         * @brief Gets a value indicating whether this converter can convert an object in the
         *        given source type to a Boolean object using the specified context.
         *
         * C++ counterpart of .NET BooleanConverter.CanConvertFrom: strings, plus the base.
         */
        [[nodiscard]] bool CanConvertFrom(ITypeDescriptorContext* context, const System::Type& sourceType) const override {
            return detail::ObjectText::IsStringType(sourceType) ||
                   TypeConverter::CanConvertFrom(context, sourceType);
        }

        /**
         * @brief Converts the given value object to a Boolean object.
         *
         * C++ counterpart of .NET BooleanConverter.ConvertFrom.
         * @throws System::FormatException reading `"<text> is not a valid value for Boolean."`
         *         when the text is neither `true` nor `false`.
         */
        [[nodiscard]] std::any ConvertFrom(ITypeDescriptorContext* context,
                                           const System::Globalization::CultureInfo* culture,
                                           const std::any& value) const override {
            if (auto text = detail::ObjectText::AsString(value)) {
                bool result = false;
                if (System::Boolean::TryParse(*text, result)) return std::any(result);
                throw System::FormatException(*text + " is not a valid value for Boolean.");
            }
            return TypeConverter::ConvertFrom(context, culture, value);
        }

        /**
         * @brief Gets a collection of standard values for the Boolean data type: `true`, `false`.
         *
         * C++ counterpart of .NET BooleanConverter.GetStandardValues.
         */
        [[nodiscard]] StandardValuesCollection GetStandardValues(ITypeDescriptorContext* context) const override {
            (void)context;
            return StandardValuesCollection(std::vector<std::any>{std::any(true), std::any(false)});
        }

        /**
         * @brief Gets a value indicating whether the list of standard values returned from
         *        GetStandardValues is an exclusive list: it is.
         *
         * C++ counterpart of .NET BooleanConverter.GetStandardValuesExclusive.
         */
        [[nodiscard]] bool GetStandardValuesExclusive(ITypeDescriptorContext* context) const override {
            (void)context;
            return true;
        }

        /**
         * @brief Gets a value indicating whether this object supports a standard set of values
         *        that can be picked from a list: it does.
         *
         * C++ counterpart of .NET BooleanConverter.GetStandardValuesSupported.
         */
        [[nodiscard]] bool GetStandardValuesSupported(ITypeDescriptorContext* context) const override {
            (void)context;
            return true;
        }
    };

} // namespace System::ComponentModel
