// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/ComponentModel/TypeConverter.hpp"

#include <typeinfo>
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ComponentModel/DefaultValueAttribute.hpp"
#include "System/ComponentModel/Design/Serialization/InstanceDescriptor.hpp"
#include "System/ComponentModel/detail/ObjectText.hpp"
#include "System/Exception.hpp"

namespace System::ComponentModel {

using System::ComponentModel::Design::Serialization::InstanceDescriptor;

namespace {

std::string converterName(const TypeConverter& converter) {
    return System::Type::FromTypeInfo(typeid(converter)).getNameProperty();
}

} // namespace

bool TypeConverter::CanConvertFrom(ITypeDescriptorContext* context, const System::Type& sourceType) const {
    (void)context;
    return sourceType == System::Type::From<InstanceDescriptor>();
}

bool TypeConverter::CanConvertTo(ITypeDescriptorContext* context, const System::Type& destinationType) const {
    (void)context;
    return destinationType == System::Type::From<std::string>();
}

std::any TypeConverter::ConvertFrom(ITypeDescriptorContext* context,
                                    const System::Globalization::CultureInfo* culture,
                                    const std::any& value) const {
    (void)context;
    (void)culture;
    if (const auto* descriptor = std::any_cast<InstanceDescriptor>(&value)) {
        return descriptor->Invoke();
    }
    throw GetConvertFromException(value);
}

std::any TypeConverter::ConvertTo(ITypeDescriptorContext* context,
                                  const System::Globalization::CultureInfo* culture,
                                  const std::any& value,
                                  const System::Type& destinationType) const {
    (void)context;
    if (destinationType == System::Type()) {
        throw System::ArgumentNullException("destinationType");
    }
    if (destinationType == System::Type::From<std::string>()) {
        if (!value.has_value()) return std::any(std::string{});
        // .NET formats an IFormattable with the culture only when one was given and it is not
        // the current culture; otherwise value.ToString(), which formats with the current
        // culture. Both collapse to "format with culture-or-current" here.
        return std::any(detail::ObjectText::ToString(value, culture));
    }
    throw GetConvertToException(value, destinationType);
}

std::string TypeConverter::ConvertToString(ITypeDescriptorContext* context,
                                           const System::Globalization::CultureInfo* culture,
                                           const std::any& value) const {
    const std::any text = ConvertTo(context, culture, value, System::Type::From<std::string>());
    if (const auto* s = std::any_cast<std::string>(&text)) return *s;
    if (auto s = detail::ObjectText::AsString(text)) return *s;
    return {};
}

std::any TypeConverter::CreateInstance(ITypeDescriptorContext* context,
                                       const System::Collections::Hashtable& propertyValues) const {
    (void)context;
    (void)propertyValues;
    return {};
}

bool TypeConverter::GetCreateInstanceSupported(ITypeDescriptorContext* context) const {
    (void)context;
    return false;
}

PropertyDescriptorCollection TypeConverter::GetProperties(ITypeDescriptorContext* context,
                                                          const std::any& value,
                                                          const AttributeCollection& attributes) const {
    (void)context;
    (void)value;
    (void)attributes;
    return {};
}

bool TypeConverter::GetPropertiesSupported(ITypeDescriptorContext* context) const {
    (void)context;
    return false;
}

TypeConverter::StandardValuesCollection TypeConverter::GetStandardValues() const {
    return GetStandardValues(nullptr);
}

TypeConverter::StandardValuesCollection TypeConverter::GetStandardValues(ITypeDescriptorContext* context) const {
    (void)context;
    return {};
}

bool TypeConverter::GetStandardValuesExclusive(ITypeDescriptorContext* context) const {
    (void)context;
    return false;
}

bool TypeConverter::GetStandardValuesSupported(ITypeDescriptorContext* context) const {
    (void)context;
    return false;
}

bool TypeConverter::IsValid(ITypeDescriptorContext* context, const std::any& value) const {
    // TypeConverter.cs deliberately tries ConvertFrom for null because a converter such as
    // NullableConverter may accept it even though null has no type to ask CanConvertFrom about.
    if (value.has_value() && !CanConvertFrom(context, System::Type::FromTypeInfo(value.type()))) return false;
    try {
        (void)ConvertFrom(context, &System::Globalization::CultureInfo::getInvariantCultureProperty(), value);
        return true;
    } catch (...) {
        return false;
    }
}

System::NotSupportedException TypeConverter::GetConvertFromException(const std::any& value) const {
    return System::NotSupportedException(converterName(*this) + " cannot convert from " +
                                         detail::ObjectText::TypeName(value) + ".");
}

System::NotSupportedException TypeConverter::GetConvertToException(const std::any& value,
                                                                   const System::Type& destinationType) const {
    return System::NotSupportedException("'" + converterName(*this) + "' is unable to convert '" +
                                         detail::ObjectText::TypeName(value) + "' to '" +
                                         destinationType.getFullNameProperty() + "'.");
}

const std::any& TypeConverter::StandardValuesCollection::getItem(SharpRuntime::intcs index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= values_.size()) {
        throw System::ArgumentOutOfRangeException("index");
    }
    return values_[static_cast<std::size_t>(index)];
}

bool TypeConverter::SimplePropertyDescriptor::CanResetValue(const std::any& component) const {
    const auto* defaultValue = getAttributesProperty().getItem<DefaultValueAttribute>();
    if (defaultValue == nullptr) return false;
    return DefaultValueAttribute(defaultValue->getValueProperty()).Equals(
        DefaultValueAttribute(GetValue(component)));
}

void TypeConverter::SimplePropertyDescriptor::ResetValue(std::any& component) const {
    const auto* defaultValue = getAttributesProperty().getItem<DefaultValueAttribute>();
    if (defaultValue != nullptr) {
        SetValue(component, defaultValue->getValueProperty());
    }
}

} // namespace System::ComponentModel
