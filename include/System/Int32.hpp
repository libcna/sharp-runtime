// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

/// Static helper methods that mirror System.Int32 in .NET.
class Int32 {
public:
    static constexpr SharpRuntime::intcs MaxValue = std::numeric_limits<int32_t>::max(); ///< Maximum value of a 32-bit signed integer (2 147 483 647).
    static constexpr SharpRuntime::intcs MinValue = std::numeric_limits<int32_t>::min(); ///< Minimum value of a 32-bit signed integer (-2 147 483 648).

    /// @brief Converts the string representation of a number to its 32-bit signed integer equivalent.
    /// @param s String to parse.
    /// @return Parsed int32 value.
    /// @throws std::invalid_argument if the string is not a valid integer.
    static SharpRuntime::intcs Parse(const std::string& s) {
        try { return static_cast<int32_t>(std::stoi(s)); }
        catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    /// @brief Tries to convert a string to a 32-bit signed integer without throwing.
    /// @param s String to parse.
    /// @param result Receives the parsed value on success, or 0 on failure.
    /// @return True if parsing succeeded; false otherwise.
    static bool TryParse(const std::string& s, SharpRuntime::intcs& result) {
        try { result = static_cast<int32_t>(std::stoi(s)); return true; }
        catch (...) { result = 0; return false; }
    }

    /// Converts the 32-bit signed integer @p value to its string representation.
    static std::string ToString(SharpRuntime::intcs value) { return std::to_string(value); }
};

} // namespace System
