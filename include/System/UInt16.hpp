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

class UInt16 {
public:
    static constexpr SharpRuntime::ushortcs MaxValue = std::numeric_limits<uint16_t>::max();
    static constexpr SharpRuntime::ushortcs MinValue = 0;

    static SharpRuntime::ushortcs Parse(const std::string& s) {
        try {
            unsigned long v = std::stoul(s);
            if (v > MaxValue) throw std::out_of_range("Value out of UInt16 range.");
            return static_cast<uint16_t>(v);
        } catch (const std::out_of_range&) { throw; }
          catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    static bool TryParse(const std::string& s, SharpRuntime::ushortcs& result) {
        try { result = Parse(s); return true; }
        catch (...) { result = 0; return false; }
    }

    static std::string ToString(SharpRuntime::ushortcs value) { return std::to_string(value); }
};

} // namespace System
