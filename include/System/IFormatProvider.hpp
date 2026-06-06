// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <typeinfo>

namespace System {

    class IFormatProvider {
    public:
        virtual ~IFormatProvider() = default;
        // Returns a formatting object for the given type, or nullptr if not supported.
        [[nodiscard]] virtual void* GetFormat(const std::type_info& formatType) const = 0;
    };

} // namespace System
