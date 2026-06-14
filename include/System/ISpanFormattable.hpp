// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/IFormattable.hpp"

namespace System {

    /**
     * @brief Provides functionality to format the string representation of an object into a span.
     *
     * C++ counterpart of .NET System.ISpanFormattable (.NET 7+).
     * Extends IFormattable. The .NET Span<char> destination is approximated here
     * as a char buffer with a maximum length; charsWritten receives the count written.
     */
    class ISpanFormattable : public IFormattable {
    public:
        virtual ~ISpanFormattable() = default;

        /**
         * @brief Tries to format the value of the current instance into the provided character buffer.
         * @param destination Pointer to the character buffer that receives the formatted result.
         * @param destLen     Maximum number of characters the buffer can hold.
         * @param charsWritten On return, the number of characters written to destination.
         * @param format      A string that defines the format of the output.
         * @return true if formatting succeeded; false if the buffer was too small.
         */
        virtual bool TryFormat(char* destination, std::size_t destLen,
                               std::size_t& charsWritten,
                               const std::string& format) const = 0;
    };

} // namespace System
