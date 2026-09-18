// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <memory>
#include <string>
#include <utility>
#include "System/ComponentModel/AttributeCollection.hpp"
#include "System/ComponentModel/MemberDescriptor.hpp"
#include "System/EventArgs.hpp"
#include "System/Type.hpp"

namespace System::ComponentModel {

    class PropertyDescriptorCollection;
    class TypeConverter;

    /**
     * @brief Provides an abstraction of a property on a class.
     *
     * C++ counterpart of .NET System.ComponentModel.PropertyDescriptor. A descriptor names a
     * property, states its type and its component's type, and reads and writes it on a boxed
     * component. .NET's `ReflectPropertyDescriptor` discovers all of that; here a derived class
     * states it -- for a public field, a pointer to member; for a property, the getter and
     * setter -- and the descriptor remains a complete, working description of the property.
     *
     * Values and components travel as `std::any`, this runtime's `object`. `SetValue` takes the
     * component by non-const reference and mutates it in place, which for a value-type component
     * is strictly more useful than .NET's write into a boxed copy; a caller that needs .NET's
     * "recreate the struct" behaviour uses `TypeConverter::CreateInstance`, exactly as a
     * property grid does.
     *
     * Value-changed notification (.NET's `AddValueChanged`/`RemoveValueChanged`) keys handlers on
     * the component's object identity, which a `std::any` value does not have; those two members
     * are therefore not declared, and `OnValueChanged` remains a protected hook a derived class
     * can override. Recorded in `docs/ComponentModelDesign.md`.
     */
    class PropertyDescriptor : public MemberDescriptor {
    public:
        /**
         * @brief Gets the type of the component this property is bound to.
         *
         * C++ counterpart of .NET PropertyDescriptor.ComponentType.
         * @return The component type.
         */
        [[nodiscard]] virtual System::Type getComponentTypeProperty() const = 0;

        /**
         * @brief Gets the type converter for this property.
         *
         * C++ counterpart of .NET PropertyDescriptor.Converter: the converter a
         * `TypeConverterAttribute` among this property's attributes creates, otherwise
         * `TypeDescriptor::GetConverter(PropertyType)`.
         * @return The converter; never null.
         */
        [[nodiscard]] virtual std::shared_ptr<TypeConverter> getConverterProperty() const;

        /**
         * @brief Gets a value indicating whether this property should be localized, as
         *        specified in the LocalizableAttribute.
         *
         * C++ counterpart of .NET PropertyDescriptor.IsLocalizable.
         * @return true if a LocalizableAttribute says so.
         */
        [[nodiscard]] virtual bool getIsLocalizableProperty() const {
            const auto* localizable = getAttributesProperty().getItem<LocalizableAttribute>();
            return localizable != nullptr && localizable->getIsLocalizableProperty();
        }

        /**
         * @brief Gets a value indicating whether this property is read-only.
         *
         * C++ counterpart of .NET PropertyDescriptor.IsReadOnly.
         * @return true if the property cannot be set.
         */
        [[nodiscard]] virtual bool getIsReadOnlyProperty() const = 0;

        /**
         * @brief Gets the type of the property.
         *
         * C++ counterpart of .NET PropertyDescriptor.PropertyType.
         * @return The property type.
         */
        [[nodiscard]] virtual System::Type getPropertyTypeProperty() const = 0;

        /**
         * @brief Gets a value indicating whether value change notifications for this property
         *        may originate from outside the property descriptor.
         *
         * C++ counterpart of .NET PropertyDescriptor.SupportsChangeEvents.
         * @return false unless a derived class says otherwise.
         */
        [[nodiscard]] virtual bool getSupportsChangeEventsProperty() const { return false; }

        /**
         * @brief Returns whether resetting an object changes its value.
         *
         * C++ counterpart of .NET PropertyDescriptor.CanResetValue(object).
         * @param component The boxed component.
         * @return true if resetting the component changes its value.
         */
        [[nodiscard]] virtual bool CanResetValue(const std::any& component) const = 0;

        /**
         * @brief Gets the current value of the property on a component.
         *
         * C++ counterpart of .NET PropertyDescriptor.GetValue(object).
         * @param component The boxed component.
         * @return The boxed property value.
         */
        [[nodiscard]] virtual std::any GetValue(const std::any& component) const = 0;

        /**
         * @brief Resets the value for this property of the component to the default value.
         *
         * C++ counterpart of .NET PropertyDescriptor.ResetValue(object).
         * @param component The boxed component, mutated in place.
         */
        virtual void ResetValue(std::any& component) const = 0;

        /**
         * @brief Sets the value of the component to a different value.
         *
         * C++ counterpart of .NET PropertyDescriptor.SetValue(object, object).
         * @param component The boxed component, mutated in place.
         * @param value The boxed new value.
         */
        virtual void SetValue(std::any& component, const std::any& value) const = 0;

        /**
         * @brief Determines a value indicating whether the value of this property needs to be
         *        persisted.
         *
         * C++ counterpart of .NET PropertyDescriptor.ShouldSerializeValue(object).
         * @param component The boxed component.
         * @return true if the property should be persisted.
         */
        [[nodiscard]] virtual bool ShouldSerializeValue(const std::any& component) const = 0;

        /**
         * @brief Returns the default PropertyDescriptorCollection.
         *
         * C++ counterpart of .NET PropertyDescriptor.GetChildProperties().
         * @return The child properties of the property type.
         */
        [[nodiscard]] PropertyDescriptorCollection GetChildProperties() const;

        /**
         * @brief Returns a PropertyDescriptorCollection using a specified array of attributes as
         *        a filter.
         *
         * C++ counterpart of .NET PropertyDescriptor.GetChildProperties(Attribute[]).
         * @param filter The attributes a child property must match.
         * @return The matching child properties.
         */
        [[nodiscard]] PropertyDescriptorCollection GetChildProperties(const AttributeCollection& filter) const;

        /**
         * @brief Returns a PropertyDescriptorCollection for a given object.
         *
         * C++ counterpart of .NET PropertyDescriptor.GetChildProperties(object).
         * @param instance The boxed value whose child properties are requested.
         * @return The child properties.
         */
        [[nodiscard]] PropertyDescriptorCollection GetChildProperties(const std::any& instance) const;

        /**
         * @brief Returns a PropertyDescriptorCollection for a given object using a specified
         *        array of attributes as a filter.
         *
         * C++ counterpart of .NET PropertyDescriptor.GetChildProperties(object, Attribute[]):
         * the registered properties of the instance's type (or of `PropertyType` when
         * @p instance is empty), filtered by @p filter.
         * @param instance The boxed value whose child properties are requested, or empty.
         * @param filter The attributes a child property must match; empty means no filter.
         * @return The matching child properties.
         */
        [[nodiscard]] virtual PropertyDescriptorCollection GetChildProperties(const std::any& instance,
                                                                             const AttributeCollection& filter) const;

        /**
         * @brief Compares this to another object to see if they are equivalent.
         *
         * C++ counterpart of .NET PropertyDescriptor.Equals(object): the same name and the same
         * property type.
         * @param other The descriptor to compare with.
         * @return true if the two describe the same property.
         */
        [[nodiscard]] bool Equals(const MemberDescriptor& other) const override {
            if (this == &other) return true;
            const auto* property = dynamic_cast<const PropertyDescriptor*>(&other);
            return property != nullptr && property->getNameProperty() == getNameProperty() &&
                   property->getPropertyTypeProperty() == getPropertyTypeProperty();
        }

        /**
         * @brief Returns the hash code for this object.
         *
         * C++ counterpart of .NET PropertyDescriptor.GetHashCode(): the name's hash combined with
         * the property type's.
         * @return The hash code.
         */
        [[nodiscard]] int GetHashCode() const override {
            return getNameHashCodeProperty() ^ static_cast<int>(getPropertyTypeProperty().GetHashCode());
        }

    protected:
        /**
         * @brief Initializes a new instance with the specified name and attributes.
         *
         * C++ counterpart of .NET PropertyDescriptor(string, Attribute[]).
         * @param name The property name.
         * @param attributes The attributes that describe the property.
         */
        explicit PropertyDescriptor(std::string name, AttributeCollection attributes = {})
            : MemberDescriptor(std::move(name), std::move(attributes)) {}

        /**
         * @brief Initializes a new instance with the name and attributes of another descriptor.
         *
         * C++ counterpart of .NET PropertyDescriptor(MemberDescriptor).
         * @param descr The descriptor to copy from.
         */
        explicit PropertyDescriptor(const MemberDescriptor& descr) : MemberDescriptor(descr) {}

        /**
         * @brief Initializes a new instance with another descriptor's name and the given
         *        attributes merged over its own.
         *
         * C++ counterpart of .NET PropertyDescriptor(MemberDescriptor, Attribute[]).
         * @param descr The descriptor to copy from.
         * @param attributes The attributes to merge.
         */
        PropertyDescriptor(const MemberDescriptor& descr, const std::vector<AttributeCollection::Element>& attributes)
            : MemberDescriptor(descr, attributes) {}

        /**
         * @brief Raises the ValueChanged event that you implemented.
         *
         * C++ counterpart of .NET PropertyDescriptor.OnValueChanged(object, EventArgs). The base
         * implementation does nothing; see the class comment for why no handler list exists.
         * @param component The boxed component whose value changed.
         * @param e The event arguments.
         */
        virtual void OnValueChanged(const std::any& component, const System::EventArgs& e) const {
            (void)component;
            (void)e;
        }
    };

} // namespace System::ComponentModel
