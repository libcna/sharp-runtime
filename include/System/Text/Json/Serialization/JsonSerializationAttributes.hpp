// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System::Text::Json::Serialization {

    /** Specifies the JSON property name for a member. */
    class JsonPropertyNameAttribute : public System::Attribute {
        std::string name_;
    public:
        /** Constructs the attribute with the given JSON property name. */
        explicit JsonPropertyNameAttribute(const std::string& name) : name_(name) {}
        /** Gets the JSON property name. */
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }
    };

    /** Controls when a property is ignored during JSON serialization. */
    enum class JsonIgnoreCondition {
        /** Property is never ignored. */
        Never          = 0,
        /** Property is always ignored. */
        Always         = 1,
        /** Property is ignored when it has a default value. */
        WhenWritingDefault = 2,
        /** Property is ignored when its value is null. */
        WhenWritingNull = 3
    };

    /** Marks a property as ignored during JSON serialization/deserialization. */
    class JsonIgnoreAttribute : public System::Attribute {
        JsonIgnoreCondition condition_ = JsonIgnoreCondition::Always;
    public:
        /** Constructs the attribute using the Always condition. */
        JsonIgnoreAttribute() = default;
        /** Constructs the attribute with the specified ignore condition. */
        explicit JsonIgnoreAttribute(JsonIgnoreCondition cond) : condition_(cond) {}
        /** Gets the condition under which the property is ignored. */
        [[nodiscard]] JsonIgnoreCondition getConditionProperty() const { return condition_; }
        /** Sets the condition under which the property is ignored. */
        void setConditionProperty(JsonIgnoreCondition c) { condition_ = c; }
    };

    /** Associates a custom JsonConverter with a property or type. */
    class JsonConverterAttribute : public System::Attribute {
        std::string converterTypeName_;
    public:
        /** Constructs the attribute with no explicit converter type. */
        JsonConverterAttribute() = default;
        /** Constructs the attribute with the given converter type name. */
        explicit JsonConverterAttribute(const std::string& typeName) : converterTypeName_(typeName) {}
        /** Gets the converter type name. */
        [[nodiscard]] const std::string& getConverterTypeNameProperty() const { return converterTypeName_; }
    };

    /** Specifies the serialization order of a JSON property. */
    class JsonPropertyOrderAttribute : public System::Attribute {
        int order_;
    public:
        /** Constructs the attribute with the given order value. */
        explicit JsonPropertyOrderAttribute(int order) : order_(order) {}
        /** Gets the serialization order. */
        [[nodiscard]] int getOrderProperty() const { return order_; }
    };

    /** Marks a non-public property for inclusion in JSON serialization. */
    class JsonIncludeAttribute : public System::Attribute {};

    /** Marks a property as required in JSON serialization. */
    class JsonRequiredAttribute : public System::Attribute {};

    /** Marks a property for receiving extension data (extra JSON properties). */
    class JsonExtensionDataAttribute : public System::Attribute {};

    /** Specifies number-handling behavior for a property or type. */
    class JsonNumberHandlingAttribute : public System::Attribute {
        int handling_; // JsonNumberHandling enum value
    public:
        /** Constructs the attribute with the given JsonNumberHandling value. */
        explicit JsonNumberHandlingAttribute(int handling) : handling_(handling) {}
        /** Gets the number-handling value. */
        [[nodiscard]] int getHandlingProperty() const { return handling_; }
    };

    /** Enables polymorphic type serialization for a base class. */
    class JsonPolymorphicAttribute : public System::Attribute {
    public:
        /** Controls how unknown derived types are handled. */
        bool UnknownDerivedTypeHandling = false;
        /** Controls whether unrecognized type discriminators are ignored. */
        bool IgnoreUnrecognizedTypeDiscriminators = false;
        /** The JSON property name used as a type discriminator. */
        std::string TypeDiscriminatorPropertyName;
    };

    /** Associates a derived type with a base class for polymorphic deserialization. */
    class JsonDerivedTypeAttribute : public System::Attribute {
        std::string derivedTypeName_;
    public:
        /** Constructs the attribute for the given derived type name. */
        explicit JsonDerivedTypeAttribute(const std::string& typeName) : derivedTypeName_(typeName) {}
        /** Constructs the attribute with a derived type name and a type discriminator value. */
        explicit JsonDerivedTypeAttribute(const std::string& typeName, const std::string& /*typeDiscriminator*/)
            : derivedTypeName_(typeName) {}
        /** Gets the derived type name. */
        [[nodiscard]] const std::string& getDerivedTypeNameProperty() const { return derivedTypeName_; }
    };

} // namespace System::Text::Json::Serialization
