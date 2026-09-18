// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include "System/Attribute.hpp"
#include "System/Type.hpp"

namespace System::ComponentModel {

    class TypeConverter;

    /**
     * @brief Specifies what type to use as a converter for the object this attribute is bound to.
     *
     * C++ counterpart of .NET System.ComponentModel.TypeConverterAttribute. .NET stores only the
     * converter's type name and instantiates it later by reflection (`Activator.CreateInstance`).
     * This runtime cannot instantiate a type from its name, so the attribute may also carry the
     * one thing reflection would have supplied: a **factory**. An attribute built with
     * `Of<TConverter>()` (or the `(Type, ConverterFactory)` constructor) can create its converter,
     * and that is what `TypeDescriptor::GetConverter` calls after finding the attribute among a
     * type's registered attributes. An attribute built from a name alone is metadata only, exactly
     * as before, and `CreateConverter()` returns null for it -- `TypeDescriptor` then falls back to
     * the plain `TypeConverter`, which is also what .NET does when the named type cannot be loaded.
     *
     * The `System::Type` overload stores the RTTI full name; assembly-qualified names are
     * unavailable by the project's permanent no-reflection deviation.
     */
    class TypeConverterAttribute final : public System::Attribute {
    public:
        /** @brief Creates the converter the attribute names. Port-specific; see the class comment. */
        using ConverterFactory = std::function<std::shared_ptr<TypeConverter>()>;

        /** @brief A shared attribute with no converter type name. */
        static const TypeConverterAttribute Default;

        /** @brief Constructs the attribute with an empty type name. */
        TypeConverterAttribute() = default;

        /**
         * @brief Initializes the attribute with the fully qualified name of the converter type.
         * @param typeName Fully qualified name of the converter type.
         */
        explicit TypeConverterAttribute(std::string typeName) : typeName_(std::move(typeName)) {}

        /**
         * @brief Initializes the attribute with the converter type.
         * @param type Converter type represented by the project's RTTI Type wrapper.
         */
        explicit TypeConverterAttribute(const System::Type& type)
            : typeName_(type.getFullNameProperty()) {}

        /**
         * @brief Initializes the attribute with the converter type and the factory that creates it.
         *
         * Port-specific: this is the constructor `TypeDescriptor::AddAttributes` and
         * `TypeDescriptor::RegisterType` need for a converter that can actually be obtained.
         * @param type Converter type represented by the project's RTTI Type wrapper.
         * @param factory Creates a new instance of that converter.
         */
        TypeConverterAttribute(const System::Type& type, ConverterFactory factory)
            : typeName_(type.getFullNameProperty()), factory_(std::move(factory)) {}

        /**
         * @brief Builds the attribute for @p TConverter with a factory that default-constructs it.
         *
         * Port-specific; the C++ spelling of `[TypeConverter(typeof(TConverter))]`.
         * @tparam TConverter A default-constructible `TypeConverter` subclass.
         * @return The attribute, able to create its converter.
         */
        template<class TConverter>
        [[nodiscard]] static TypeConverterAttribute Of() {
            return TypeConverterAttribute(System::Type::From<TConverter>(),
                                          [] { return std::shared_ptr<TypeConverter>(std::make_shared<TConverter>()); });
        }

        /**
         * @brief Gets the fully qualified type name of the Type to use as a converter.
         * @return The fully qualified converter type name.
         */
        [[nodiscard]] const std::string& getConverterTypeNameProperty() const noexcept { return typeName_; }

        /**
         * @brief Reports whether this attribute carries a factory and can create its converter.
         *
         * Port-specific: .NET answers this question by loading the named type.
         * @return true when `CreateConverter()` returns an instance.
         */
        [[nodiscard]] bool getCanCreateConverterProperty() const noexcept { return static_cast<bool>(factory_); }

        /**
         * @brief Creates a new instance of the converter this attribute names.
         *
         * Port-specific: the counterpart of `Activator.CreateInstance(Type.GetType(ConverterTypeName))`
         * in .NET's `TypeDescriptor`.
         * @return A new converter, or null when the attribute was built from a name alone.
         */
        [[nodiscard]] std::shared_ptr<TypeConverter> CreateConverter() const {
            return factory_ ? factory_() : nullptr;
        }

        /** @brief Compares attributes by converter type name, as .NET does. */
        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const TypeConverterAttribute*>(&other);
            return attribute != nullptr && attribute->typeName_ == typeName_;
        }

        /** @brief Returns a hash code based on the converter type name. */
        [[nodiscard]] int GetHashCode() const override {
            return static_cast<int>(std::hash<std::string>{}(typeName_));
        }

    private:
        std::string typeName_;
        ConverterFactory factory_;
    };
    inline const TypeConverterAttribute TypeConverterAttribute::Default{};

} // namespace System::ComponentModel
