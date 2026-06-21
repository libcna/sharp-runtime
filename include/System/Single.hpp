// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace System {

/**
 * @brief Provides constants and static methods for working with single-precision
 * floating-point values.
 *
 * C++ counterpart of .NET System.Single.
 */
class Single {
public:
    static constexpr float MaxValue          =  std::numeric_limits<float>::max();          ///< Largest finite positive float (~3.4e38).
    static constexpr float MinValue          = -std::numeric_limits<float>::max();          ///< Largest finite negative float (~-3.4e38).
    static constexpr float Epsilon           =  std::numeric_limits<float>::min();          ///< Smallest positive normalised float (~1.175e-38).
    static constexpr float NaN               =  std::numeric_limits<float>::quiet_NaN();    ///< Not-a-Number (quiet).
    static constexpr float PositiveInfinity  =  std::numeric_limits<float>::infinity();     ///< Positive infinity.
    static constexpr float NegativeInfinity  = -std::numeric_limits<float>::infinity();     ///< Negative infinity.
    static constexpr float NegativeZero      = -0.0f;                                       ///< Negative zero.

    /// Returns true if @p f is Not-a-Number (NaN).
    static bool IsNaN(float f)               { return std::isnan(f); }
    /// Returns true if @p f is positive or negative infinity.
    static bool IsInfinity(float f)          { return std::isinf(f); }
    /// Returns true if @p f is positive infinity.
    static bool IsPositiveInfinity(float f)  { return std::isinf(f) && f > 0.0f; }
    /// Returns true if @p f is negative infinity.
    static bool IsNegativeInfinity(float f)  { return std::isinf(f) && f < 0.0f; }
    /// Returns true if @p f is a finite number (not infinity or NaN).
    static bool IsFinite(float f)            { return std::isfinite(f); }
    /// Returns true if @p f is a normalised (not denormalised) finite number.
    static bool IsNormal(float f)            { return std::isnormal(f); }
    /// Returns true if @p f is a subnormal (denormalised) number.
    static bool IsSubnormal(float f)         { return std::fpclassify(f) == FP_SUBNORMAL; }

    /// @brief Converts the string representation of a number to its float equivalent.
    /// @param s String to parse ("NaN", "Infinity", "-Infinity", or a decimal number).
    /// @return Parsed float value.
    /// @throws std::invalid_argument if the string is not a valid floating-point literal.
    static float Parse(const std::string& s) {
        if (s == "NaN")       return std::numeric_limits<float>::quiet_NaN();
        if (s == "Infinity")  return  std::numeric_limits<float>::infinity();
        if (s == "-Infinity") return -std::numeric_limits<float>::infinity();
        float result{};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
        if (ec != std::errc{} || ptr != s.data() + s.size())
            throw std::invalid_argument("Input string was not in a correct format.");
        return result;
    }

    /// @brief Tries to convert a string to a float without throwing.
    /// @param s String to parse.
    /// @param result Receives the parsed value on success, or 0.0f on failure.
    /// @return True if parsing succeeded; false otherwise.
    static bool TryParse(const std::string& s, float& result) {
        if (s == "NaN")       { result = std::numeric_limits<float>::quiet_NaN(); return true; }
        if (s == "Infinity")  { result =  std::numeric_limits<float>::infinity(); return true; }
        if (s == "-Infinity") { result = -std::numeric_limits<float>::infinity(); return true; }
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
        if (ec != std::errc{} || ptr != s.data() + s.size()) { result = 0.0f; return false; }
        return true;
    }

    /// @brief Converts the float @p value to its string representation.
    /// @return "NaN", "Infinity", "-Infinity", or the shortest round-trip decimal string.
    static std::string ToString(float value) {
        if (std::isnan(value)) return "NaN";
        if (std::isinf(value)) return value > 0 ? "Infinity" : "-Infinity";
        std::array<char, 64> buf;
        auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
        if (ec == std::errc{}) return std::string(buf.data(), ptr);
        return std::to_string(value);
    }

    /// @brief Converts @p value to a string using format specifier ("F2", "E3", "G", "R").
    static std::string ToString(float value, const std::string& format) {
        if (format.empty()) return ToString(value);
        if (std::isnan(value)) return "NaN";
        if (std::isinf(value)) return value > 0 ? "Infinity" : "-Infinity";
        char type = format[0];
        int prec = format.size() > 1 ? std::stoi(format.substr(1)) : -1;
        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        if (type == 'F' || type == 'f') {
            oss << std::fixed << std::setprecision(prec >= 0 ? prec : 2) << value;
            return oss.str();
        }
        if (type == 'E' || type == 'e') {
            if (type == 'E') oss << std::uppercase;
            oss << std::scientific << std::setprecision(prec >= 0 ? prec : 6) << value;
            return oss.str();
        }
        if (type == 'G' || type == 'g') {
            if (prec > 0) oss << std::setprecision(prec);
            oss << value;
            return oss.str();
        }
        if (type == 'R' || type == 'r') return ToString(value);
        return ToString(value);
    }
};

} // namespace System
