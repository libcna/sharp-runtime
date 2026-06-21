// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System {

    /**
     * @brief Provides a mechanism for formatting the value of an object into a
     * string representation using a format string.
     *
     * C++ counterpart of .NET System.IFormattable.
     */
    class IFormattable {
    public:
        /** @brief Virtual destructor for safe polymorphic destruction. */
        virtual ~IFormattable() = default;

        /**
         * @brief Formats the value of the current instance using the specified format.
         *
         * C++ counterpart of .NET IFormattable.ToString(string, IFormatProvider).
         * The format-provider parameter is omitted because sharp-runtime has no
         * IFormatProvider equivalent.
         * @param format A format string, or empty for the default format.
         * @return The value of the current instance in the specified format.
         */
        [[nodiscard]] virtual std::string ToString(const std::string& format) const = 0;
    };

} // namespace System
