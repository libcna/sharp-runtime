// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include "System/IServiceProvider.hpp"

namespace System::ComponentModel {

    class PropertyDescriptor;

    /**
     * @brief Provides contextual information about a component, such as its container and
     *        property descriptor.
     *
     * C++ counterpart of .NET System.ComponentModel.ITypeDescriptorContext. A designer or
     * property grid implements it and passes it to `TypeConverter` and `PropertyDescriptor`
     * calls; the converters this runtime ships never require one, so every context parameter
     * accepts `nullptr`, which is .NET's `null`.
     *
     * .NET's interface also exposes `Container` (an `IContainer`). This runtime has no
     * `IComponent`/`IContainer`/`ISite` model, so that member is not declared rather than
     * declared with a type that could never be implemented; it is the one omission from the
     * .NET surface, recorded in `docs/ComponentModelDesign.md`.
     */
    class ITypeDescriptorContext : public System::IServiceProvider {
    public:
        /**
         * @brief Gets the object that is connected with this type descriptor request.
         *
         * C++ counterpart of .NET ITypeDescriptorContext.Instance.
         * @return The boxed instance, or an empty `std::any` when there is none.
         */
        [[nodiscard]] virtual std::any getInstanceProperty() const = 0;

        /**
         * @brief Gets the PropertyDescriptor that is associated with the given context item.
         *
         * C++ counterpart of .NET ITypeDescriptorContext.PropertyDescriptor.
         * @return The descriptor, or null when the request is not about a property.
         */
        [[nodiscard]] virtual const PropertyDescriptor* getPropertyDescriptorProperty() const = 0;

        /**
         * @brief Raises the ComponentChanging event.
         *
         * C++ counterpart of .NET ITypeDescriptorContext.OnComponentChanging().
         * @return true if the object can be changed; false if it cannot.
         */
        virtual bool OnComponentChanging() = 0;

        /**
         * @brief Raises the ComponentChanged event.
         *
         * C++ counterpart of .NET ITypeDescriptorContext.OnComponentChanged().
         */
        virtual void OnComponentChanged() = 0;
    };

} // namespace System::ComponentModel
