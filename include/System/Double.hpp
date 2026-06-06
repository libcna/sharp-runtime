// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace System {

class Double {
public:
    static constexpr double MaxValue         =  std::numeric_limits<double>::max();
    static constexpr double MinValue         = -std::numeric_limits<double>::max();
    static constexpr double Epsilon          =  std::numeric_limits<double>::min();
    static constexpr double NaN              =  std::numeric_limits<double>::quiet_NaN();
    static constexpr double PositiveInfinity =  std::numeric_limits<double>::infinity();
    static constexpr double NegativeInfinity = -std::numeric_limits<double>::infinity();
    static constexpr double NegativeZero     = -0.0;

    static bool IsNaN(double d)               { return std::isnan(d); }
    static bool IsInfinity(double d)          { return std::isinf(d); }
    static bool IsPositiveInfinity(double d)  { return std::isinf(d) && d > 0.0; }
    static bool IsNegativeInfinity(double d)  { return std::isinf(d) && d < 0.0; }
    static bool IsFinite(double d)            { return std::isfinite(d); }
    static bool IsNormal(double d)            { return std::isnormal(d); }
    static bool IsSubnormal(double d)         { return std::fpclassify(d) == FP_SUBNORMAL; }

    static double Parse(const std::string& s) {
        try { return std::stod(s); }
        catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    static bool TryParse(const std::string& s, double& result) {
        try { result = std::stod(s); return true; }
        catch (...) { result = 0.0; return false; }
    }

    static std::string ToString(double value) { return std::to_string(value); }
};

} // namespace System
