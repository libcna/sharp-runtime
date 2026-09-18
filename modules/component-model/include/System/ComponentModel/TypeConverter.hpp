// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/ComponentModel/AttributeCollection.hpp"
#include "System/ComponentModel/BrowsableAttribute.hpp"
#include "System/ComponentModel/ITypeDescriptorContext.hpp"
#include "System/ComponentModel/PropertyDescriptor.hpp"
#include "System/ComponentModel/PropertyDescriptorCollection.hpp"
#include "System/Globalization/CultureInfo.hpp"
#include "System/NotSupportedException.hpp"
#include "System/Type.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a unified way of converting types of values to other types, as well as
     *        for accessing standard values and subproperties.
     *
     * C++ counterpart of .NET System.ComponentModel.TypeConverter. The surface is .NET's, with
     * this runtime's conventions for the untyped parts of it:
     *
     *  - `object` is `std::any`; a string value is a `std::string` (a `const char*` is accepted
     *    wherever a string is and treated as one);
     *  - `CultureInfo` and `ITypeDescriptorContext` are passed by pointer, and `nullptr` is .NET's
     *    `null` -- the current culture and no context respectively;
     *  - `IDictionary propertyValues` is a `System::Collections::Hashtable`, the `IDictionary`
     *    implementation with a string-keyed surface, because this runtime's raw `IDictionary`
     *    keys are object identities and cannot express `propertyValues["X"]`;
     *  - `PropertyDescriptorCollection` and `StandardValuesCollection` are returned by value, and
     *    .NET's `null` result is the empty collection (`GetPropertiesSupported` and
     *    `GetStandardValuesSupported` are the questions that distinguish the two);
     *  - exceptions are the .NET ones: `NotSupportedException` from a conversion the converter
     *    does not do, `ArgumentNullException` for a null destination type.
     *
     * Converters are obtained from `TypeDescriptor::GetConverter` and shared, so a converter is
     * not copyable; it is stateless by convention and safe to use from several threads at once.
     */
    class TypeConverter {
    public:
        class StandardValuesCollection;
        class SimplePropertyDescriptor;

        TypeConverter() = default;
        virtual ~TypeConverter() = default;
        TypeConverter(const TypeConverter&) = delete;
        TypeConverter& operator=(const TypeConverter&) = delete;

        /**
         * @brief Returns whether this converter can convert an object of the given type to the
         *        type of this converter.
         *
         * C++ counterpart of .NET TypeConverter.CanConvertFrom(Type).
         * @param sourceType The type to convert from.
         * @return true if the conversion is possible.
         */
        [[nodiscard]] bool CanConvertFrom(const System::Type& sourceType) const {
            return CanConvertFrom(nullptr, sourceType);
        }

        /**
         * @brief Returns whether this converter can convert an object of the given type to the
         *        type of this converter, using the specified context.
         *
         * C++ counterpart of .NET TypeConverter.CanConvertFrom(ITypeDescriptorContext, Type).
         * The base converter can convert from an `InstanceDescriptor` only.
         * @param context The format context, or null.
         * @param sourceType The type to convert from.
         * @return true if the conversion is possible.
         */
        [[nodiscard]] virtual bool CanConvertFrom(ITypeDescriptorContext* context, const System::Type& sourceType) const;

        /**
         * @brief Returns whether this converter can convert the object to the specified type.
         *
         * C++ counterpart of .NET TypeConverter.CanConvertTo(Type).
         * @param destinationType The type to convert to.
         * @return true if the conversion is possible.
         */
        [[nodiscard]] bool CanConvertTo(const System::Type& destinationType) const {
            return CanConvertTo(nullptr, destinationType);
        }

        /**
         * @brief Returns whether this converter can convert the object to the specified type,
         *        using the specified context.
         *
         * C++ counterpart of .NET TypeConverter.CanConvertTo(ITypeDescriptorContext, Type). The
         * base converter can convert to `std::string` only.
         * @param context The format context, or null.
         * @param destinationType The type to convert to.
         * @return true if the conversion is possible.
         */
        [[nodiscard]] virtual bool CanConvertTo(ITypeDescriptorContext* context, const System::Type& destinationType) const;

        /**
         * @brief Converts the given value to the type of this converter.
         *
         * C++ counterpart of .NET TypeConverter.ConvertFrom(object): the current culture and
         * no context.
         * @param value The boxed value to convert.
         * @return The converted value, boxed.
         * @throws System::NotSupportedException if the conversion cannot be performed.
         */
        [[nodiscard]] std::any ConvertFrom(const std::any& value) const {
            return ConvertFrom(nullptr, &System::Globalization::CultureInfo::getCurrentCultureProperty(), value);
        }

        /**
         * @brief Converts the given object to the type of this converter, using the specified
         *        context and culture information.
         *
         * C++ counterpart of .NET TypeConverter.ConvertFrom(ITypeDescriptorContext, CultureInfo,
         * object). The base converter invokes an `InstanceDescriptor` and refuses anything else.
         * @param context The format context, or null.
         * @param culture The culture, or null for the current culture.
         * @param value The boxed value to convert.
         * @return The converted value, boxed.
         * @throws System::NotSupportedException if the conversion cannot be performed.
         */
        [[nodiscard]] virtual std::any ConvertFrom(ITypeDescriptorContext* context,
                                                   const System::Globalization::CultureInfo* culture,
                                                   const std::any& value) const;

        /**
         * @brief Converts the given string to the type of this converter, using the invariant
         *        culture.
         *
         * C++ counterpart of .NET TypeConverter.ConvertFromInvariantString(string).
         * @param text The text to convert.
         * @return The converted value, boxed.
         */
        [[nodiscard]] std::any ConvertFromInvariantString(const std::string& text) const {
            return ConvertFromString(nullptr, &System::Globalization::CultureInfo::getInvariantCultureProperty(), text);
        }

        /**
         * @brief Converts the given string to the type of this converter, using the invariant
         *        culture and the specified context.
         *
         * C++ counterpart of .NET TypeConverter.ConvertFromInvariantString(ITypeDescriptorContext, string).
         * @param context The format context, or null.
         * @param text The text to convert.
         * @return The converted value, boxed.
         */
        [[nodiscard]] std::any ConvertFromInvariantString(ITypeDescriptorContext* context, const std::string& text) const {
            return ConvertFromString(context, &System::Globalization::CultureInfo::getInvariantCultureProperty(), text);
        }

        /**
         * @brief Converts the specified text to an object.
         *
         * C++ counterpart of .NET TypeConverter.ConvertFromString(string): no context and the
         * culture left to `ConvertFrom` (which reads it as the current culture).
         * @param text The text to convert.
         * @return The converted value, boxed.
         */
        [[nodiscard]] std::any ConvertFromString(const std::string& text) const {
            return ConvertFrom(nullptr, nullptr, std::any(text));
        }

        /**
         * @brief Converts the given text to an object, using the specified context.
         *
         * C++ counterpart of .NET TypeConverter.ConvertFromString(ITypeDescriptorContext, string).
         * @param context The format context, or null.
         * @param text The text to convert.
         * @return The converted value, boxed.
         */
        [[nodiscard]] std::any ConvertFromString(ITypeDescriptorContext* context, const std::string& text) const {
            return ConvertFrom(context, &System::Globalization::CultureInfo::getCurrentCultureProperty(), std::any(text));
        }

        /**
         * @brief Converts the given text to an object, using the specified context and culture
         *        information.
         *
         * C++ counterpart of .NET TypeConverter.ConvertFromString(ITypeDescriptorContext, CultureInfo, string).
         * @param context The format context, or null.
         * @param culture The culture, or null for the current culture.
         * @param text The text to convert.
         * @return The converted value, boxed.
         */
        [[nodiscard]] std::any ConvertFromString(ITypeDescriptorContext* context,
                                                 const System::Globalization::CultureInfo* culture,
                                                 const std::string& text) const {
            return ConvertFrom(context, culture, std::any(text));
        }

        /**
         * @brief Converts the given value object to the specified type.
         *
         * C++ counterpart of .NET TypeConverter.ConvertTo(object, Type): no context, current
         * culture.
         * @param value The boxed value to convert.
         * @param destinationType The type to convert to.
         * @return The converted value, boxed.
         * @throws System::NotSupportedException if the conversion cannot be performed.
         */
        [[nodiscard]] std::any ConvertTo(const std::any& value, const System::Type& destinationType) const {
            return ConvertTo(nullptr, nullptr, value, destinationType);
        }

        /**
         * @brief Converts the given value object to the specified type, using the specified
         *        context and culture information.
         *
         * C++ counterpart of .NET TypeConverter.ConvertTo(ITypeDescriptorContext, CultureInfo,
         * object, Type). The base converter converts to `std::string` only: an empty value
         * becomes the empty string, a primitive is formatted with the culture, a string is
         * itself, and any other value becomes its registered `ToString` text or its type name.
         * @param context The format context, or null.
         * @param culture The culture, or null for the current culture.
         * @param value The boxed value to convert.
         * @param destinationType The type to convert to.
         * @return The converted value, boxed.
         * @throws System::ArgumentNullException if @p destinationType is the null type.
         * @throws System::NotSupportedException if the conversion cannot be performed.
         */
        [[nodiscard]] virtual std::any ConvertTo(ITypeDescriptorContext* context,
                                                 const System::Globalization::CultureInfo* culture,
                                                 const std::any& value,
                                                 const System::Type& destinationType) const;

        /**
         * @brief Converts the specified value to a culture-invariant string representation.
         *
         * C++ counterpart of .NET TypeConverter.ConvertToInvariantString(object).
         * @param value The boxed value.
         * @return The invariant text.
         */
        [[nodiscard]] std::string ConvertToInvariantString(const std::any& value) const {
            return ConvertToString(nullptr, &System::Globalization::CultureInfo::getInvariantCultureProperty(), value);
        }

        /**
         * @brief Converts the specified value to a culture-invariant string representation,
         *        using the specified context.
         *
         * C++ counterpart of .NET TypeConverter.ConvertToInvariantString(ITypeDescriptorContext, object).
         * @param context The format context, or null.
         * @param value The boxed value.
         * @return The invariant text.
         */
        [[nodiscard]] std::string ConvertToInvariantString(ITypeDescriptorContext* context, const std::any& value) const {
            return ConvertToString(context, &System::Globalization::CultureInfo::getInvariantCultureProperty(), value);
        }

        /**
         * @brief Converts the specified value to a string representation.
         *
         * C++ counterpart of .NET TypeConverter.ConvertToString(object).
         * @param value The boxed value.
         * @return The text, in the current culture.
         */
        [[nodiscard]] std::string ConvertToString(const std::any& value) const {
            return ConvertToString(nullptr, &System::Globalization::CultureInfo::getCurrentCultureProperty(), value);
        }

        /**
         * @brief Converts the given value to a string representation, using the given context.
         *
         * C++ counterpart of .NET TypeConverter.ConvertToString(ITypeDescriptorContext, object).
         * @param context The format context, or null.
         * @param value The boxed value.
         * @return The text, in the current culture.
         */
        [[nodiscard]] std::string ConvertToString(ITypeDescriptorContext* context, const std::any& value) const {
            return ConvertToString(context, &System::Globalization::CultureInfo::getCurrentCultureProperty(), value);
        }

        /**
         * @brief Converts the given value to a string representation, using the specified
         *        context and culture information.
         *
         * C++ counterpart of .NET TypeConverter.ConvertToString(ITypeDescriptorContext, CultureInfo, object).
         * @param context The format context, or null.
         * @param culture The culture, or null for the current culture.
         * @param value The boxed value.
         * @return The text.
         */
        [[nodiscard]] std::string ConvertToString(ITypeDescriptorContext* context,
                                                  const System::Globalization::CultureInfo* culture,
                                                  const std::any& value) const;

        /**
         * @brief Re-creates an object given a set of property values for the object.
         *
         * C++ counterpart of .NET TypeConverter.CreateInstance(IDictionary).
         * @param propertyValues The new property values, keyed by property name.
         * @return The new instance, boxed; empty when the converter does not create instances.
         */
        [[nodiscard]] std::any CreateInstance(const System::Collections::Hashtable& propertyValues) const {
            return CreateInstance(nullptr, propertyValues);
        }

        /**
         * @brief Creates an instance of the type that this converter is associated with, using
         *        the specified context, given a set of property values for the object.
         *
         * C++ counterpart of .NET TypeConverter.CreateInstance(ITypeDescriptorContext, IDictionary).
         * The base converter returns an empty value (.NET's `null`).
         * @param context The format context, or null.
         * @param propertyValues The new property values, keyed by property name.
         * @return The new instance, boxed.
         */
        [[nodiscard]] virtual std::any CreateInstance(ITypeDescriptorContext* context,
                                                      const System::Collections::Hashtable& propertyValues) const;

        /**
         * @brief Returns whether changing a value on this object requires a call to
         *        CreateInstance to create a new value.
         *
         * C++ counterpart of .NET TypeConverter.GetCreateInstanceSupported().
         * @return true if `CreateInstance` must be called after a property changes.
         */
        [[nodiscard]] bool GetCreateInstanceSupported() const { return GetCreateInstanceSupported(nullptr); }

        /**
         * @brief Returns whether changing a value on this object requires a call to
         *        CreateInstance to create a new value, using the specified context.
         *
         * C++ counterpart of .NET TypeConverter.GetCreateInstanceSupported(ITypeDescriptorContext).
         * @param context The format context, or null.
         * @return false in the base converter.
         */
        [[nodiscard]] virtual bool GetCreateInstanceSupported(ITypeDescriptorContext* context) const;

        /**
         * @brief Returns a collection of properties for the type of array specified by the
         *        value parameter.
         *
         * C++ counterpart of .NET TypeConverter.GetProperties(object).
         * @param value The boxed value whose properties are requested.
         * @return The properties; empty when the converter exposes none.
         */
        [[nodiscard]] PropertyDescriptorCollection GetProperties(const std::any& value) const {
            static const AttributeCollection browsableOnly({
                std::make_shared<BrowsableAttribute>(BrowsableAttribute::Yes)});
            return GetProperties(nullptr, value, browsableOnly);
        }

        /**
         * @brief Returns a collection of properties for the type of array specified by the
         *        value parameter, using the specified context.
         *
         * C++ counterpart of .NET TypeConverter.GetProperties(ITypeDescriptorContext, object).
         * @param context The format context, or null.
         * @param value The boxed value whose properties are requested.
         * @return The properties; empty when the converter exposes none.
         */
        [[nodiscard]] PropertyDescriptorCollection GetProperties(ITypeDescriptorContext* context, const std::any& value) const {
            static const AttributeCollection browsableOnly({
                std::make_shared<BrowsableAttribute>(BrowsableAttribute::Yes)});
            return GetProperties(context, value, browsableOnly);
        }

        /**
         * @brief Returns a collection of properties for the type of array specified by the
         *        value parameter, using the specified context and attributes.
         *
         * C++ counterpart of .NET TypeConverter.GetProperties(ITypeDescriptorContext, object,
         * Attribute[]). The base converter returns the empty collection (.NET's `null`).
         * @param context The format context, or null.
         * @param value The boxed value whose properties are requested.
         * @param attributes The attributes a property must match; empty means no filter.
         * @return The properties.
         */
        [[nodiscard]] virtual PropertyDescriptorCollection GetProperties(ITypeDescriptorContext* context,
                                                                        const std::any& value,
                                                                        const AttributeCollection& attributes) const;

        /**
         * @brief Returns whether this object supports properties.
         *
         * C++ counterpart of .NET TypeConverter.GetPropertiesSupported().
         * @return true if `GetProperties` should be called.
         */
        [[nodiscard]] bool GetPropertiesSupported() const { return GetPropertiesSupported(nullptr); }

        /**
         * @brief Returns whether this object supports properties, using the specified context.
         *
         * C++ counterpart of .NET TypeConverter.GetPropertiesSupported(ITypeDescriptorContext).
         * @param context The format context, or null.
         * @return false in the base converter.
         */
        [[nodiscard]] virtual bool GetPropertiesSupported(ITypeDescriptorContext* context) const;

        /**
         * @brief Returns a collection of standard values from the default context for the data
         *        type this type converter is designed for.
         *
         * C++ counterpart of .NET TypeConverter.GetStandardValues().
         * @return The standard values; empty when there are none.
         */
        [[nodiscard]] StandardValuesCollection GetStandardValues() const;

        /**
         * @brief Returns a collection of standard values for the data type this type converter
         *        is designed for when provided with a format context.
         *
         * C++ counterpart of .NET TypeConverter.GetStandardValues(ITypeDescriptorContext). The
         * base converter has none.
         * @param context The format context, or null.
         * @return The standard values.
         */
        [[nodiscard]] virtual StandardValuesCollection GetStandardValues(ITypeDescriptorContext* context) const;

        /**
         * @brief Returns whether the collection of standard values returned from
         *        GetStandardValues is an exclusive list.
         *
         * C++ counterpart of .NET TypeConverter.GetStandardValuesExclusive().
         * @return true if the list is exhaustive.
         */
        [[nodiscard]] bool GetStandardValuesExclusive() const { return GetStandardValuesExclusive(nullptr); }

        /**
         * @brief Returns whether the collection of standard values returned from
         *        GetStandardValues is an exclusive list of possible values, using the specified
         *        context.
         *
         * C++ counterpart of .NET TypeConverter.GetStandardValuesExclusive(ITypeDescriptorContext).
         * @param context The format context, or null.
         * @return false in the base converter.
         */
        [[nodiscard]] virtual bool GetStandardValuesExclusive(ITypeDescriptorContext* context) const;

        /**
         * @brief Returns whether this object supports a standard set of values that can be
         *        picked from a list.
         *
         * C++ counterpart of .NET TypeConverter.GetStandardValuesSupported().
         * @return true if `GetStandardValues` should be called.
         */
        [[nodiscard]] bool GetStandardValuesSupported() const { return GetStandardValuesSupported(nullptr); }

        /**
         * @brief Returns whether this object supports a standard set of values that can be
         *        picked from a list, using the specified context.
         *
         * C++ counterpart of .NET TypeConverter.GetStandardValuesSupported(ITypeDescriptorContext).
         * @param context The format context, or null.
         * @return false in the base converter.
         */
        [[nodiscard]] virtual bool GetStandardValuesSupported(ITypeDescriptorContext* context) const;

        /**
         * @brief Returns whether the given value object is valid for this type.
         *
         * C++ counterpart of .NET TypeConverter.IsValid(object).
         * @param value The boxed value.
         * @return true if the value is valid.
         */
        [[nodiscard]] bool IsValid(const std::any& value) const { return IsValid(nullptr, value); }

        /**
         * @brief Returns whether the given value object is valid for this type and for the
         *        specified context.
         *
         * C++ counterpart of .NET TypeConverter.IsValid(ITypeDescriptorContext, object): the
         * value is valid when it can be converted from, and the conversion succeeds.
         * @param context The format context, or null.
         * @param value The boxed value.
         * @return true if the value is valid.
         */
        [[nodiscard]] virtual bool IsValid(ITypeDescriptorContext* context, const std::any& value) const;

        /**
         * @brief Represents a collection of values.
         *
         * C++ counterpart of .NET TypeConverter.StandardValuesCollection: an ordered list of
         * boxed values.
         */
        class StandardValuesCollection {
        public:
            /** @brief Creates an empty collection. */
            StandardValuesCollection() = default;

            /**
             * @brief Initializes the collection with the given values.
             *
             * C++ counterpart of .NET StandardValuesCollection(ICollection).
             * @param values The values, in order.
             */
            explicit StandardValuesCollection(std::vector<std::any> values) : values_(std::move(values)) {}

            /**
             * @brief Gets the number of objects in the collection.
             *
             * C++ counterpart of .NET StandardValuesCollection.Count.
             * @return The number of values.
             */
            [[nodiscard]] SharpRuntime::intcs getCountProperty() const noexcept {
                return static_cast<SharpRuntime::intcs>(values_.size());
            }

            /**
             * @brief Gets the object at the specified index number.
             *
             * C++ counterpart of .NET StandardValuesCollection.this[int].
             * @param index The zero-based index.
             * @return The boxed value.
             * @throws System::ArgumentOutOfRangeException if @p index is out of range.
             */
            [[nodiscard]] const std::any& getItem(SharpRuntime::intcs index) const;

            /** @brief Iteration support: the first value. */
            [[nodiscard]] std::vector<std::any>::const_iterator begin() const noexcept { return values_.begin(); }

            /** @brief Iteration support: one past the last value. */
            [[nodiscard]] std::vector<std::any>::const_iterator end() const noexcept { return values_.end(); }

        private:
            std::vector<std::any> values_;
        };

        /**
         * @brief Represents an abstract class that provides properties for objects that do not
         *        have properties.
         *
         * C++ counterpart of .NET TypeConverter.SimplePropertyDescriptor: a descriptor whose
         * component type, name and property type are given at construction; a derived class
         * supplies `GetValue` and `SetValue`.
         */
        class SimplePropertyDescriptor : public PropertyDescriptor {
        public:
            /** @copydoc PropertyDescriptor::getComponentTypeProperty */
            [[nodiscard]] System::Type getComponentTypeProperty() const override { return componentType_; }

            /**
             * @brief Gets whether this property is read-only: true exactly when the attributes
             *        carry `ReadOnlyAttribute::Yes`.
             *
             * C++ counterpart of .NET SimplePropertyDescriptor.IsReadOnly.
             */
            [[nodiscard]] bool getIsReadOnlyProperty() const override {
                return getAttributesProperty().Contains(ReadOnlyAttribute::Yes);
            }

            /** @copydoc PropertyDescriptor::getPropertyTypeProperty */
            [[nodiscard]] System::Type getPropertyTypeProperty() const override { return propertyType_; }

            /**
             * @brief Returns whether resetting the component changes the value of the component.
             *
             * The .NET implementation returns true when a `DefaultValueAttribute` is present
             * and its value equals the current value. This counterintuitive reference behavior
             * is preserved exactly.
             *
             * C++ counterpart of .NET SimplePropertyDescriptor.CanResetValue(object).
             */
            [[nodiscard]] bool CanResetValue(const std::any& component) const override;

            /**
             * @brief Resets the value for this property of the component to the
             *        `DefaultValueAttribute` value, when there is one.
             *
             * C++ counterpart of .NET SimplePropertyDescriptor.ResetValue(object).
             */
            void ResetValue(std::any& component) const override;

            /**
             * @brief Returns whether the value of this property needs to be persisted: never.
             *
             * C++ counterpart of .NET SimplePropertyDescriptor.ShouldSerializeValue(object).
             */
            [[nodiscard]] bool ShouldSerializeValue(const std::any& component) const override {
                (void)component;
                return false;
            }

        protected:
            /**
             * @brief Initializes a new instance with the specified component type, name and
             *        property type.
             *
             * C++ counterpart of .NET SimplePropertyDescriptor(Type, string, Type).
             */
            SimplePropertyDescriptor(System::Type componentType, std::string name, System::Type propertyType)
                : SimplePropertyDescriptor(componentType, std::move(name), propertyType, AttributeCollection{}) {}

            /**
             * @brief Initializes a new instance with the specified component type, name,
             *        property type and attributes.
             *
             * C++ counterpart of .NET SimplePropertyDescriptor(Type, string, Type, Attribute[]).
             */
            SimplePropertyDescriptor(System::Type componentType, std::string name, System::Type propertyType,
                                     AttributeCollection attributes)
                : PropertyDescriptor(std::move(name), std::move(attributes)),
                  componentType_(componentType), propertyType_(propertyType) {}

        private:
            System::Type componentType_;
            System::Type propertyType_;
        };

    protected:
        /**
         * @brief Returns an exception to throw when a conversion cannot be performed.
         *
         * C++ counterpart of .NET TypeConverter.GetConvertFromException(object): a
         * `NotSupportedException` reading `"<Converter> cannot convert from <type>."`.
         * @param value The value that could not be converted.
         * @return The exception to throw.
         */
        [[nodiscard]] System::NotSupportedException GetConvertFromException(const std::any& value) const;

        /**
         * @brief Returns an exception to throw when a conversion cannot be performed.
         *
         * C++ counterpart of .NET TypeConverter.GetConvertToException(object, Type): a
         * `NotSupportedException` reading `"'<Converter>' is unable to convert '<type>' to '<destination>'."`.
         * @param value The value that could not be converted.
         * @param destinationType The type it could not be converted to.
         * @return The exception to throw.
         */
        [[nodiscard]] System::NotSupportedException GetConvertToException(const std::any& value,
                                                                          const System::Type& destinationType) const;

        /**
         * @brief Sorts a collection of properties.
         *
         * C++ counterpart of .NET TypeConverter.SortProperties(PropertyDescriptorCollection, string[]).
         * @param props The properties to sort.
         * @param names The property names to put first, in order.
         * @return The sorted collection.
         */
        [[nodiscard]] PropertyDescriptorCollection SortProperties(const PropertyDescriptorCollection& props,
                                                                  const std::vector<std::string>& names) const {
            return props.Sort(names);
        }
    };

} // namespace System::ComponentModel
