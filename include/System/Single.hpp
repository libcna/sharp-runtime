// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace System {

class Single {
public:
    static constexpr float MaxValue          =  std::numeric_limits<float>::max();
    static constexpr float MinValue          = -std::numeric_limits<float>::max();
    static constexpr float Epsilon           =  std::numeric_limits<float>::min();
    static constexpr float NaN               =  std::numeric_limits<float>::quiet_NaN();
    static constexpr float PositiveInfinity  =  std::numeric_limits<float>::infinity();
    static constexpr float NegativeInfinity  = -std::numeric_limits<float>::infinity();
    static constexpr float NegativeZero      = -0.0f;

    static bool IsNaN(float f)               { return std::isnan(f); }
    static bool IsInfinity(float f)          { return std::isinf(f); }
    static bool IsPositiveInfinity(float f)  { return std::isinf(f) && f > 0.0f; }
    static bool IsNegativeInfinity(float f)  { return std::isinf(f) && f < 0.0f; }
    static bool IsFinite(float f)            { return std::isfinite(f); }
    static bool IsNormal(float f)            { return std::isnormal(f); }
    static bool IsSubnormal(float f)         { return std::fpclassify(f) == FP_SUBNORMAL; }

    static float Parse(const std::string& s) {
        try { return std::stof(s); }
        catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    static bool TryParse(const std::string& s, float& result) {
        try { result = std::stof(s); return true; }
        catch (...) { result = 0.0f; return false; }
    }

    static std::string ToString(float value) { return std::to_string(value); }
};

} // namespace System
