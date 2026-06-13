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

/// Static helper methods that mirror System.UInt64 in .NET.
class UInt64 {
public:
    static constexpr SharpRuntime::ulongcs MaxValue = std::numeric_limits<uint64_t>::max(); ///< Maximum value of a 64-bit unsigned integer.
    static constexpr SharpRuntime::ulongcs MinValue = 0;                                    ///< Minimum value of a 64-bit unsigned integer (0).

    /// @brief Converts the string representation of a number to its 64-bit unsigned integer equivalent.
    /// @param s String to parse.
    /// @return Parsed uint64 value.
    /// @throws std::invalid_argument if the string is not a valid integer.
    static SharpRuntime::ulongcs Parse(const std::string& s) {
        try { return std::stoull(s); }
        catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    /// @brief Tries to convert a string to a 64-bit unsigned integer without throwing.
    /// @param s String to parse.
    /// @param result Receives the parsed value on success, or 0 on failure.
    /// @return True if parsing succeeded; false otherwise.
    static bool TryParse(const std::string& s, SharpRuntime::ulongcs& result) {
        try { result = Parse(s); return true; }
        catch (...) { result = 0; return false; }
    }

    /// Converts the 64-bit unsigned integer @p value to its string representation.
    static std::string ToString(SharpRuntime::ulongcs value) { return std::to_string(value); }
};

} // namespace System
