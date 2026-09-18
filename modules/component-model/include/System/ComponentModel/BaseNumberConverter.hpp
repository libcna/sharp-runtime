// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <cctype>
#include <exception>
#include <string>
#include "System/ArgumentNullException.hpp"
#include "System/ComponentModel/TypeConverter.hpp"
#include "System/ComponentModel/detail/ObjectText.hpp"
#include "System/Exception.hpp"
#include "System/Globalization/CultureInfo.hpp"
#include "System/Globalization/NumberFormatInfo.hpp"
#include "System/Type.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a base type converter for nonfloating-point numerical types.
     *
     * C++ counterpart of .NET System.ComponentModel.BaseNumberConverter, the base of
     * `ByteConverter`, `Int32Converter`, `SingleConverter` and the rest. A derived converter
     * names its target type and supplies the three primitives .NET's does: parse from a radix,
     * parse with a `NumberFormatInfo`, and format with a `NumberFormatInfo`.
     *
     * The behaviour is .NET's, verified against `BaseNumberConverter.cs`:
     *  - a string is trimmed; for the integer converters a leading `#` or `0x`/`0X`/`&h`/`&H`
     *    means hexadecimal; otherwise it is parsed with the culture's `NumberFormatInfo`;
     *  - a parse failure is reported as an `Exception` reading `"<text> is not a valid value
     *    for <Type>."` with the parser's exception as the inner exception;
     *  - `ConvertTo` a string formats with the culture's `NumberFormatInfo`;
     *  - `ConvertTo` another primitive is not supported (.NET's `Convert.ChangeType` has no
     *    counterpart here), so `CanConvertTo` answers only for `std::string` and
     *    `InstanceDescriptor`-free base behaviour.
     */
    class BaseNumberConverter : public TypeConverter {
    public:
        using TypeConverter::CanConvertFrom;
        using TypeConverter::ConvertFrom;
        using TypeConverter::ConvertTo;

        /**
         * @brief Determines if this converter can convert an object in the given source type to
         *        the native type of the converter.
         *
         * C++ counterpart of .NET BaseNumberConverter.CanConvertFrom: strings, plus the base.
         */
        [[nodiscard]] bool CanConvertFrom(ITypeDescriptorContext* context, const System::Type& sourceType) const override {
            return detail::ObjectText::IsStringType(sourceType) ||
                   TypeConverter::CanConvertFrom(context, sourceType);
        }

        /**
         * @brief Converts the given object to the converter's native type.
         *
         * C++ counterpart of .NET BaseNumberConverter.ConvertFrom.
         * @throws System::Exception reading `"<text> is not a valid value for <Type>."` when
         *         the text does not parse; the parser's exception is the inner exception.
         */
        [[nodiscard]] std::any ConvertFrom(ITypeDescriptorContext* context,
                                           const System::Globalization::CultureInfo* culture,
                                           const std::any& value) const override {
            if (auto text = detail::ObjectText::AsString(value)) {
                const std::string trimmed = trim(*text);
                try {
                    if (getAllowHexProperty() && !trimmed.empty() && trimmed[0] == '#') {
                        return FromString(trimmed.substr(1), 16);
                    }
                    if (getAllowHexProperty() && (startsWith(trimmed, "0x") || startsWith(trimmed, "0X") ||
                                                  startsWith(trimmed, "&h") || startsWith(trimmed, "&H"))) {
                        return FromString(trimmed.substr(2), 16);
                    }
                    if (culture == nullptr) culture = &System::Globalization::CultureInfo::getCurrentCultureProperty();
                    return FromString(trimmed, culture->getNumberFormatProperty());
                } catch (const System::Exception&) {
                    throw FromStringError(*text, std::current_exception());
                } catch (const std::exception&) {
                    throw FromStringError(*text, std::current_exception());
                }
            }
            return TypeConverter::ConvertFrom(context, culture, value);
        }

        /**
         * @brief Converts the given value object to the destination type.
         *
         * C++ counterpart of .NET BaseNumberConverter.ConvertTo: to a string, formatted with
         * the culture's `NumberFormatInfo`.
         * @throws System::ArgumentNullException if @p destinationType is the null type.
         */
        [[nodiscard]] std::any ConvertTo(ITypeDescriptorContext* context,
                                         const System::Globalization::CultureInfo* culture,
                                         const std::any& value,
                                         const System::Type& destinationType) const override {
            if (destinationType == System::Type()) throw System::ArgumentNullException("destinationType");
            if (destinationType == System::Type::From<std::string>() && value.has_value() &&
                System::Type::FromTypeInfo(value.type()) == getTargetTypeProperty()) {
                if (culture == nullptr) culture = &System::Globalization::CultureInfo::getCurrentCultureProperty();
                return std::any(ToString(value, culture->getNumberFormatProperty()));
            }
            return TypeConverter::ConvertTo(context, culture, value, destinationType);
        }

    protected:
        BaseNumberConverter() = default;

        /**
         * @brief Whether the converter accepts a `#`/`0x` prefixed hexadecimal string.
         *
         * C++ counterpart of .NET BaseNumberConverter.AllowHex: true for the integer converters.
         */
        [[nodiscard]] virtual bool getAllowHexProperty() const noexcept { return true; }

        /**
         * @brief The type this converter converts to and from.
         *
         * C++ counterpart of .NET BaseNumberConverter.TargetType.
         */
        [[nodiscard]] virtual System::Type getTargetTypeProperty() const noexcept = 0;

        /**
         * @brief Parses @p value in the given radix.
         *
         * C++ counterpart of .NET BaseNumberConverter.FromString(string, int).
         */
        [[nodiscard]] virtual std::any FromString(const std::string& value, SharpRuntime::intcs radix) const = 0;

        /**
         * @brief Parses @p value with the given format information.
         *
         * C++ counterpart of .NET BaseNumberConverter.FromString(string, NumberFormatInfo).
         */
        [[nodiscard]] virtual std::any FromString(const std::string& value,
                                                  const System::Globalization::NumberFormatInfo& formatInfo) const = 0;

        /**
         * @brief Formats @p value with the given format information.
         *
         * C++ counterpart of .NET BaseNumberConverter.ToString(object, NumberFormatInfo).
         */
        [[nodiscard]] virtual std::string ToString(const std::any& value,
                                                   const System::Globalization::NumberFormatInfo& formatInfo) const = 0;

        /**
         * @brief Builds the exception `ConvertFrom` throws for text that does not parse.
         *
         * C++ counterpart of .NET BaseNumberConverter.FromStringError(string, Exception):
         * `"<text> is not a valid value for <Type>."`, with the parser's exception inside.
         */
        [[nodiscard]] virtual System::Exception FromStringError(const std::string& failedText,
                                                                std::exception_ptr innerException) const {
            return System::Exception(failedText + " is not a valid value for " +
                                     getTargetTypeProperty().getNameProperty() + ".", innerException);
        }

    private:
        static std::string trim(const std::string& text) {
            std::size_t begin = 0;
            std::size_t end = text.size();
            while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
            while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
            return text.substr(begin, end - begin);
        }

        static bool startsWith(const std::string& text, const char* prefix) {
            return text.rfind(prefix, 0) == 0;
        }
    };

    namespace detail {

    /**
     * @brief The shared body of the ten numeric converters: `T` is the boxed value type and
     *        `Primitive` the `System::X` helper whose `Parse`/`ToString` it delegates to.
     */
    template<class T, class Primitive, bool AllowHex>
    class NumberConverterBase : public BaseNumberConverter {
    protected:
        [[nodiscard]] bool getAllowHexProperty() const noexcept override { return AllowHex; }

        [[nodiscard]] System::Type getTargetTypeProperty() const noexcept override { return System::Type::From<T>(); }

        [[nodiscard]] std::any FromString(const std::string& value, SharpRuntime::intcs radix) const override {
            if constexpr (AllowHex) {
                if (radix == 16) {
                    return std::any(Primitive::Parse(value, System::Globalization::NumberStyles::HexNumber, nullptr));
                }
            }
            (void)radix;
            return std::any(Primitive::Parse(value));
        }

        [[nodiscard]] std::any FromString(const std::string& value,
                                          const System::Globalization::NumberFormatInfo& formatInfo) const override {
            const FormatInfoProvider provider{&formatInfo};
            if constexpr (AllowHex) {
                return std::any(Primitive::Parse(value, System::Globalization::NumberStyles::Integer, &provider));
            } else {
                return std::any(Primitive::Parse(value, System::Globalization::NumberStyles::Float, &provider));
            }
        }

        [[nodiscard]] std::string ToString(const std::any& value,
                                           const System::Globalization::NumberFormatInfo& formatInfo) const override {
            const FormatInfoProvider provider{&formatInfo};
            if constexpr (AllowHex) {
                // Int32Converter and its siblings format with "G", which for an integer is the
                // plain decimal digits the no-format overload produces.
                return Primitive::ToString(std::any_cast<T>(value), &provider);
            } else {
                return Primitive::ToString(std::any_cast<T>(value), "R", &provider);
            }
        }

    private:
        // The NumberFormatInfo the culture handed us, offered back as a provider so the
        // numeric primitives' IFormatProvider overloads can read it.
        class FormatInfoProvider final : public System::IFormatProvider {
        public:
            explicit FormatInfoProvider(const System::Globalization::NumberFormatInfo* info) : info_(info) {}
            [[nodiscard]] void* GetFormat(const std::type_info& formatType) const override {
                if (formatType == typeid(System::Globalization::NumberFormatInfo)) {
                    return const_cast<System::Globalization::NumberFormatInfo*>(info_);
                }
                return nullptr;
            }
        private:
            const System::Globalization::NumberFormatInfo* info_;
        };
    };

    } // namespace detail

} // namespace System::ComponentModel
