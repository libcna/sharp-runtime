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

class UInt32 {
public:
    static constexpr SharpRuntime::uintcs MaxValue = std::numeric_limits<uint32_t>::max();
    static constexpr SharpRuntime::uintcs MinValue = 0;

    static SharpRuntime::uintcs Parse(const std::string& s) {
        try { return static_cast<uint32_t>(std::stoul(s)); }
        catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    static bool TryParse(const std::string& s, SharpRuntime::uintcs& result) {
        try { result = Parse(s); return true; }
        catch (...) { result = 0; return false; }
    }

    static std::string ToString(SharpRuntime::uintcs value) { return std::to_string(value); }
};

} // namespace System
