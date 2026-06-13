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

/// Represents the System.Byte type and its static parse/convert helpers.
class Byte {
public:
    /// The maximum value of a Byte (255).
    static constexpr SharpRuntime::bytecs MaxValue = std::numeric_limits<uint8_t>::max();
    /// The minimum value of a Byte (0).
    static constexpr SharpRuntime::bytecs MinValue = 0;

    /// Parses a string to a Byte value; throws on failure.
    static SharpRuntime::bytecs Parse(const std::string& s) {
        try {
            int v = std::stoi(s);
            if (v < 0 || v > MaxValue) throw std::out_of_range("Value out of Byte range.");
            return static_cast<uint8_t>(v);
        } catch (const std::out_of_range&) { throw; }
          catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    /// Attempts to parse a string to a Byte value; returns false on failure.
    static bool TryParse(const std::string& s, SharpRuntime::bytecs& result) {
        try { result = Parse(s); return true; }
        catch (...) { result = 0; return false; }
    }

    /// Converts a Byte value to its string representation.
    static std::string ToString(SharpRuntime::bytecs value) { return std::to_string(static_cast<unsigned>(value)); }

    /// Converts @p value to a string using format specifier ("X", "X2", "D", "D3", "G", "B").
    static std::string ToString(SharpRuntime::bytecs value, const std::string& format) {
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
        if (type == 'B' || type == 'b') {
            std::string s;
            for (int i = 7; i >= 0; --i) s += ((uv >> i) & 1) ? '1' : '0';
            s.erase(0, s.find_first_not_of('0'));
            if (s.empty()) s = "0";
            while (static_cast<int>(s.size()) < width) s = "0" + s;
            return s;
        }
        return ToString(value);
    }
};

} // namespace System
