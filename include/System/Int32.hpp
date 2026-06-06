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

class Int32 {
public:
    static constexpr SharpRuntime::intcs MaxValue = std::numeric_limits<int32_t>::max();
    static constexpr SharpRuntime::intcs MinValue = std::numeric_limits<int32_t>::min();

    static SharpRuntime::intcs Parse(const std::string& s) {
        try { return static_cast<int32_t>(std::stoi(s)); }
        catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    static bool TryParse(const std::string& s, SharpRuntime::intcs& result) {
        try { result = static_cast<int32_t>(std::stoi(s)); return true; }
        catch (...) { result = 0; return false; }
    }

    static std::string ToString(SharpRuntime::intcs value) { return std::to_string(value); }
};

} // namespace System
