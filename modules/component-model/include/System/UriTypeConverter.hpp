// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "System/ArgumentNullException.hpp"
#include "System/ComponentModel/Design/Serialization/InstanceDescriptor.hpp"
#include "System/ComponentModel/TypeConverter.hpp"
#include "System/ComponentModel/detail/ObjectText.hpp"
#include "System/Reflection/ConstructorInfo.hpp"
#include "System/Type.hpp"
#include "System/Uri.hpp"
#include "System/UriFormatException.hpp"

namespace System {

    /**
     * @brief Converts a String type to a Uri type, and vice versa.
     *
     * C++ counterpart of .NET System.UriTypeConverter, derived from
     * `System::ComponentModel::TypeConverter` as .NET's is. It is owned by the `ComponentModel`
     * component (its include spelling is unchanged) so that `Uri` itself stays a leaf component.
     *
     * The behaviour is `UriTypeConverter.cs`, verified line by line:
     *  - converts from a string (an **empty** string yields .NET's `null`, an empty `std::any`;
     *    a malformed one is left to the `Uri` constructor to reject) and from a `Uri` (a copy
     *    with the same kind); as in .NET, `CanConvertFrom(InstanceDescriptor)` is inherited as
     *    true even though this override rejects such a value;
     *  - converts to a string (`OriginalString`, so a relative URI round-trips), to a `Uri`, and
     *    to an `InstanceDescriptor` naming `Uri(string, UriKind)`;
     *  - `IsValid` accepts a string `Uri` would accept as relative-or-absolute, and any `Uri`.
     *
     * Until this class joined the TypeConverter hierarchy it exposed four C++-shaped members
     * (`CanConvertFrom()`, `ConvertFrom(const std::string&)` returning `std::optional<Uri>`,
     * ...). They are gone; `docs/Migration-UriTypeConverterIsATypeConverter.md` records the
     * replacement spellings.
     */
    class UriTypeConverter : public System::ComponentModel::TypeConverter {
    public:
        using System::ComponentModel::TypeConverter::CanConvertFrom;
        using System::ComponentModel::TypeConverter::CanConvertTo;
        using System::ComponentModel::TypeConverter::ConvertFrom;
        using System::ComponentModel::TypeConverter::ConvertTo;
        using System::ComponentModel::TypeConverter::IsValid;

        /** @brief Initializes a new instance of the UriTypeConverter class. */
        UriTypeConverter() = default;

        /**
         * @brief Returns whether this converter can convert an object of the given type to the
         *        type of this converter, using the specified context.
         *
         * C++ counterpart of .NET UriTypeConverter.CanConvertFrom: strings, `Uri`, and
         * `InstanceDescriptor`.
         */
        [[nodiscard]] bool CanConvertFrom(System::ComponentModel::ITypeDescriptorContext* context,
                                          const System::Type& sourceType) const override {
            (void)context;
            return System::ComponentModel::detail::ObjectText::IsStringType(sourceType) ||
                   sourceType == System::Type::From<Uri>() ||
                   sourceType == System::Type::From<System::ComponentModel::Design::Serialization::InstanceDescriptor>();
        }

        /**
         * @brief Returns whether this converter can convert the object to the specified type,
         *        using the specified context.
         *
         * C++ counterpart of .NET UriTypeConverter.CanConvertTo: `InstanceDescriptor`, string,
         * and `Uri`.
         */
        [[nodiscard]] bool CanConvertTo(System::ComponentModel::ITypeDescriptorContext* context,
                                        const System::Type& destinationType) const override {
            (void)context;
            return destinationType == System::Type::From<System::ComponentModel::Design::Serialization::InstanceDescriptor>() ||
                   destinationType == System::Type::From<std::string>() ||
                   destinationType == System::Type::From<Uri>();
        }

        /**
         * @brief Converts the given object to the type of this converter, using the specified
         *        context and culture information.
         *
         * C++ counterpart of .NET UriTypeConverter.ConvertFrom. An empty string yields an
         * empty value (.NET's `null`); this is the only input short-circuited -- .NET's own
         * comment says a malformed string is left for the `Uri` constructor to reject.
         * @throws System::UriFormatException if the string is malformed.
         * @throws System::NotSupportedException for any other value type.
         */
        [[nodiscard]] std::any ConvertFrom(System::ComponentModel::ITypeDescriptorContext* context,
                                           const System::Globalization::CultureInfo* culture,
                                           const std::any& value) const override {
            if (auto text = asString(value)) {
                if (text->empty()) return {};
                // Let the Uri constructor throw any informative exceptions.
                return std::any(Uri(*text, UriKind::RelativeOrAbsolute));
            }
            if (const auto* uri = std::any_cast<Uri>(&value)) {
                return std::any(Uri(uri->getOriginalStringProperty(), kindOf(*uri)));
            }
            (void)context;
            (void)culture;
            throw GetConvertFromException(value);
        }

        /**
         * @brief Converts a given value object to the specified type, using the specified
         *        context and culture information.
         *
         * C++ counterpart of .NET UriTypeConverter.ConvertTo: a `Uri` to its `OriginalString`,
         * to a `Uri` copy, or to an `InstanceDescriptor` for `Uri(string, UriKind)`.
         * @throws System::ArgumentNullException if @p destinationType is the null type.
         * @throws System::NotSupportedException for any other combination.
         */
        [[nodiscard]] std::any ConvertTo(System::ComponentModel::ITypeDescriptorContext* context,
                                         const System::Globalization::CultureInfo* culture,
                                         const std::any& value,
                                         const System::Type& destinationType) const override {
            if (destinationType == System::Type()) throw System::ArgumentNullException("destinationType");
            if (const auto* uri = std::any_cast<Uri>(&value)) {
                using System::ComponentModel::Design::Serialization::InstanceDescriptor;
                if (destinationType == System::Type::From<InstanceDescriptor>()) {
                    static const std::shared_ptr<const System::Reflection::ConstructorInfo> ctor =
                        System::Reflection::ConstructorInfo::Of<Uri, std::string, UriKind>({"uriString", "uriKind"});
                    return std::any(InstanceDescriptor(ctor, {std::any(uri->getOriginalStringProperty()),
                                                              std::any(kindOf(*uri))}));
                }
                if (destinationType == System::Type::From<std::string>()) {
                    return std::any(uri->getOriginalStringProperty());
                }
                if (destinationType == System::Type::From<Uri>()) {
                    return std::any(Uri(uri->getOriginalStringProperty(), kindOf(*uri)));
                }
            }
            (void)context;
            (void)culture;
            throw GetConvertToException(value, destinationType);
        }

        /**
         * @brief Returns whether the given value object is valid for this type and for the
         *        specified context.
         *
         * C++ counterpart of .NET UriTypeConverter.IsValid: a string `Uri` accepts as
         * relative-or-absolute, or any `Uri`.
         */
        [[nodiscard]] bool IsValid(System::ComponentModel::ITypeDescriptorContext* context,
                                   const std::any& value) const override {
            (void)context;
            if (auto text = asString(value)) {
                try {
                    (void)Uri(*text, UriKind::RelativeOrAbsolute);
                    return true;
                } catch (const System::UriFormatException&) {
                    return false;
                }
            }
            return std::any_cast<Uri>(&value) != nullptr;
        }

    private:
        static std::optional<std::string> asString(const std::any& value) {
            if (const auto* text = std::any_cast<std::string>(&value)) return *text;
            if (const auto* text = std::any_cast<std::string_view>(&value)) return std::string(*text);
            if (const auto* text = std::any_cast<const char*>(&value)) {
                return *text != nullptr ? std::string(*text) : std::string{};
            }
            if (const auto* text = std::any_cast<char*>(&value)) {
                return *text != nullptr ? std::string(*text) : std::string{};
            }
            return std::nullopt;
        }

        static UriKind kindOf(const Uri& uri) noexcept {
            return uri.getIsAbsoluteUriProperty() ? UriKind::Absolute : UriKind::Relative;
        }
    };

} // namespace System
