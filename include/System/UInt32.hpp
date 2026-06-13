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

/// Static helper methods that mirror System.UInt32 in .NET.
class UInt32 {
public:
    static constexpr SharpRuntime::uintcs MaxValue = std::numeric_limits<uint32_t>::max(); ///< Maximum value of a 32-bit unsigned integer (4 294 967 295).
    static constexpr SharpRuntime::uintcs MinValue = 0;                                    ///< Minimum value of a 32-bit unsigned integer (0).

    /// @brief Converts the string representation of a number to its 32-bit unsigned integer equivalent.
    /// @param s String to parse.
    /// @return Parsed uint32 value.
    /// @throws std::invalid_argument if the string is not valid or the value overflows.
    static SharpRuntime::uintcs Parse(const std::string& s) {
        try {
            unsigned long v = std::stoul(s);
            if (v > std::numeric_limits<uint32_t>::max())
                throw std::out_of_range("overflow");
            return static_cast<uint32_t>(v);
        }
        catch (const std::invalid_argument&) { throw std::invalid_argument("Input string was not in a correct format."); }
        catch (const std::out_of_range&)     { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    /// @brief Tries to convert a string to a 32-bit unsigned integer without throwing.
    /// @param s String to parse.
    /// @param result Receives the parsed value on success, or 0 on failure.
    /// @return True if parsing succeeded; false otherwise.
    static bool TryParse(const std::string& s, SharpRuntime::uintcs& result) {
        try { result = Parse(s); return true; }
        catch (...) { result = 0; return false; }
    }

    /// Converts the 32-bit unsigned integer @p value to its string representation.
    static std::string ToString(SharpRuntime::uintcs value) { return std::to_string(value); }
};

} // namespace System
