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

class UInt64 {
public:
    static constexpr SharpRuntime::ulongcs MaxValue = std::numeric_limits<uint64_t>::max();
    static constexpr SharpRuntime::ulongcs MinValue = 0;

    static SharpRuntime::ulongcs Parse(const std::string& s) {
        try { return std::stoull(s); }
        catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    static bool TryParse(const std::string& s, SharpRuntime::ulongcs& result) {
        try { result = Parse(s); return true; }
        catch (...) { result = 0; return false; }
    }

    static std::string ToString(SharpRuntime::ulongcs value) { return std::to_string(value); }
};

} // namespace System
