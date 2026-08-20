// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <optional>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Attribute.hpp"
#include "System/InvalidOperationException.hpp"

namespace System::ComponentModel::DataAnnotations {

    using SharpRuntime::intcs;

    /** Base class for validation attributes. */
    /**
     * @brief Base class for validation attributes.
     *
     * @warning **THIS FAMILY VALIDATES NOTHING, AND THAT IS A DECLARED LIMITATION RATHER THAN a
     * defect to be discovered at run time (#2406).** .NET's `ValidationAttribute` carries
     * `IsValid(object?)` (`ValidationAttribute.cs:352`), `Validate(...)` (`:468,497`),
     * `FormatErrorMessage(string)` (`:330`) and `RequiresValidationContext` (`:139`), and every
     * subclass overrides `IsValid`. **None of those exist here**, on this class or on any of the
     * eleven subclasses below: this whole namespace is **metadata only**.
     *
     * The names are what make that worth a warning rather than a footnote. A caller who writes
     * `RequiredAttribute` and sets an error message has every reason to believe something checks
     * it, and nothing does -- there is no member to call, so the mistake surfaces as *validation
     * that silently never happened*.
     *
     * Implementing it is not one member: `RegularExpressionAttribute` needs the regular-expression
     * engine, `EmailAddressAttribute`/`UrlAttribute`/`PhoneAttribute` need .NET's exact accepted
     * grammars, and `Validate` needs `ValidationContext` and `ValidationResult`, neither of which
     * this port has. It is a feature, not a repair, and it is recorded here rather than implied by
     * silence.
     */
    class ValidationAttribute : public System::Attribute {
    protected:
        std::string errorMessage_; ///< The validation error message template.
    public:
        /** Constructs the attribute with an empty error message. */
        ValidationAttribute() = default;

        /** @param errorMessage The error message returned on validation failure. */
        explicit ValidationAttribute(const std::string& errorMessage) : errorMessage_(errorMessage) {}

        /** @return The validation error message. */
        [[nodiscard]] const std::string& getErrorMessageProperty() const { return errorMessage_; }

        /** Sets the validation error message. */
        void setErrorMessageProperty(const std::string& v) { errorMessage_ = v; }
    };

    /** Specifies that a data field value is required. */
    class RequiredAttribute : public ValidationAttribute {
    public:
        bool AllowEmptyStrings = false; ///< If true, empty strings are accepted.

        /** Default constructor. */
        RequiredAttribute() = default;

        /** @param errorMessage Custom error message on validation failure. */
        explicit RequiredAttribute(const std::string& errorMessage) : ValidationAttribute(errorMessage) {}
    };

    /** Specifies the numeric range constraints for a data field. */
    class RangeAttribute : public ValidationAttribute {
        double minimum_;
        double maximum_;
    public:
        /**
         * @param min Minimum allowed value.
         * @param max Maximum allowed value.
         */
        RangeAttribute(double min, double max) : minimum_(min), maximum_(max) {}

        /** Integer overload — @param min and @p max are promoted to double. */
        RangeAttribute(intcs min, intcs max) : minimum_(static_cast<double>(min)), maximum_(static_cast<double>(max)) {}

        /** @return The minimum allowed value. */
        [[nodiscard]] double getMinimumProperty() const { return minimum_; }

        /** @return The maximum allowed value. */
        [[nodiscard]] double getMaximumProperty() const { return maximum_; }
    };

    /** Specifies the maximum and minimum string length. */
    class StringLengthAttribute : public ValidationAttribute {
        intcs maximumLength_;
        intcs minimumLength_ = 0;
    public:
        /** @param maximumLength Maximum allowed string length. */
        explicit StringLengthAttribute(intcs maximumLength) : maximumLength_(maximumLength) {}

        /** @return The maximum allowed string length. */
        [[nodiscard]] intcs getMaximumLengthProperty() const { return maximumLength_; }

        /** @return The minimum allowed string length. */
        [[nodiscard]] intcs getMinimumLengthProperty() const { return minimumLength_; }

        /** Sets the minimum allowed string length. */
        void setMinimumLengthProperty(intcs v) { minimumLength_ = v; }
    };

    /** Specifies the maximum length of array or string data. */
    class MaxLengthAttribute : public ValidationAttribute {
        intcs length_;
    public:
        /** @param length Maximum length; -1 means unlimited. */
        explicit MaxLengthAttribute(intcs length = -1) : length_(length) {}

        /** @return The maximum allowed length. */
        [[nodiscard]] intcs getLengthProperty() const { return length_; }
    };

    /** Specifies the minimum length of array or string data. */
    class MinLengthAttribute : public ValidationAttribute {
        intcs length_;
    public:
        /** @param length Minimum required length. */
        explicit MinLengthAttribute(intcs length) : length_(length) {}

        /** @return The minimum required length. */
        [[nodiscard]] intcs getLengthProperty() const { return length_; }
    };

    /** Specifies that a data field value must match the given regular expression. */
    class RegularExpressionAttribute : public ValidationAttribute {
        std::string pattern_;
    public:
        /** @param pattern The regular expression pattern. */
        explicit RegularExpressionAttribute(const std::string& pattern) : pattern_(pattern) {}

        /** @return The regular expression pattern. */
        [[nodiscard]] const std::string& getPatternProperty() const { return pattern_; }
    };

    /** Denotes that a data field value is a well-formed email address. */
    class EmailAddressAttribute : public ValidationAttribute {};
    /** Denotes that a data field value is a phone number. */
    class PhoneAttribute        : public ValidationAttribute {};
    /** Denotes that a data field value is a well-formed URL. */
    class UrlAttribute          : public ValidationAttribute {};
    /** Denotes that a data field value is a credit card number. */
    class CreditCardAttribute   : public ValidationAttribute {};

    /** Provides general-purpose metadata for display in UI scaffolding. */
    class DisplayAttribute : public System::Attribute {
    public:
        std::string Name;               ///< Display name.
        std::string Description;        ///< Description text.
        std::string Prompt;             ///< Watermark / prompt text.
        std::string GroupName;          ///< Group the field belongs to.
        std::string ShortName;          ///< Abbreviated display name.
        intcs Order  = 0;                ///< Display order.
        bool AutoGenerateField  = true; ///< Whether to auto-generate a field for this property.
        bool AutoGenerateFilter = true; ///< Whether to auto-generate a filter for this property.
    };

    /** Denotes one or more properties that uniquely identify an entity. */
    class KeyAttribute          : public System::Attribute {};

    /** Specifies whether a class or data column is excluded from scaffolding. */
    /**
     * @brief Specifies whether a column should be included in scaffolding.
     * `ScaffoldColumnAttribute.cs` -- `public bool Scaffold { get; }`, get-only.
     */
    class ScaffoldColumnAttribute : public System::Attribute {
        bool scaffold_;

    public:
        /** @param scaffold True to include the column in scaffolding. */
        explicit ScaffoldColumnAttribute(bool scaffold) : scaffold_(scaffold) {}

        /** @return true if the column should be included in scaffolding. */
        [[nodiscard]] bool getScaffoldProperty() const noexcept { return scaffold_; }
    };

    /** Specifies the data type to associate with a data field. */
    /**
     * @brief The kind of data a member holds. `DataType.cs` -- transcribed exactly, all seventeen.
     *
     * This port used to have no such enum: `DataTypeAttribute` held an untyped `std::string`, so
     * *"a known data type"* and *"a custom one named by the caller"* -- which .NET separates with
     * two constructors -- were the same thing, and any string at all was accepted where only
     * seventeen values are meaningful.
     */
    enum class DataType {
        Custom        = 0,
        DateTime      = 1,
        Date          = 2,
        Time          = 3,
        Duration      = 4,
        PhoneNumber   = 5,
        Currency      = 6,
        Text          = 7,
        Html          = 8,
        MultilineText = 9,
        EmailAddress  = 10,
        Password      = 11,
        Url           = 12,
        ImageUrl      = 13,
        CreditCard    = 14,
        PostalCode    = 15,
        Upload        = 16,
    };

    /**
     * @brief Specifies the kind of data a member holds. `DataTypeAttribute.cs:20-71,83`.
     *
     * @note **`DisplayFormat` is deliberately absent.** .NET's constructor sets a
     * `DisplayFormatAttribute` for `Date`, `Time` and `Currency` (`:26-45`) and publishes it as
     * `public DisplayFormatAttribute? DisplayFormat { get; protected set; }` (`:56`).
     * `DisplayFormatAttribute` does not exist in this port, and inventing it to be set by this
     * constructor would be adding a type in order to have somewhere to put a value nothing reads.
     * The constructor's three-case switch exists **only** to populate it, so omitting the switch
     * *is* the omission of `DisplayFormat` rather than a second one.
     */
    class DataTypeAttribute : public System::Attribute {
        DataAnnotations::DataType dataType_;
        std::optional<std::string> customDataType_;

    public:
        /** @param dataType One of the seventeen known kinds. `DataTypeAttribute.cs:20`. */
        explicit DataTypeAttribute(DataAnnotations::DataType dataType) : dataType_(dataType) {}

        /**
         * @brief Names a custom kind. `DataTypeAttribute.cs:55-59` -- chains to
         * `this(DataType.Custom)` and then stores the name, so the kind really is `Custom` and the
         * string is a separate fact rather than a replacement for it.
         */
        explicit DataTypeAttribute(const std::string& customDataType)
            : dataType_(DataAnnotations::DataType::Custom), customDataType_(customDataType) {}

        /** @return The kind. `:64` -- get-only. */
        [[nodiscard]] DataAnnotations::DataType getDataTypeProperty() const noexcept {
            return dataType_;
        }

        /** @return The custom kind's name, absent unless the string constructor was used. `:70`. */
        [[nodiscard]] const std::optional<std::string>& getCustomDataTypeProperty() const noexcept {
            return customDataType_;
        }

        /**
         * @brief The kind's name. `DataTypeAttribute.cs:83-96`.
         *
         * .NET reads `Enum.GetNames<DataType>()[(int)DataType]`, which is reflection. The
         * exhaustive `switch` below is this repository's substitute and is **stronger** for one
         * reason worth keeping: it has **no `default:`**, so under `-Wall -Wextra -Werror` a new
         * enumerator is a **compile error** here rather than a silently missing name -- the idiom
         * #1980 G-5 established, where a name table could not pin an enum's membership and an
         * exhaustive switch could.
         *
         * @throws System::InvalidOperationException if the kind is `Custom` and no custom name was
         *         supplied (`:115-121`, `EnsureValidDataType`), with .NET's own text.
         */
        [[nodiscard]] virtual std::string GetDataTypeName() const {
            if (dataType_ == DataAnnotations::DataType::Custom) {
                // EnsureValidDataType tests IsNullOrWhiteSpace, not merely empty.
                const bool blank =
                    !customDataType_.has_value() ||
                    customDataType_->find_first_not_of(" \t\n\v\f\r") == std::string::npos;
                if (blank) {
                    throw System::InvalidOperationException(
                        "The custom DataType string cannot be null or empty.");
                }
                return *customDataType_;
            }
            switch (dataType_) { // no default: -Wswitch pins this enum's membership
                case DataAnnotations::DataType::Custom:        break; // handled above
                case DataAnnotations::DataType::DateTime:      return "DateTime";
                case DataAnnotations::DataType::Date:          return "Date";
                case DataAnnotations::DataType::Time:          return "Time";
                case DataAnnotations::DataType::Duration:      return "Duration";
                case DataAnnotations::DataType::PhoneNumber:   return "PhoneNumber";
                case DataAnnotations::DataType::Currency:      return "Currency";
                case DataAnnotations::DataType::Text:          return "Text";
                case DataAnnotations::DataType::Html:          return "Html";
                case DataAnnotations::DataType::MultilineText: return "MultilineText";
                case DataAnnotations::DataType::EmailAddress:  return "EmailAddress";
                case DataAnnotations::DataType::Password:      return "Password";
                case DataAnnotations::DataType::Url:           return "Url";
                case DataAnnotations::DataType::ImageUrl:      return "ImageUrl";
                case DataAnnotations::DataType::CreditCard:    return "CreditCard";
                case DataAnnotations::DataType::PostalCode:    return "PostalCode";
                case DataAnnotations::DataType::Upload:        return "Upload";
            }
            return "Custom";
        }
    };

    /** Specifies that a data field value must match the value of another property. */
    class CompareAttribute : public ValidationAttribute {
        std::string otherProperty_;
    public:
        /** @param otherProperty Name of the property to compare against. */
        explicit CompareAttribute(const std::string& otherProperty) : otherProperty_(otherProperty) {}

        /** @return The name of the comparison target property. */
        [[nodiscard]] const std::string& getOtherPropertyProperty() const { return otherProperty_; }
    };

} // namespace System::ComponentModel::DataAnnotations
