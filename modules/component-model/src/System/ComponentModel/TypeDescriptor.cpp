// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/ComponentModel/TypeDescriptor.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ComponentModel/BooleanConverter.hpp"
#include "System/ComponentModel/ByteConverter.hpp"
#include "System/ComponentModel/DoubleConverter.hpp"
#include "System/ComponentModel/Int16Converter.hpp"
#include "System/ComponentModel/Int32Converter.hpp"
#include "System/ComponentModel/Int64Converter.hpp"
#include "System/ComponentModel/SByteConverter.hpp"
#include "System/ComponentModel/SingleConverter.hpp"
#include "System/ComponentModel/StringConverter.hpp"
#include "System/ComponentModel/TypeConverterAttribute.hpp"
#include "System/ComponentModel/detail/ObjectText.hpp"
#include "System/ComponentModel/UInt16Converter.hpp"
#include "System/ComponentModel/UInt32Converter.hpp"
#include "System/ComponentModel/UInt64Converter.hpp"
#include "System/Uri.hpp"
#include "System/UriTypeConverter.hpp"

namespace System::ComponentModel {

namespace {

struct Registration {
    std::vector<AttributeCollection::Element> attributes;
    PropertyDescriptorCollection properties;
    std::shared_ptr<TypeConverter> converter;
};

struct Registry {
    std::mutex mutex;
    std::unordered_map<System::Type, Registration> entries;
    std::uint64_t generation = 0;
};

// A function-local static: constructed on first use, so a registration made from another
// translation unit's static initialiser finds it ready regardless of link order.
Registry& registry() {
    static Registry instance;
    return instance;
}

// The converters .NET associates with its primitives without any attribute
// (ReflectTypeDescriptionProvider's intrinsic table), over the types this runtime boxes.
std::shared_ptr<TypeConverter> intrinsicConverter(const System::Type& type) {
    if (type == System::Type::From<bool>()) return std::make_shared<BooleanConverter>();
    if (type == System::Type::From<SharpRuntime::bytecs>()) return std::make_shared<ByteConverter>();
    if (type == System::Type::From<SharpRuntime::sbytecs>()) return std::make_shared<SByteConverter>();
    if (type == System::Type::From<SharpRuntime::shortcs>()) return std::make_shared<Int16Converter>();
    if (type == System::Type::From<SharpRuntime::ushortcs>()) return std::make_shared<UInt16Converter>();
    if (type == System::Type::From<SharpRuntime::intcs>()) return std::make_shared<Int32Converter>();
    if (type == System::Type::From<SharpRuntime::uintcs>()) return std::make_shared<UInt32Converter>();
    if (type == System::Type::From<SharpRuntime::longcs>()) return std::make_shared<Int64Converter>();
    if (type == System::Type::From<SharpRuntime::ulongcs>()) return std::make_shared<UInt64Converter>();
    if (type == System::Type::From<float>()) return std::make_shared<SingleConverter>();
    if (type == System::Type::From<double>()) return std::make_shared<DoubleConverter>();
    if (detail::ObjectText::IsStringType(type)) {
        return std::make_shared<StringConverter>();
    }
    // Uri is in .NET's intrinsic table too (ReflectTypeDescriptionProvider.IntrinsicTypeConverters).
    if (type == System::Type::From<System::Uri>()) return std::make_shared<System::UriTypeConverter>();
    return nullptr;
}

std::shared_ptr<TypeConverter> defaultConverter() {
    static const std::shared_ptr<TypeConverter> instance = std::make_shared<TypeConverter>();
    return instance;
}

// TypeDescriptor.ShouldHideMember: a member is kept when, for each filter attribute, it
// carries a matching attribute of that type, or carries none and the filter is that type's
// default.
bool matchesFilter(const PropertyDescriptor& property, const AttributeCollection& filter) {
    for (const AttributeCollection::Element& wanted : filter) {
        const System::Attribute* present = property.getAttributesProperty().getItem(wanted->getTypeIdProperty());
        if (present == nullptr) {
            if (!wanted->getIsDefaultAttributeProperty()) return false;
        } else if (!wanted->Match(*present)) {
            return false;
        }
    }
    return true;
}

PropertyDescriptorCollection filterProperties(const PropertyDescriptorCollection& properties,
                                              const AttributeCollection& filter) {
    if (filter.getCountProperty() == 0) return properties;
    std::vector<PropertyDescriptorCollection::Element> kept;
    for (const auto& property : properties) {
        if (matchesFilter(*property, filter)) kept.push_back(property);
    }
    return PropertyDescriptorCollection(std::move(kept));
}

void mergeAttributes(std::vector<AttributeCollection::Element>& into,
                     std::vector<AttributeCollection::Element>&& added) {
    for (AttributeCollection::Element& attribute : added) {
        if (attribute == nullptr) continue;
        bool replaced = false;
        for (AttributeCollection::Element& existing : into) {
            if (existing->getTypeIdProperty() == attribute->getTypeIdProperty()) {
                existing = std::move(attribute);
                replaced = true;
                break;
            }
        }
        if (!replaced) into.push_back(std::move(attribute));
    }
}

} // namespace

void TypeDescriptor::RegisterType(const System::Type& type, std::vector<AttributeCollection::Element> attributes,
                                  PropertyDescriptorCollection properties) {
    Registry& reg = registry();
    const std::lock_guard<std::mutex> lock(reg.mutex);
    Registration& entry = reg.entries[type];
    mergeAttributes(entry.attributes, std::move(attributes));
    if (properties.getCountProperty() > 0) entry.properties = std::move(properties);
    entry.converter.reset();
    ++reg.generation;
}

void TypeDescriptor::AddAttributes(const System::Type& type, std::vector<AttributeCollection::Element> attributes) {
    RegisterType(type, std::move(attributes), PropertyDescriptorCollection{});
}

AttributeCollection TypeDescriptor::GetAttributes(const System::Type& componentType) {
    Registry& reg = registry();
    const std::lock_guard<std::mutex> lock(reg.mutex);
    const auto found = reg.entries.find(componentType);
    if (found == reg.entries.end()) return {};
    return AttributeCollection(found->second.attributes);
}

AttributeCollection TypeDescriptor::GetAttributes(const std::any& component) {
    if (!component.has_value()) return {};
    return GetAttributes(System::Type::FromTypeInfo(component.type()));
}

std::shared_ptr<TypeConverter> TypeDescriptor::GetConverter(const System::Type& type) {
    Registry& reg = registry();
    for (;;) {
        TypeConverterAttribute::ConverterFactory factory;
        std::uint64_t generation;
        {
            const std::lock_guard<std::mutex> lock(reg.mutex);
            generation = reg.generation;
            const auto found = reg.entries.find(type);
            if (found != reg.entries.end()) {
                if (found->second.converter) return found->second.converter;
                for (const AttributeCollection::Element& attribute : found->second.attributes) {
                    if (const auto* converterAttribute = dynamic_cast<const TypeConverterAttribute*>(attribute.get())) {
                        if (converterAttribute->getCanCreateConverterProperty()) {
                            factory = [converterAttribute, attribute] { return converterAttribute->CreateConverter(); };
                        }
                        break;
                    }
                }
            }
        }
        // Created outside the lock: a converter's constructor is user code and may consult
        // TypeDescriptor itself. If metadata changes while it runs, discard the stale result
        // and retry against the new generation.
        std::shared_ptr<TypeConverter> created = factory ? factory() : intrinsicConverter(type);
        if (!created) created = defaultConverter();
        {
            const std::lock_guard<std::mutex> lock(reg.mutex);
            if (reg.generation != generation) continue;
            Registration& entry = reg.entries[type];
            if (!entry.converter) entry.converter = std::move(created);
            return entry.converter;
        }
    }
}

std::shared_ptr<TypeConverter> TypeDescriptor::GetConverter(const std::any& component) {
    if (!component.has_value()) return defaultConverter();
    return GetConverter(System::Type::FromTypeInfo(component.type()));
}

PropertyDescriptorCollection TypeDescriptor::GetProperties(const System::Type& componentType) {
    return GetProperties(componentType, AttributeCollection::Empty);
}

PropertyDescriptorCollection TypeDescriptor::GetProperties(const System::Type& componentType,
                                                           const AttributeCollection& attributes) {
    PropertyDescriptorCollection registered;
    {
        Registry& reg = registry();
        const std::lock_guard<std::mutex> lock(reg.mutex);
        const auto found = reg.entries.find(componentType);
        if (found != reg.entries.end()) registered = found->second.properties;
    }
    return filterProperties(registered, attributes);
}

PropertyDescriptorCollection TypeDescriptor::GetProperties(const std::any& component) {
    return GetProperties(component, AttributeCollection::Empty);
}

PropertyDescriptorCollection TypeDescriptor::GetProperties(const std::any& component,
                                                           const AttributeCollection& attributes) {
    if (!component.has_value()) throw System::ArgumentNullException("component");
    return GetProperties(System::Type::FromTypeInfo(component.type()), attributes);
}

void TypeDescriptor::Refresh(const System::Type& type) {
    Registry& reg = registry();
    const std::lock_guard<std::mutex> lock(reg.mutex);
    const auto found = reg.entries.find(type);
    if (found != reg.entries.end()) {
        found->second.converter.reset();
        ++reg.generation;
    }
}

} // namespace System::ComponentModel
