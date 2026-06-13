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
    static std::string ToString(SharpRuntime::bytecs value) { return std::to_string(value); }
};

} // namespace System
