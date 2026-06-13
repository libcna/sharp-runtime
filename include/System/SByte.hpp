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

/// Static helper methods that mirror System.SByte in .NET.
class SByte {
public:
    static constexpr SharpRuntime::sbytecs MaxValue = std::numeric_limits<int8_t>::max(); ///< Maximum value of an 8-bit signed integer (127).
    static constexpr SharpRuntime::sbytecs MinValue = std::numeric_limits<int8_t>::min(); ///< Minimum value of an 8-bit signed integer (-128).

    /// @brief Converts the string representation of a number to its 8-bit signed integer equivalent.
    /// @param s String to parse.
    /// @return Parsed int8 value.
    /// @throws std::out_of_range if the value exceeds SByte range.
    /// @throws std::invalid_argument if the string is not a valid integer.
    static SharpRuntime::sbytecs Parse(const std::string& s) {
        try {
            int v = std::stoi(s);
            if (v < MinValue || v > MaxValue) throw std::out_of_range("Value out of SByte range.");
            return static_cast<int8_t>(v);
        } catch (const std::out_of_range&) { throw; }
          catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    /// @brief Tries to convert a string to an 8-bit signed integer without throwing.
    /// @param s String to parse.
    /// @param result Receives the parsed value on success, or 0 on failure.
    /// @return True if parsing succeeded; false otherwise.
    static bool TryParse(const std::string& s, SharpRuntime::sbytecs& result) {
        try { result = Parse(s); return true; }
        catch (...) { result = 0; return false; }
    }

    /// Converts the 8-bit signed integer @p value to its string representation.
    static std::string ToString(SharpRuntime::sbytecs value) { return std::to_string(value); }
};

} // namespace System
