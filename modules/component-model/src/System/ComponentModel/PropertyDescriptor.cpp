// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/ComponentModel/PropertyDescriptor.hpp"

#include "System/ComponentModel/PropertyDescriptorCollection.hpp"
#include "System/ComponentModel/TypeConverter.hpp"
#include "System/ComponentModel/TypeConverterAttribute.hpp"
#include "System/ComponentModel/TypeDescriptor.hpp"

namespace System::ComponentModel {

std::shared_ptr<TypeConverter> PropertyDescriptor::getConverterProperty() const {
    // PropertyDescriptor.cs: a TypeConverterAttribute on the property wins over the
    // property type's converter. Only an attribute that can create its converter can win
    // here; a name-only attribute is metadata, exactly as one naming an unloadable type is
    // in .NET, and the type's converter is used instead.
    if (const auto* attribute = getAttributesProperty().getItem<TypeConverterAttribute>()) {
        if (auto converter = attribute->CreateConverter()) return converter;
    }
    return TypeDescriptor::GetConverter(getPropertyTypeProperty());
}

PropertyDescriptorCollection PropertyDescriptor::GetChildProperties() const {
    return GetChildProperties(std::any{}, AttributeCollection::Empty);
}

PropertyDescriptorCollection PropertyDescriptor::GetChildProperties(const AttributeCollection& filter) const {
    return GetChildProperties(std::any{}, filter);
}

PropertyDescriptorCollection PropertyDescriptor::GetChildProperties(const std::any& instance) const {
    return GetChildProperties(instance, AttributeCollection::Empty);
}

PropertyDescriptorCollection PropertyDescriptor::GetChildProperties(const std::any& instance,
                                                                    const AttributeCollection& filter) const {
    if (!instance.has_value()) {
        return TypeDescriptor::GetProperties(getPropertyTypeProperty(), filter);
    }
    return TypeDescriptor::GetProperties(instance, filter);
}

} // namespace System::ComponentModel
