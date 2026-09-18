// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <memory>
#include <utility>
#include <vector>
#include "System/ComponentModel/AttributeCollection.hpp"
#include "System/ComponentModel/PropertyDescriptorCollection.hpp"
#include "System/ComponentModel/TypeConverter.hpp"
#include "System/Type.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides information about the characteristics for a component, such as its
     *        attributes, properties, and events. This class cannot be inherited.
     *
     * C++ counterpart of .NET System.ComponentModel.TypeDescriptor, over **explicit
     * registration instead of reflection**. .NET reads a type's `[TypeConverter]` attribute and
     * its public properties from metadata; this runtime has no such metadata, so a type's author
     * (or the library that owns it) registers what .NET would have discovered:
     *
     * @code
     * // the C++ spelling of [TypeConverter(typeof(Vector3Converter))] on Vector3
     * TypeDescriptor::RegisterType<Vector3>({std::make_shared<TypeConverterAttribute>(
     *     TypeConverterAttribute::Of<Vector3Converter>())});
     * auto converter = TypeDescriptor::GetConverter(System::Type::From<Vector3>());  // a Vector3Converter
     * @endcode
     *
     * `RegisterType<T>` is .NET 7's own name for "supply this type's metadata without
     * reflection"; `AddAttributes` is .NET's run-time way of attaching attributes to a type.
     * Both go into one process-wide registry:
     *
     *  - **lookup** is by `System::Type`, under a mutex, and is safe from any thread;
     *  - **converters are created lazily** on the first `GetConverter` for a type and cached,
     *    so a program that never asks pays only the registration itself (one entry per type);
     *    the factory runs outside the lock, so a converter's constructor may itself consult
     *    `TypeDescriptor`;
     *  - **the registry is a function-local static**, initialised on first use, so registration
     *    from a static initialiser in any translation unit is safe regardless of link order;
     *  - **replacement is deterministic**: a later completed `AddAttributes` replaces an earlier
     *    attribute of the same type (as .NET's does) and drops the cached converter.
     *
     * Types this runtime boxes as primitives (`bool`, the eight integer aliases, `float`,
     * `double`, `std::string`) have intrinsic converters exactly as in .NET
     * (`Int32Converter`, `SingleConverter`, `StringConverter`, ...), consulted when no
     * `TypeConverterAttribute` is registered. Every other unregistered type gets the plain
     * `TypeConverter`, which is what .NET returns for a type with no converter of its own.
     *
     * Not ported: `TypeDescriptionProvider`, `ICustomTypeDescriptor`, events, editors, and the
     * component/site model. See `docs/ComponentModelDesign.md`.
     */
    class TypeDescriptor final {
    public:
        TypeDescriptor() = delete;

        /**
         * @brief Registers the metadata of @p T: its attributes and its property descriptors.
         *
         * C++ counterpart of .NET TypeDescriptor.RegisterType<T>(), which in .NET makes a
         * type's reflection metadata available without reflection; here the metadata is the
         * argument. Registering a type twice merges: new attributes replace same-typed ones, and
         * a non-empty @p properties replaces the registered collection.
         * @tparam T The type being described.
         * @param attributes The type's attributes; a `TypeConverterAttribute` with a factory
         *        associates the converter.
         * @param properties The type's property descriptors, as `GetProperties` returns them.
         */
        template<class T>
        static void RegisterType(std::vector<AttributeCollection::Element> attributes = {},
                                 PropertyDescriptorCollection properties = {}) {
            RegisterType(System::Type::From<T>(), std::move(attributes), std::move(properties));
        }

        /**
         * @brief Registers the metadata of @p type; the non-template form of `RegisterType<T>`.
         * @param type The type being described.
         * @param attributes The type's attributes.
         * @param properties The type's property descriptors.
         */
        static void RegisterType(const System::Type& type, std::vector<AttributeCollection::Element> attributes,
                                 PropertyDescriptorCollection properties);

        /**
         * @brief Adds class-level attributes to the target component type.
         *
         * C++ counterpart of .NET TypeDescriptor.AddAttributes(Type, params Attribute[]). An
         * added attribute replaces a registered attribute of the same type, and the type's cached
         * converter is dropped so a new `TypeConverterAttribute` takes effect.
         * @param type The type to attach the attributes to.
         * @param attributes The attributes to add.
         */
        static void AddAttributes(const System::Type& type, std::vector<AttributeCollection::Element> attributes);

        /**
         * @brief Returns a collection of attributes for the specified type of component.
         *
         * C++ counterpart of .NET TypeDescriptor.GetAttributes(Type).
         * @param componentType The type.
         * @return The registered attributes; empty for an unregistered type.
         */
        [[nodiscard]] static AttributeCollection GetAttributes(const System::Type& componentType);

        /**
         * @brief Returns the collection of attributes for the specified component.
         *
         * C++ counterpart of .NET TypeDescriptor.GetAttributes(object).
         * @param component The boxed component.
         * @return The registered attributes of its type; empty for an empty value.
         */
        [[nodiscard]] static AttributeCollection GetAttributes(const std::any& component);

        /**
         * @brief Returns a type converter for the specified type.
         *
         * C++ counterpart of .NET TypeDescriptor.GetConverter(Type): the converter the type's
         * registered `TypeConverterAttribute` creates, else the intrinsic converter for a
         * primitive, else the plain `TypeConverter`. Never null; the same instance is returned
         * on every call until the type's registration changes.
         * @param type The type.
         * @return The converter.
         */
        [[nodiscard]] static std::shared_ptr<TypeConverter> GetConverter(const System::Type& type);

        /**
         * @brief Returns a type converter for the type of the specified component.
         *
         * C++ counterpart of .NET TypeDescriptor.GetConverter(object).
         * @param component The boxed component.
         * @return The converter for its dynamic type; the plain `TypeConverter` for an empty value.
         */
        [[nodiscard]] static std::shared_ptr<TypeConverter> GetConverter(const std::any& component);

        /**
         * @brief Returns the collection of properties for a specified type of component.
         *
         * C++ counterpart of .NET TypeDescriptor.GetProperties(Type).
         * @param componentType The type.
         * @return The registered properties; empty for an unregistered type.
         */
        [[nodiscard]] static PropertyDescriptorCollection GetProperties(const System::Type& componentType);

        /**
         * @brief Returns the collection of properties for a specified type of component using a
         *        specified array of attributes as a filter.
         *
         * C++ counterpart of .NET TypeDescriptor.GetProperties(Type, Attribute[]): a property is
         * kept when, for every filter attribute, it carries a matching attribute of that type, or
         * carries none and the filter attribute is its type's default.
         * @param componentType The type.
         * @param attributes The filter; empty means no filter.
         * @return The matching registered properties.
         */
        [[nodiscard]] static PropertyDescriptorCollection GetProperties(const System::Type& componentType,
                                                                        const AttributeCollection& attributes);

        /**
         * @brief Returns the collection of properties for a specified component.
         *
         * C++ counterpart of .NET TypeDescriptor.GetProperties(object).
         * @param component The boxed component.
         * @return The registered properties of its type.
         * @throws System::ArgumentNullException if @p component is empty.
         */
        [[nodiscard]] static PropertyDescriptorCollection GetProperties(const std::any& component);

        /**
         * @brief Returns the collection of properties for a specified component using a
         *        specified array of attributes as a filter.
         *
         * C++ counterpart of .NET TypeDescriptor.GetProperties(object, Attribute[]).
         * @param component The boxed component.
         * @param attributes The filter; empty means no filter.
         * @return The matching registered properties of its type.
         * @throws System::ArgumentNullException if @p component is empty.
         */
        [[nodiscard]] static PropertyDescriptorCollection GetProperties(const std::any& component,
                                                                        const AttributeCollection& attributes);

        /**
         * @brief Clears the properties and events for the specified type from the cache.
         *
         * C++ counterpart of .NET TypeDescriptor.Refresh(Type): drops the cached converter so the
         * next `GetConverter` creates a fresh one; the registration itself is kept.
         * @param type The type to refresh.
         */
        static void Refresh(const System::Type& type);
    };

} // namespace System::ComponentModel
