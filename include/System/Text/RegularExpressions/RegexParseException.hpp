// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/ArgumentException.hpp"
#include "System/Text/RegularExpressions/RegexParseError.hpp"

namespace System::Text::RegularExpressions {

    /**
     * @brief Thrown when a regular expression parsing error occurs.
     *
     * C++ counterpart of .NET System.Text.RegularExpressions.RegexParseException.
     */
    class RegexParseException : public System::ArgumentException {
        RegexParseError error_;

    public:
        RegexParseException(RegexParseError error, const std::string& message) : ArgumentException(message), error_(error) {}

        /** @return The specific reason parsing failed. */
        [[nodiscard]] RegexParseError getErrorProperty() const { return error_; }
    };

} // namespace System::Text::RegularExpressions
