// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include "System/ComponentModel/AttributeCollection.hpp"
#include "System/ComponentModel/PropertyDescriptorCollection.hpp"
#include "System/ComponentModel/TypeConverter.hpp"
#include "System/ComponentModel/TypeDescriptor.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a type converter to convert expandable objects to and from various other
     *        representations.
     *
     * C++ counterpart of .NET System.ComponentModel.ExpandableObjectConverter: the converter
     * that makes a value expandable in a property grid. `GetPropertiesSupported` is true and
     * `GetProperties` returns `TypeDescriptor::GetProperties(value, attributes)` -- in .NET the
     * reflected public properties of the value's type, here the properties registered for it
     * with `TypeDescriptor::RegisterType`. It is the base of
     * `Microsoft.Xna.Framework.Design.MathTypeConverter`, whose subclasses override
     * `GetProperties` with their own descriptors.
     */
    class ExpandableObjectConverter : public TypeConverter {
    public:
        using TypeConverter::GetProperties;
        using TypeConverter::GetPropertiesSupported;

        /** @brief Initializes a new instance of the ExpandableObjectConverter class. */
        ExpandableObjectConverter() = default;

        /**
         * @brief Gets a collection of properties for the type of object specified by the value
         *        parameter.
         *
         * C++ counterpart of .NET ExpandableObjectConverter.GetProperties.
         */
        [[nodiscard]] PropertyDescriptorCollection GetProperties(ITypeDescriptorContext* context,
                                                                const std::any& value,
                                                                const AttributeCollection& attributes) const override {
            (void)context;
            if (!value.has_value()) return {};
            return TypeDescriptor::GetProperties(value, attributes);
        }

        /**
         * @brief Gets a value indicating whether this object supports properties using the
         *        specified context: it does.
         *
         * C++ counterpart of .NET ExpandableObjectConverter.GetPropertiesSupported.
         */
        [[nodiscard]] bool GetPropertiesSupported(ITypeDescriptorContext* context) const override {
            (void)context;
            return true;
        }
    };

} // namespace System::ComponentModel
