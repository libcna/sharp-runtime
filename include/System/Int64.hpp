// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

/// Static helper methods that mirror System.Int64 in .NET.
class Int64 {
public:
    static constexpr SharpRuntime::longcs MaxValue = std::numeric_limits<int64_t>::max(); ///< Maximum value of a 64-bit signed integer.
    static constexpr SharpRuntime::longcs MinValue = std::numeric_limits<int64_t>::min(); ///< Minimum value of a 64-bit signed integer.

    /// @brief Converts the string representation of a number to its 64-bit signed integer equivalent.
    /// @param s String to parse.
    /// @return Parsed int64 value.
    /// @throws std::invalid_argument if the string is not a valid integer.
    static SharpRuntime::longcs Parse(const std::string& s) {
        try { return static_cast<int64_t>(std::stoll(s)); }
        catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    /// @brief Tries to convert a string to a 64-bit signed integer without throwing.
    /// @param s String to parse.
    /// @param result Receives the parsed value on success, or 0 on failure.
    /// @return True if parsing succeeded; false otherwise.
    static bool TryParse(const std::string& s, SharpRuntime::longcs& result) {
        try { result = static_cast<int64_t>(std::stoll(s)); return true; }
        catch (...) { result = 0; return false; }
    }

    /// Converts the 64-bit signed integer @p value to its string representation.
    static std::string ToString(SharpRuntime::longcs value) { return std::to_string(value); }

    /// @brief Converts @p value to a string using format specifier ("X","D","G","B").
    static std::string ToString(SharpRuntime::longcs value, const std::string& format) {
        if (format.empty()) return std::to_string(value);
        char type = format[0];
        int width = format.size() > 1 ? std::stoi(format.substr(1)) : 0;
        std::ostringstream oss;
        if (type == 'X') {
            oss << std::uppercase << std::hex;
            if (width > 0) oss << std::setfill('0') << std::setw(width);
            oss << static_cast<uint64_t>(value);
            return oss.str();
        }
        if (type == 'x') {
            oss << std::hex;
            if (width > 0) oss << std::setfill('0') << std::setw(width);
            oss << static_cast<uint64_t>(value);
            return oss.str();
        }
        if (type == 'D' || type == 'd') {
            if (width > 0) {
                std::string s = std::to_string(value);
                bool neg = value < 0;
                if (neg) s = s.substr(1);
                while (static_cast<int>(s.size()) < width) s = "0" + s;
                return neg ? "-" + s : s;
            }
            return std::to_string(value);
        }
        if (type == 'G' || type == 'g') return std::to_string(value);
        return std::to_string(value);
    }
};

} // namespace System

