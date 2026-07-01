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

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;

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
     * @brief Returns the value with the larger magnitude; prefers @p x on tie.
     * C++ counterpart of .NET Double.MaxMagnitude(double,double).
     */
    [[nodiscard]] static double MaxMagnitude(double x, double y) noexcept
    {
        return std::abs(x) >= std::abs(y) ? x : y;
    }

    /**
     * @brief Returns the value with the smaller magnitude; prefers @p x on tie.
     * C++ counterpart of .NET Double.MinMagnitude(double,double).
     */
    [[nodiscard]] static double MinMagnitude(double x, double y) noexcept
    {
        return std::abs(x) <= std::abs(y) ? x : y;
    }

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
    // Rounding
    // -----------------------------------------------------------------------

    /** @brief Returns the smallest integral value greater than or equal to @p x. C++ counterpart of .NET Double.Ceiling(double). */
    [[nodiscard]] static double Ceiling(double x) noexcept { return std::ceil(x); }

    /** @brief Returns the largest integral value less than or equal to @p x. C++ counterpart of .NET Double.Floor(double). */
    [[nodiscard]] static double Floor(double x) noexcept { return std::floor(x); }

    /** @brief Returns the integral part (discards fractional part). C++ counterpart of .NET Double.Truncate(double). */
    [[nodiscard]] static double Truncate(double x) noexcept { return std::trunc(x); }

    /** @brief Rounds @p x to the nearest integer (ties to even). C++ counterpart of .NET Double.Round(double). */
    [[nodiscard]] static double Round(double x) noexcept { return std::nearbyint(x); }

    /** @brief Rounds @p x to @p digits decimal places (ties to even). C++ counterpart of .NET Double.Round(double,int). */
    [[nodiscard]] static double Round(double x, intcs digits) noexcept
    {
        double factor = std::pow(10.0, static_cast<double>(digits));
        return std::nearbyint(x * factor) / factor;
    }

    // -----------------------------------------------------------------------
    // Exponential / logarithmic
    // -----------------------------------------------------------------------

    /** @brief Returns e raised to the power @p x. C++ counterpart of .NET Double.Exp(double). */
    [[nodiscard]] static double Exp(double x) noexcept { return std::exp(x); }

    /** @brief Returns 2 raised to the power @p x. C++ counterpart of .NET Double.Exp2(double). */
    [[nodiscard]] static double Exp2(double x) noexcept { return std::exp2(x); }

    /** @brief Returns 10 raised to the power @p x. C++ counterpart of .NET Double.Exp10(double). */
    [[nodiscard]] static double Exp10(double x) noexcept { return std::pow(10.0, x); }

    /** @brief Returns the natural logarithm of @p x. C++ counterpart of .NET Double.Log(double). */
    [[nodiscard]] static double Log(double x) noexcept { return std::log(x); }

    /** @brief Returns the logarithm of @p x in base @p newBase. C++ counterpart of .NET Double.Log(double,double). */
    [[nodiscard]] static double Log(double x, double newBase) noexcept { return std::log(x) / std::log(newBase); }

    /** @brief Returns the base-2 logarithm of @p x. C++ counterpart of .NET Double.Log2(double). */
    [[nodiscard]] static double Log2(double x) noexcept { return std::log2(x); }

    /** @brief Returns the base-10 logarithm of @p x. C++ counterpart of .NET Double.Log10(double). */
    [[nodiscard]] static double Log10(double x) noexcept { return std::log10(x); }

    /** @brief Returns @p x raised to the power @p y. C++ counterpart of .NET Double.Pow(double,double). */
    [[nodiscard]] static double Pow(double x, double y) noexcept { return std::pow(x, y); }

    // -----------------------------------------------------------------------
    // Root / reciprocal
    // -----------------------------------------------------------------------

    /** @brief Returns the square root of @p x. C++ counterpart of .NET Double.Sqrt(double). */
    [[nodiscard]] static double Sqrt(double x) noexcept { return std::sqrt(x); }

    /** @brief Returns the cube root of @p x. C++ counterpart of .NET Double.Cbrt(double). */
    [[nodiscard]] static double Cbrt(double x) noexcept { return std::cbrt(x); }

    /** @brief Returns the n-th root of @p x. C++ counterpart of .NET Double.RootN(double,int). */
    [[nodiscard]] static double RootN(double x, intcs n) noexcept
    {
        return std::pow(x, 1.0 / static_cast<double>(n));
    }

    /** @brief Returns an estimate of 1/x. C++ counterpart of .NET Double.ReciprocalEstimate(double). */
    [[nodiscard]] static double ReciprocalEstimate(double x) noexcept { return 1.0 / x; }

    /** @brief Returns an estimate of 1/sqrt(x). C++ counterpart of .NET Double.ReciprocalSqrtEstimate(double). */
    [[nodiscard]] static double ReciprocalSqrtEstimate(double x) noexcept { return 1.0 / std::sqrt(x); }

    // -----------------------------------------------------------------------
    // Trigonometric
    // -----------------------------------------------------------------------

    /** @brief Returns the sine of @p x (in radians). C++ counterpart of .NET Double.Sin(double). */
    [[nodiscard]] static double Sin(double x) noexcept { return std::sin(x); }

    /** @brief Returns the cosine of @p x (in radians). C++ counterpart of .NET Double.Cos(double). */
    [[nodiscard]] static double Cos(double x) noexcept { return std::cos(x); }

    /** @brief Returns the tangent of @p x (in radians). C++ counterpart of .NET Double.Tan(double). */
    [[nodiscard]] static double Tan(double x) noexcept { return std::tan(x); }

    /** @brief Returns the arcsine of @p x. C++ counterpart of .NET Double.Asin(double). */
    [[nodiscard]] static double Asin(double x) noexcept { return std::asin(x); }

    /** @brief Returns the arccosine of @p x. C++ counterpart of .NET Double.Acos(double). */
    [[nodiscard]] static double Acos(double x) noexcept { return std::acos(x); }

    /** @brief Returns the arctangent of @p x. C++ counterpart of .NET Double.Atan(double). */
    [[nodiscard]] static double Atan(double x) noexcept { return std::atan(x); }

    /** @brief Returns the angle (in radians) whose tangent is @p y/@p x. C++ counterpart of .NET Double.Atan2(double,double). */
    [[nodiscard]] static double Atan2(double y, double x) noexcept { return std::atan2(y, x); }

    /** @brief Returns the sine of @p x * Pi. C++ counterpart of .NET Double.SinPi(double). */
    [[nodiscard]] static double SinPi(double x) noexcept { return std::sin(x * Pi); }

    /** @brief Returns the cosine of @p x * Pi. C++ counterpart of .NET Double.CosPi(double). */
    [[nodiscard]] static double CosPi(double x) noexcept { return std::cos(x * Pi); }

    /** @brief Returns the tangent of @p x * Pi. C++ counterpart of .NET Double.TanPi(double). */
    [[nodiscard]] static double TanPi(double x) noexcept { return std::tan(x * Pi); }

    /** @brief Returns the arccosine of @p x divided by Pi. C++ counterpart of .NET Double.AcosPi(double). */
    [[nodiscard]] static double AcosPi(double x) noexcept { return std::acos(x) / Pi; }

    /** @brief Returns the arcsine of @p x divided by Pi. C++ counterpart of .NET Double.AsinPi(double). */
    [[nodiscard]] static double AsinPi(double x) noexcept { return std::asin(x) / Pi; }

    /** @brief Returns the arctangent of @p x divided by Pi. C++ counterpart of .NET Double.AtanPi(double). */
    [[nodiscard]] static double AtanPi(double x) noexcept { return std::atan(x) / Pi; }

    /** @brief Returns the angle (in turns) whose tangent is @p y/@p x. C++ counterpart of .NET Double.Atan2Pi(double,double). */
    [[nodiscard]] static double Atan2Pi(double y, double x) noexcept { return std::atan2(y, x) / Pi; }

    /** @brief Pair returned by SinCos. */
    struct SinCosResult { double Sin; double Cos; };

    /**
     * @brief Computes sine and cosine of @p x simultaneously.
     * C++ counterpart of .NET Double.SinCos(double).
     */
    [[nodiscard]] static SinCosResult SinCos(double x) noexcept
    {
        return { std::sin(x), std::cos(x) };
    }

    /** @brief Pair returned by SinCosPi. */
    struct SinCosPiResult { double SinPi; double CosPi; };

    /**
     * @brief Computes sine and cosine of @p x * Pi simultaneously.
     * C++ counterpart of .NET Double.SinCosPi(double).
     */
    [[nodiscard]] static SinCosPiResult SinCosPi(double x) noexcept
    {
        return { std::sin(x * Pi), std::cos(x * Pi) };
    }

    // -----------------------------------------------------------------------
    // Hyperbolic
    // -----------------------------------------------------------------------

    /** @brief Returns the hyperbolic sine of @p x. C++ counterpart of .NET Double.Sinh(double). */
    [[nodiscard]] static double Sinh(double x) noexcept { return std::sinh(x); }

    /** @brief Returns the hyperbolic cosine of @p x. C++ counterpart of .NET Double.Cosh(double). */
    [[nodiscard]] static double Cosh(double x) noexcept { return std::cosh(x); }

    /** @brief Returns the hyperbolic tangent of @p x. C++ counterpart of .NET Double.Tanh(double). */
    [[nodiscard]] static double Tanh(double x) noexcept { return std::tanh(x); }

    /** @brief Returns the inverse hyperbolic sine of @p x. C++ counterpart of .NET Double.Asinh(double). */
    [[nodiscard]] static double Asinh(double x) noexcept { return std::asinh(x); }

    /** @brief Returns the inverse hyperbolic cosine of @p x. C++ counterpart of .NET Double.Acosh(double). */
    [[nodiscard]] static double Acosh(double x) noexcept { return std::acosh(x); }

    /** @brief Returns the inverse hyperbolic tangent of @p x. C++ counterpart of .NET Double.Atanh(double). */
    [[nodiscard]] static double Atanh(double x) noexcept { return std::atanh(x); }

    // -----------------------------------------------------------------------
    // IEEE 754 utilities
    // -----------------------------------------------------------------------

    /**
     * @brief Returns the hypotenuse of a right triangle with legs @p x and @p y.
     * C++ counterpart of .NET Double.Hypot(double,double).
     */
    [[nodiscard]] static double Hypot(double x, double y) noexcept { return std::hypot(x, y); }

    /**
     * @brief Returns (left * right) + addend computed with a single rounding.
     * C++ counterpart of .NET Double.FusedMultiplyAdd(double,double,double).
     */
    [[nodiscard]] static double FusedMultiplyAdd(double left, double right, double addend) noexcept
    {
        return std::fma(left, right, addend);
    }

    /**
     * @brief Returns the IEEE 754 remainder of @p left / @p right.
     * C++ counterpart of .NET Double.Ieee754Remainder(double,double).
     */
    [[nodiscard]] static double Ieee754Remainder(double left, double right) noexcept
    {
        return std::remainder(left, right);
    }

    /**
     * @brief Returns the next representable double value after @p x in the direction of negative infinity.
     * C++ counterpart of .NET Double.BitDecrement(double).
     */
    [[nodiscard]] static double BitDecrement(double x) noexcept
    {
        return std::nextafter(x, -std::numeric_limits<double>::infinity());
    }

    /**
     * @brief Returns the next representable double value after @p x in the direction of positive infinity.
     * C++ counterpart of .NET Double.BitIncrement(double).
     */
    [[nodiscard]] static double BitIncrement(double x) noexcept
    {
        return std::nextafter(x, std::numeric_limits<double>::infinity());
    }

    /**
     * @brief Returns the base-2 integer exponent of @p x.
     * C++ counterpart of .NET Double.ILogB(double).
     */
    [[nodiscard]] static intcs ILogB(double x) noexcept { return static_cast<intcs>(std::ilogb(x)); }

    /**
     * @brief Returns x * 2^n. C++ counterpart of .NET Double.ScaleB(double,int).
     */
    [[nodiscard]] static double ScaleB(double x, intcs n) noexcept { return std::ldexp(x, n); }

    // -----------------------------------------------------------------------
    // Angle conversion
    // -----------------------------------------------------------------------

    /** @brief Converts @p degrees to radians. C++ counterpart of .NET Double.DegreesToRadians(double). */
    [[nodiscard]] static double DegreesToRadians(double degrees) noexcept { return degrees * (Pi / 180.0); }

    /** @brief Converts @p radians to degrees. C++ counterpart of .NET Double.RadiansToDegrees(double). */
    [[nodiscard]] static double RadiansToDegrees(double radians) noexcept { return radians * (180.0 / Pi); }

    // -----------------------------------------------------------------------
    // Comparison
    // -----------------------------------------------------------------------

    /**
     * @brief Returns negative/zero/positive when @p a is less than/equal to/greater than @p b.
     *
     * C++ counterpart of .NET Double.CompareTo(double). NaN compares as less than every
     * other value (including negative infinity) and equal to itself.
     */
    [[nodiscard]] static intcs CompareTo(double a, double b) noexcept
    {
        if (std::isnan(a) && std::isnan(b)) return 0;
        if (std::isnan(a)) return -1;
        if (std::isnan(b)) return  1;
        return static_cast<intcs>((a > b) - (a < b));
    }

    /**
     * @brief Returns true if @p a equals @p b.
     *
     * C++ counterpart of .NET Double.Equals(double). Unlike @c operator==, two NaN
     * values are considered equal to each other here.
     */
    [[nodiscard]] static bool Equals(double a, double b) noexcept
    {
        return a == b || (std::isnan(a) && std::isnan(b));
    }

    /**
     * @brief Returns a hash code for @p value.
     *
     * C++ counterpart of .NET Double.GetHashCode(). All NaN bit patterns and both
     * signed zeros hash identically, so that values considered Equals() (see above)
     * always produce the same hash code.
     */
    [[nodiscard]] static intcs GetHashCode(double value) noexcept
    {
        uint64_t bits;
        static_assert(sizeof(bits) == sizeof(value));
        __builtin_memcpy(&bits, &value, sizeof(bits));
        if (std::isnan(value) || value == 0.0) {
            constexpr uint64_t PositiveInfinityBits = 0x7FF0'0000'0000'0000ull;
            bits &= PositiveInfinityBits;
        }
        int32_t lo = static_cast<int32_t>(bits);
        int32_t hi = static_cast<int32_t>(bits >> 32);
        return static_cast<intcs>(lo ^ hi);
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
