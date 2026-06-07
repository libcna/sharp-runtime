// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <limits>
#include <stdexcept>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

class Int64 {
public:
    static constexpr SharpRuntime::longcs MaxValue = std::numeric_limits<int64_t>::max();
    static constexpr SharpRuntime::longcs MinValue = std::numeric_limits<int64_t>::min();

    static SharpRuntime::longcs Parse(const std::string& s) {
        try { return static_cast<int64_t>(std::stoll(s)); }
        catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    static bool TryParse(const std::string& s, SharpRuntime::longcs& result) {
        try { result = static_cast<int64_t>(std::stoll(s)); return true; }
        catch (...) { result = 0; return false; }
    }

    static std::string ToString(SharpRuntime::longcs value) { return std::to_string(value); }
};

} // namespace System

