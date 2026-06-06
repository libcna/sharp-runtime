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

class Byte {
public:
    static constexpr SharpRuntime::bytecs MaxValue = std::numeric_limits<uint8_t>::max();
    static constexpr SharpRuntime::bytecs MinValue = 0;

    static SharpRuntime::bytecs Parse(const std::string& s) {
        try {
            int v = std::stoi(s);
            if (v < 0 || v > MaxValue) throw std::out_of_range("Value out of Byte range.");
            return static_cast<uint8_t>(v);
        } catch (const std::out_of_range&) { throw; }
          catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    static bool TryParse(const std::string& s, SharpRuntime::bytecs& result) {
        try { result = Parse(s); return true; }
        catch (...) { result = 0; return false; }
    }

    static std::string ToString(SharpRuntime::bytecs value) { return std::to_string(value); }
};

} // namespace System
