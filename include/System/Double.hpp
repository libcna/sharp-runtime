// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace System {

/// Represents the System.Double type and its static helpers.
class Double {
public:
    /// The maximum finite value of a double.
    static constexpr double MaxValue         =  std::numeric_limits<double>::max();
    /// The minimum finite value of a double (most negative).
    static constexpr double MinValue         = -std::numeric_limits<double>::max();
    /// The smallest positive double value greater than zero.
    static constexpr double Epsilon          =  std::numeric_limits<double>::min();
    /// Represents a value that is not a number (NaN).
    static constexpr double NaN              =  std::numeric_limits<double>::quiet_NaN();
    /// Represents positive infinity.
    static constexpr double PositiveInfinity =  std::numeric_limits<double>::infinity();
    /// Represents negative infinity.
    static constexpr double NegativeInfinity = -std::numeric_limits<double>::infinity();
    /// Represents negative zero.
    static constexpr double NegativeZero     = -0.0;

    /// Returns true if the specified value is Not a Number (NaN).
    static bool IsNaN(double d)               { return std::isnan(d); }
    /// Returns true if the specified value is positive or negative infinity.
    static bool IsInfinity(double d)          { return std::isinf(d); }
    /// Returns true if the specified value is positive infinity.
    static bool IsPositiveInfinity(double d)  { return std::isinf(d) && d > 0.0; }
    /// Returns true if the specified value is negative infinity.
    static bool IsNegativeInfinity(double d)  { return std::isinf(d) && d < 0.0; }
    /// Returns true if the specified value is finite (not NaN or infinity).
    static bool IsFinite(double d)            { return std::isfinite(d); }
    /// Returns true if the specified value is a normal floating-point number.
    static bool IsNormal(double d)            { return std::isnormal(d); }
    /// Returns true if the specified value is subnormal.
    static bool IsSubnormal(double d)         { return std::fpclassify(d) == FP_SUBNORMAL; }

    /// Parses a string to a double; throws on failure.
    static double Parse(const std::string& s) {
        try { return std::stod(s); }
        catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    /// Attempts to parse a string to a double; returns false on failure.
    static bool TryParse(const std::string& s, double& result) {
        try { result = std::stod(s); return true; }
        catch (...) { result = 0.0; return false; }
    }

    /// Converts a double to its string representation.
    static std::string ToString(double value) { return std::to_string(value); }
};

} // namespace System
