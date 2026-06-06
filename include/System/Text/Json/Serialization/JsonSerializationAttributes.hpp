// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System::Text::Json::Serialization {

    class JsonPropertyNameAttribute : public System::Attribute {
        std::string name_;
    public:
        explicit JsonPropertyNameAttribute(const std::string& name) : name_(name) {}
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }
    };

    enum class JsonIgnoreCondition {
        Never          = 0,
        Always         = 1,
        WhenWritingDefault = 2,
        WhenWritingNull = 3
    };

    class JsonIgnoreAttribute : public System::Attribute {
        JsonIgnoreCondition condition_ = JsonIgnoreCondition::Always;
    public:
        JsonIgnoreAttribute() = default;
        explicit JsonIgnoreAttribute(JsonIgnoreCondition cond) : condition_(cond) {}
        [[nodiscard]] JsonIgnoreCondition getConditionProperty() const { return condition_; }
        void setConditionProperty(JsonIgnoreCondition c) { condition_ = c; }
    };

    class JsonConverterAttribute : public System::Attribute {
        std::string converterTypeName_;
    public:
        JsonConverterAttribute() = default;
        explicit JsonConverterAttribute(const std::string& typeName) : converterTypeName_(typeName) {}
        [[nodiscard]] const std::string& getConverterTypeNameProperty() const { return converterTypeName_; }
    };

    class JsonPropertyOrderAttribute : public System::Attribute {
        int order_;
    public:
        explicit JsonPropertyOrderAttribute(int order) : order_(order) {}
        [[nodiscard]] int getOrderProperty() const { return order_; }
    };

    class JsonIncludeAttribute : public System::Attribute {};

    class JsonRequiredAttribute : public System::Attribute {};

    class JsonExtensionDataAttribute : public System::Attribute {};

    class JsonNumberHandlingAttribute : public System::Attribute {
        int handling_; // JsonNumberHandling enum value
    public:
        explicit JsonNumberHandlingAttribute(int handling) : handling_(handling) {}
        [[nodiscard]] int getHandlingProperty() const { return handling_; }
    };

    class JsonPolymorphicAttribute : public System::Attribute {
    public:
        bool UnknownDerivedTypeHandling = false;
        bool IgnoreUnrecognizedTypeDiscriminators = false;
        std::string TypeDiscriminatorPropertyName;
    };

    class JsonDerivedTypeAttribute : public System::Attribute {
        std::string derivedTypeName_;
    public:
        explicit JsonDerivedTypeAttribute(const std::string& typeName) : derivedTypeName_(typeName) {}
        explicit JsonDerivedTypeAttribute(const std::string& typeName, const std::string& /*typeDiscriminator*/)
            : derivedTypeName_(typeName) {}
        [[nodiscard]] const std::string& getDerivedTypeNameProperty() const { return derivedTypeName_; }
    };

} // namespace System::Text::Json::Serialization
