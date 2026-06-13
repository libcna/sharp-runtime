// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

/// Static helper methods that mirror System.UInt16 in .NET.
class UInt16 {
public:
    static constexpr SharpRuntime::ushortcs MaxValue = std::numeric_limits<uint16_t>::max(); ///< Maximum value of a 16-bit unsigned integer (65535).
    static constexpr SharpRuntime::ushortcs MinValue = 0;                                    ///< Minimum value of a 16-bit unsigned integer (0).

    /// @brief Converts the string representation of a number to its 16-bit unsigned integer equivalent.
    /// @param s String to parse.
    /// @return Parsed uint16 value.
    /// @throws std::out_of_range if the value exceeds UInt16 range.
    /// @throws std::invalid_argument if the string is not a valid integer.
    static SharpRuntime::ushortcs Parse(const std::string& s) {
        try {
            unsigned long v = std::stoul(s);
            if (v > MaxValue) throw std::out_of_range("Value out of UInt16 range.");
            return static_cast<uint16_t>(v);
        } catch (const std::out_of_range&) { throw; }
          catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    /// @brief Tries to convert a string to a 16-bit unsigned integer without throwing.
    /// @param s String to parse.
    /// @param result Receives the parsed value on success, or 0 on failure.
    /// @return True if parsing succeeded; false otherwise.
    static bool TryParse(const std::string& s, SharpRuntime::ushortcs& result) {
        try { result = Parse(s); return true; }
        catch (...) { result = 0; return false; }
    }

    /// Converts the 16-bit unsigned integer @p value to its string representation.
    static std::string ToString(SharpRuntime::ushortcs value) { return std::to_string(value); }

    /// Converts @p value to a string using format specifier ("X", "X4", "D", "D5", "G").
    static std::string ToString(SharpRuntime::ushortcs value, const std::string& format) {
        if (format.empty()) return ToString(value);
        char type = format[0];
        int width = format.size() > 1 ? std::stoi(format.substr(1)) : 0;
        unsigned uv = static_cast<unsigned>(value);
        std::ostringstream oss;
        if (type == 'X') { oss << std::uppercase << std::hex << std::setfill('0') << std::setw(width) << uv; return oss.str(); }
        if (type == 'x') { oss << std::hex << std::setfill('0') << std::setw(width) << uv; return oss.str(); }
        if (type == 'D' || type == 'd') {
            std::string s = std::to_string(uv);
            while (static_cast<int>(s.size()) < width) s = "0" + s;
            return s;
        }
        if (type == 'G' || type == 'g') return ToString(value);
        return ToString(value);
    }
};

} // namespace System
