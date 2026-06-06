// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <typeinfo>

namespace System
{
    /// Provides access to services identified by their runtime type.
    class IServiceProvider
    {
    public:
        virtual ~IServiceProvider() = default;

        /// Gets a service object for the requested type, or nullptr when it is not registered.
        [[nodiscard]] virtual void* GetService(const std::type_info& type) const = 0;
    };
}
