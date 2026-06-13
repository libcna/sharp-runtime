// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <charconv>
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

    /// Parses a string to a double; throws on failure. Locale-independent (always uses '.').
    static double Parse(const std::string& s) {
        if (s == "NaN")       return std::numeric_limits<double>::quiet_NaN();
        if (s == "Infinity")  return  std::numeric_limits<double>::infinity();
        if (s == "-Infinity") return -std::numeric_limits<double>::infinity();
        double result{};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
        if (ec != std::errc{} || ptr != s.data() + s.size())
            throw std::invalid_argument("Input string was not in a correct format.");
        return result;
    }

    /// Attempts to parse a string to a double; returns false on failure. Locale-independent.
    static bool TryParse(const std::string& s, double& result) {
        if (s == "NaN")       { result = std::numeric_limits<double>::quiet_NaN(); return true; }
        if (s == "Infinity")  { result =  std::numeric_limits<double>::infinity(); return true; }
        if (s == "-Infinity") { result = -std::numeric_limits<double>::infinity(); return true; }
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
        if (ec != std::errc{} || ptr != s.data() + s.size()) { result = 0.0; return false; }
        return true;
    }

    /// Converts a double to its string representation. Locale-independent (always uses '.').
    static std::string ToString(double value) {
        if (std::isnan(value)) return "NaN";
        if (std::isinf(value)) return value > 0 ? "Infinity" : "-Infinity";
        std::array<char, 64> buf;
        auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
        if (ec == std::errc{}) return std::string(buf.data(), ptr);
        return std::to_string(value);
    }
};

} // namespace System
