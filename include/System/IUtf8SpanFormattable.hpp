// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>
#include "System/Span.hpp"

namespace System {

    /**
     * @brief Provides functionality to format the string representation of an object
     * into a span of UTF-8 bytes.
     *
     * C++ counterpart of .NET System.IUtf8SpanFormattable (introduced in .NET 8).
     *
     * @tparam TSelf The implementing type (CRTP pattern).
     */
    template<typename TSelf>
    class IUtf8SpanFormattable {
    public:
        /** @brief Virtual destructor for safe polymorphic destruction. */
        virtual ~IUtf8SpanFormattable() = default;

        /**
         * @brief Tries to format the value of the current instance as UTF-8 into
         * the provided span of bytes.
         *
         * @param utf8Destination  Destination span to write UTF-8 bytes into.
         * @param bytesWritten     Receives the number of bytes written on success.
         * @param format           Optional format string (may be empty).
         * @return true if formatting succeeded and the output fits; false otherwise.
         */
        virtual bool TryFormatUtf8(Span<uint8_t> utf8Destination, int& bytesWritten,
                                    const std::string& format) const = 0;
    };

} // namespace System
