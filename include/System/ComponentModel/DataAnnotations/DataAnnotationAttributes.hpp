// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System::ComponentModel::DataAnnotations {

    class ValidationAttribute : public System::Attribute {
    protected:
        std::string errorMessage_;
    public:
        ValidationAttribute() = default;
        explicit ValidationAttribute(const std::string& errorMessage) : errorMessage_(errorMessage) {}
        [[nodiscard]] const std::string& getErrorMessageProperty() const { return errorMessage_; }
        void setErrorMessageProperty(const std::string& v) { errorMessage_ = v; }
    };

    class RequiredAttribute : public ValidationAttribute {
    public:
        bool AllowEmptyStrings = false;
        RequiredAttribute() = default;
        explicit RequiredAttribute(const std::string& errorMessage) : ValidationAttribute(errorMessage) {}
    };

    class RangeAttribute : public ValidationAttribute {
        double minimum_;
        double maximum_;
    public:
        RangeAttribute(double min, double max) : minimum_(min), maximum_(max) {}
        RangeAttribute(int min, int max) : minimum_(static_cast<double>(min)), maximum_(static_cast<double>(max)) {}
        [[nodiscard]] double getMinimumProperty() const { return minimum_; }
        [[nodiscard]] double getMaximumProperty() const { return maximum_; }
    };

    class StringLengthAttribute : public ValidationAttribute {
        int maximumLength_;
        int minimumLength_ = 0;
    public:
        explicit StringLengthAttribute(int maximumLength) : maximumLength_(maximumLength) {}
        [[nodiscard]] int getMaximumLengthProperty() const { return maximumLength_; }
        [[nodiscard]] int getMinimumLengthProperty() const { return minimumLength_; }
        void setMinimumLengthProperty(int v) { minimumLength_ = v; }
    };

    class MaxLengthAttribute : public ValidationAttribute {
        int length_;
    public:
        explicit MaxLengthAttribute(int length = -1) : length_(length) {}
        [[nodiscard]] int getLengthProperty() const { return length_; }
    };

    class MinLengthAttribute : public ValidationAttribute {
        int length_;
    public:
        explicit MinLengthAttribute(int length) : length_(length) {}
        [[nodiscard]] int getLengthProperty() const { return length_; }
    };

    class RegularExpressionAttribute : public ValidationAttribute {
        std::string pattern_;
    public:
        explicit RegularExpressionAttribute(const std::string& pattern) : pattern_(pattern) {}
        [[nodiscard]] const std::string& getPatternProperty() const { return pattern_; }
    };

    class EmailAddressAttribute : public ValidationAttribute {};
    class PhoneAttribute        : public ValidationAttribute {};
    class UrlAttribute          : public ValidationAttribute {};
    class CreditCardAttribute   : public ValidationAttribute {};

    class DisplayAttribute : public System::Attribute {
    public:
        std::string Name;
        std::string Description;
        std::string Prompt;
        std::string GroupName;
        std::string ShortName;
        int Order    = 0;
        bool AutoGenerateField  = true;
        bool AutoGenerateFilter = true;
    };

    class KeyAttribute          : public System::Attribute {};
    class ScaffoldColumnAttribute : public System::Attribute {
    public:
        bool Scaffold;
        explicit ScaffoldColumnAttribute(bool scaffold) : Scaffold(scaffold) {}
    };

    class DataTypeAttribute : public System::Attribute {
    public:
        std::string DataType;
        explicit DataTypeAttribute(const std::string& dt) : DataType(dt) {}
    };

    class CompareAttribute : public ValidationAttribute {
        std::string otherProperty_;
    public:
        explicit CompareAttribute(const std::string& otherProperty) : otherProperty_(otherProperty) {}
        [[nodiscard]] const std::string& getOtherPropertyProperty() const { return otherProperty_; }
    };

} // namespace System::ComponentModel::DataAnnotations
