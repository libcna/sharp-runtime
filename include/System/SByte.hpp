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

class SByte {
public:
    static constexpr SharpRuntime::sbytecs MaxValue = std::numeric_limits<int8_t>::max();
    static constexpr SharpRuntime::sbytecs MinValue = std::numeric_limits<int8_t>::min();

    static SharpRuntime::sbytecs Parse(const std::string& s) {
        try {
            int v = std::stoi(s);
            if (v < MinValue || v > MaxValue) throw std::out_of_range("Value out of SByte range.");
            return static_cast<int8_t>(v);
        } catch (const std::out_of_range&) { throw; }
          catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    static bool TryParse(const std::string& s, SharpRuntime::sbytecs& result) {
        try { result = Parse(s); return true; }
        catch (...) { result = 0; return false; }
    }

    static std::string ToString(SharpRuntime::sbytecs value) { return std::to_string(value); }
};

} // namespace System
