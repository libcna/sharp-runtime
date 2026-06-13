// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System {

    /**
     * @brief Provides a mechanism for formatting the value of an object using a format string.
     *
     * Partial C++ counterpart of .NET System.IFormattable.
     *
     * @note Status: Stub
     */
    class IFormattable {
    public:
        /// Virtual destructor for safe polymorphic destruction.
        virtual ~IFormattable() = default;
        /// Formats the value of the current instance using the specified format.
        [[nodiscard]] virtual std::string ToString(const std::string& format) const = 0;
    };

} // namespace System
