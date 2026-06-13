// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <string>

namespace System {

/// Represents the Boolean (true/false) value type and its string equivalents.
class Boolean {
public:
    /// The string representation of the Boolean true value.
    static constexpr const char* TrueString  = "True";
    /// The string representation of the Boolean false value.
    static constexpr const char* FalseString = "False";

    /// Parses a string as a Boolean value; throws if the string is not "True" or "False".
    static bool Parse(const std::string& s) {
        if (s == "True"  || s == "true")  return true;
        if (s == "False" || s == "false") return false;
        throw std::invalid_argument("String must be 'True' or 'False'.");
    }

    /// Attempts to parse a string as a Boolean value; returns false on failure.
    static bool TryParse(const std::string& s, bool& result) {
        if (s == "True"  || s == "true")  { result = true;  return true; }
        if (s == "False" || s == "false") { result = false; return true; }
        result = false; return false;
    }

    /// Returns the string "True" or "False" for the specified Boolean value.
    static std::string ToString(bool value) { return value ? "True" : "False"; }
};

} // namespace System
