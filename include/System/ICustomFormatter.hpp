// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <memory>
#include "System/IFormatProvider.hpp"

namespace System {

    /// Defines a method that supports custom formatting of the value of an object.
    class ICustomFormatter {
    public:
        /// Virtual destructor for safe polymorphic destruction.
        virtual ~ICustomFormatter() = default;
        /// Converts the value of the specified object to an equivalent string representation using the specified format and culture-specific formatting information.
        [[nodiscard]] virtual std::string Format(
            const std::string& format,
            const void* arg,
            const IFormatProvider* formatProvider) const = 0;
    };

} // namespace System
