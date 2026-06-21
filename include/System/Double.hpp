// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace System {

/**
 * @brief Represents a double-precision floating-point number.
 *
 * C++ counterpart of .NET System.Double.
 * Provides static constants and helpers mirroring the .NET Double struct.
 */
class Double {
public:
    // -----------------------------------------------------------------------
    // Constants
    // -----------------------------------------------------------------------

    /** @brief Represents the smallest positive double value greater than zero. */
    static constexpr double Epsilon          =  std::numeric_limits<double>::min();

    /** @brief Represents the largest possible value of a double. */
    static constexpr double MaxValue         =  std::numeric_limits<double>::max();

    /** @brief Represents the smallest possible value of a double (most negative). */
    static constexpr double MinValue         = -std::numeric_limits<double>::max();

    /** @brief Represents a value that is not a number (NaN). */
    static constexpr double NaN              =  std::numeric_limits<double>::quiet_NaN();

    /** @brief Represents negative infinity. */
    static constexpr double NegativeInfinity = -std::numeric_limits<double>::infinity();

    /** @brief Represents positive infinity. */
    static constexpr double PositiveInfinity =  std::numeric_limits<double>::infinity();

    /** @brief Represents the number negative zero (-0). */
    static constexpr double NegativeZero     = -0.0;

    /**
     * @brief Represents the natural logarithmic base, specified by the constant, e.
     *
     * Euler's number is approximately 2.7182818284590452354.
     */
    static constexpr double E   = 2.718281828459045235360287471352662;

    /**
     * @brief Represents the ratio of the circumference of a circle to its diameter, PI.
     *
     * Pi is approximately 3.1415926535897932385.
     */
    static constexpr double Pi  = 3.141592653589793238462643383279502;

    /**
     * @brief Represents the number of radians in one turn, Tau.
     *
     * Tau is approximately 6.2831853071795864769.
     */
    static constexpr double Tau = 6.283185307179586476925286766559005;

    // -----------------------------------------------------------------------
    // Classification predicates
    // -----------------------------------------------------------------------

    /** @brief Determines whether the specified value is Not a Number (NaN). */
    static bool IsNaN(double d)               { return std::isnan(d); }

    /** @brief Determines whether the specified value is positive or negative infinity. */
    static bool IsInfinity(double d)          { return std::isinf(d); }

    /** @brief Determines whether the specified value is positive infinity. */
    static bool IsPositiveInfinity(double d)  { return d == PositiveInfinity; }

    /** @brief Determines whether the specified value is negative infinity. */
    static bool IsNegativeInfinity(double d)  { return d == NegativeInfinity; }

    /** @brief Determines whether the specified value is finite (not NaN or infinity). */
    static bool IsFinite(double d)            { return std::isfinite(d); }

    /** @brief Determines whether the specified value is a normal floating-point number. */
    static bool IsNormal(double d)            { return std::isnormal(d); }

    /** @brief Determines whether the specified value is subnormal (finite, but not zero or normal). */
    static bool IsSubnormal(double d)         { return std::fpclassify(d) == FP_SUBNORMAL; }

    /**
     * @brief Determines whether the specified value is negative.
     *
     * Returns true for negative zero and negative NaN as well.
     */
    static bool IsNegative(double d)
    {
        return std::signbit(d);
    }

    /**
     * @brief Determines whether the specified value is positive.
     *
     * Returns true for positive zero; false for negative NaN.
     */
    static bool IsPositive(double d)
    {
        return !std::signbit(d);
    }

    /**
     * @brief Determines whether the specified value is an integer (finite and has no fractional part).
     */
    static bool IsInteger(double value)
    {
        return IsFinite(value) && (value == std::trunc(value));
    }

    /**
     * @brief Determines whether the specified value is an even integer.
     */
    static bool IsEvenInteger(double value)
    {
        return IsInteger(value) && (std::fabs(std::fmod(value, 2.0)) == 0.0);
    }

    /**
     * @brief Determines whether the specified value is an odd integer.
     */
    static bool IsOddInteger(double value)
    {
        return IsInteger(value) && (std::fabs(std::fmod(value, 2.0)) == 1.0);
    }

    /**
     * @brief Determines whether the specified value is a real number (finite or infinite, but not NaN).
     */
    static bool IsRealNumber(double value)
    {
        return !IsNaN(value);
    }

    /**
     * @brief Determines whether the specified value is a power of two.
     *
     * Returns false for zero, negative values, NaN, and infinity.
     */
    static bool IsPow2(double value)
    {
        if (value <= 0.0 || !IsFinite(value)) return false;
        uint64_t bits;
        static_assert(sizeof(double) == sizeof(uint64_t));
        __builtin_memcpy(&bits, &value, sizeof(bits));
        // Trailing significand must be zero (only the implicit leading 1 bit)
        constexpr uint64_t TrailingMask = 0x000F'FFFF'FFFF'FFFFull;
        return (bits & TrailingMask) == 0;
    }

    // -----------------------------------------------------------------------
    // Math helpers
    // -----------------------------------------------------------------------

    /** @brief Returns the absolute value of a double-precision floating-point number. */
    static double Abs(double value)                              { return std::fabs(value); }

    /**
     * @brief Returns @p value clamped to the inclusive range [@p min, @p max].
     */
    static double Clamp(double value, double min, double max)
    {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    /**
     * @brief Returns a value with the magnitude of @p value and the sign of @p sign.
     */
    static double CopySign(double value, double sign)            { return std::copysign(value, sign); }

    /** @brief Returns the larger of two double-precision floating-point numbers. */
    static double Max(double x, double y)                        { return std::fmax(x, y); }

    /** @brief Returns the smaller of two double-precision floating-point numbers. */
    static double Min(double x, double y)                        { return std::fmin(x, y); }

    /**
     * @brief Returns an integer that indicates the sign of a double-precision floating-point number.
     *
     * Returns -1, 0, or 1. Throws if @p value is NaN.
     */
    static int Sign(double value)
    {
        if (IsNaN(value)) throw std::invalid_argument("Value is not a number (NaN).");
        if (value < 0.0) return -1;
        if (value > 0.0) return  1;
        return 0;
    }

    // -----------------------------------------------------------------------
    // Parse / TryParse
    // -----------------------------------------------------------------------

    /**
     * @brief Converts the string representation of a number to its double equivalent.
     *
     * Locale-independent (always uses '.'). Throws std::invalid_argument on failure.
     */
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

    /**
     * @brief Tries to convert a string to a double. Returns false on failure.
     *
     * Locale-independent. Sets @p result to 0.0 on failure.
     */
    static bool TryParse(const std::string& s, double& result) {
        if (s == "NaN")       { result = std::numeric_limits<double>::quiet_NaN(); return true; }
        if (s == "Infinity")  { result =  std::numeric_limits<double>::infinity(); return true; }
        if (s == "-Infinity") { result = -std::numeric_limits<double>::infinity(); return true; }
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
        if (ec != std::errc{} || ptr != s.data() + s.size()) { result = 0.0; return false; }
        return true;
    }

    // -----------------------------------------------------------------------
    // ToString
    // -----------------------------------------------------------------------

    /**
     * @brief Converts a double to its string representation.
     *
     * Locale-independent (always uses '.'). Produces the round-trip ("R") format.
     */
    static std::string ToString(double value) {
        if (std::isnan(value)) return "NaN";
        if (std::isinf(value)) return value > 0 ? "Infinity" : "-Infinity";
        std::array<char, 64> buf;
        auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
        if (ec == std::errc{}) return std::string(buf.data(), ptr);
        return std::to_string(value);
    }

    /**
     * @brief Converts @p value to a string using a format specifier.
     *
     * Supports format specifiers: F/f (fixed), E/e (scientific), G/g (general),
     * R/r (round-trip), N/n (numeric). Digit count follows the specifier (e.g. "F2").
     */
    static std::string ToString(double value, const std::string& format) {
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
        if (type == 'N' || type == 'n') {
            oss << std::fixed << std::setprecision(prec >= 0 ? prec : 2) << value;
            return oss.str();
        }
        return ToString(value);
    }
};

} // namespace System
