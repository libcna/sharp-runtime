// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <bit>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

#include "SharpRuntime/PortableFromChars.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ArithmeticException.hpp"
#include "System/FormatException.hpp"

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

    /** @brief Represents the smallest positive double value greater than zero (the smallest denormal, ~4.94E-324; NOT the smallest normal value). */
    static constexpr double Epsilon          =  std::numeric_limits<double>::denorm_min();

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
        constexpr uint64_t BiasedExponentMask = 0x7FF0'0000'0000'0000ull;
        // Trailing significand must be zero for a normal power of two (only the implicit
        // leading 1 bit); SR-AUD-030 (#1860): a subnormal (biased exponent 0) power of two
        // carries exactly one trailing-significand bit, so the normal rule wrongly rejects
        // Double::Epsilon and every subnormal power of two. Match .NET Double.IsPow2: for
        // subnormals accept exactly one significand bit. The zero/negative guard above and
        // !IsFinite already reject zero, negatives, NaN and infinity.
        constexpr uint64_t TrailingMask = 0x000F'FFFF'FFFF'FFFFull;
        if ((bits & BiasedExponentMask) == 0)
            return std::popcount(bits & TrailingMask) == 1;
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
        if (min > max) throw System::ArgumentException("min cannot be greater than max.");
        return Min(Max(value, min), max);
    }

    /**
     * @brief Returns a value with the magnitude of @p value and the sign of @p sign.
     */
    static double CopySign(double value, double sign)            { return std::copysign(value, sign); }

    /**
     * @brief Returns the larger of two double-precision floating-point numbers.
     *
     * C++ counterpart of .NET Double.Max(double,double) (matches IEEE 754-2019
     * `maximum`). Unlike std::fmax (which returns the non-NaN argument when one
     * side is NaN), this propagates NaN if either argument is NaN, and treats
     * +0 as greater than -0, matching .NET's actual semantics exactly.
     */
    static double Max(double x, double y) noexcept
    {
        if (x != y) {
            if (!std::isnan(x)) return y < x ? x : y;
            return x;
        }
        return std::signbit(y) ? x : y;
    }

    /**
     * @brief Returns the smaller of two double-precision floating-point numbers.
     *
     * C++ counterpart of .NET Double.Min(double,double) (matches IEEE 754-2019
     * `minimum`). Unlike std::fmin, this propagates NaN if either argument is
     * NaN, and treats +0 as greater than -0, matching .NET's actual semantics.
     */
    static double Min(double x, double y) noexcept
    {
        if (x != y) {
            if (!std::isnan(x)) return x < y ? x : y;
            return x;
        }
        return std::signbit(x) ? x : y;
    }

    /**
     * @brief Returns the value with the larger magnitude; if equal magnitude, returns the positive one.
     * C++ counterpart of .NET Double.MaxMagnitude(double,double).
     */
    [[nodiscard]] static double MaxMagnitude(double x, double y) noexcept
    {
        double ax = std::abs(x), ay = std::abs(y);
        if (ax > ay || std::isnan(ax)) return x;
        if (ax == ay) return std::signbit(x) ? y : x;
        return y;
    }

    /**
     * @brief Returns the value with the smaller magnitude; if equal magnitude, returns the negative one.
     * C++ counterpart of .NET Double.MinMagnitude(double,double).
     */
    [[nodiscard]] static double MinMagnitude(double x, double y) noexcept
    {
        double ax = std::abs(x), ay = std::abs(y);
        if (ax < ay || std::isnan(ax)) return x;
        if (ax == ay) return std::signbit(x) ? x : y;
        return y;
    }

    /**
     * @brief Returns an integer that indicates the sign of a double-precision floating-point number.
     *
     * Returns -1, 0, or 1.
     * @throws System::ArithmeticException if @p value is NaN, matching .NET.
     */
    static intcs Sign(double value)
    {
        if (value < 0.0) return -1;
        if (value > 0.0) return  1;
        if (value == 0.0) return 0;
        throw ArithmeticException("Function does not accept floating point Not-a-Number values.");
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

    /**
     * @brief Rounds @p x to @p digits decimal places (ties to even). C++ counterpart of .NET Double.Round(double,int).
     *
     * @param x      The value to round.
     * @param digits The number of decimal places; must be in [0, 15].
     * @throws System::ArgumentOutOfRangeException if @p digits is outside [0, 15].
     */
    [[nodiscard]] static double Round(double x, intcs digits)
    {
        // .NET's Double.Round(x,digits) forwards to Math.Round(x,digits,ToEven),
        // whose funnel validates `(uint)digits > maxRoundingDigits` first -- the
        // unsigned cast makes a NEGATIVE digits count throw as well. Adding that
        // check here required dropping this overload's `noexcept` (ticket #1862,
        // approved 2026-07-31): a throw from a noexcept function is
        // std::terminate, not an exception. Before the check, an out-of-range
        // digits count reached std::pow and returned a spurious value or NaN with
        // no diagnostic (Round(1.2345, 99) silently ignored the request,
        // Round(1.2345, INTCS_MIN) == NaN). The limit is 15 here and 6 for float,
        // exactly as .NET splits Math.Round from MathF.Round. The mangled name
        // does not encode noexcept, so this is a source-level change only -- no
        // ABI symbol break, no layout change.
        if (digits < 0 || digits > 15)
            throw System::ArgumentOutOfRangeException(
                "digits", "Rounding digits must be between 0 and 15, inclusive.");
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

private:
    [[nodiscard]] static double RootNPositive(double x, intcs n) noexcept {
        if (std::isfinite(x)) {
            if (x != 0.0) {
                if (x > 0.0 || (n % 2 != 0)) {
                    double result = std::pow(std::abs(x), 1.0 / n);
                    return std::copysign(result, x);
                }
                return std::numeric_limits<double>::quiet_NaN();
            }
            return (n % 2 == 0) ? 0.0 : std::copysign(0.0, x);
        }
        if (std::isnan(x)) return std::numeric_limits<double>::quiet_NaN();
        if (x > 0.0) return std::numeric_limits<double>::infinity();
        return (n % 2 != 0) ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::quiet_NaN();
    }

    [[nodiscard]] static double RootNNegative(double x, intcs n) noexcept {
        if (std::isfinite(x)) {
            if (x != 0.0) {
                if (x > 0.0 || (n % 2 != 0)) {
                    double result = std::pow(std::abs(x), 1.0 / n);
                    return std::copysign(result, x);
                }
                return std::numeric_limits<double>::quiet_NaN();
            }
            return (n % 2 == 0) ? std::numeric_limits<double>::infinity()
                                 : std::copysign(std::numeric_limits<double>::infinity(), x);
        }
        if (std::isnan(x)) return std::numeric_limits<double>::quiet_NaN();
        if (x > 0.0) return 0.0;
        return (n % 2 != 0) ? -0.0 : std::numeric_limits<double>::quiet_NaN();
    }

public:
    /** @brief Returns the n-th root of @p x. C++ counterpart of .NET Double.RootN(double,int). */
    [[nodiscard]] static double RootN(double x, intcs n) noexcept
    {
        if (n > 0) {
            if (n == 2) return x != 0.0 ? Sqrt(x) : 0.0;
            if (n == 3) return Cbrt(x);
            return RootNPositive(x, n);
        }
        if (n < 0) return RootNNegative(x, n);
        return std::numeric_limits<double>::quiet_NaN();
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

    /**
     * @brief Returns the sine of @p x * Pi. C++ counterpart of .NET Double.SinPi(double).
     *
     * Ported from the .NET `Double.SinPi` implementation (based on `sinpi` from
     * amd/aocl-libm-ose). Reduces the argument by its integral turn so exact
     * turn boundaries are honoured: integer turns return a sign-carried zero
     * (`x * 0.0`), half turns return `±1`, and non-finite inputs return NaN —
     * rather than a naive `std::sin(x * Pi)` that leaks tiny nonzero residues.
     */
    [[nodiscard]] static double SinPi(double x) noexcept {
        double result;

        if (IsFinite(x)) {
            double ax = Abs(x);

            if (ax < 4'503'599'627'370'496.0) {         // |x| < 2^52
                if (ax > 0.25) {
                    std::int64_t integral = static_cast<std::int64_t>(ax);

                    double fractional = ax - static_cast<double>(integral);
                    double sign = ((x > 0.0) ? +1.0 : -1.0) * (((integral & 1) != 0) ? -1.0 : +1.0);

                    if (fractional <= 0.25) {
                        if (fractional != 0.00) {
                            result = sign * SinForIntervalPiBy4(fractional * Pi, 0.0);
                        } else {
                            result = x * 0.0;
                        }
                    } else if (fractional <= 0.50) {
                        if (fractional != 0.50) {
                            result = sign * CosForIntervalPiBy4((0.5 - fractional) * Pi, 0.0);
                        } else {
                            result = sign;
                        }
                    } else if (fractional <= 0.75) {
                        result = sign * CosForIntervalPiBy4((fractional - 0.5) * Pi, 0.0);
                    } else {
                        result = sign * SinForIntervalPiBy4((1.0 - fractional) * Pi, 0.0);
                    }
                } else if (ax >= 1.220703125E-4) {          // |x| >= 2^-13
                    result = SinForIntervalPiBy4(x * Pi, 0.0);
                } else if (ax >= 7.450580596923828E-09) {   // |x| >= 2^-27
                    double value = x * Pi;
                    result = value - (value * value * value * (1.0 / 6.0));
                } else {
                    result = x * Pi;
                }
            } else {
                // x is an integer
                result = x * 0.0;
            }
        } else {
            result = NaN;
        }

        return result;
    }

    /**
     * @brief Returns the cosine of @p x * Pi. C++ counterpart of .NET Double.CosPi(double).
     *
     * Ported from the .NET `Double.CosPi` implementation (based on `cospi` from
     * amd/aocl-libm-ose). Honours exact turn boundaries: `CosPi(k+0.5) == +0`,
     * `CosPi(k) == ±1` by parity of @p k, and non-finite inputs return NaN.
     */
    [[nodiscard]] static double CosPi(double x) noexcept {
        double result;

        if (IsFinite(x)) {
            double ax = Abs(x);

            if (ax < 4'503'599'627'370'496.0) {         // |x| < 2^52
                if (ax > 0.25) {
                    std::int64_t integral = static_cast<std::int64_t>(ax);

                    double fractional = ax - static_cast<double>(integral);
                    double sign = ((integral & 1) != 0) ? -1.0 : +1.0;

                    if (fractional <= 0.25) {
                        if (fractional != 0.00) {
                            result = sign * CosForIntervalPiBy4(fractional * Pi, 0.0);
                        } else {
                            result = sign;
                        }
                    } else if (fractional <= 0.50) {
                        if (fractional != 0.50) {
                            result = sign * SinForIntervalPiBy4((0.5 - fractional) * Pi, 0.0);
                        } else {
                            result = 0.0;
                        }
                    } else if (fractional <= 0.75) {
                        result = -sign * SinForIntervalPiBy4((fractional - 0.5) * Pi, 0.0);
                    } else {
                        result = -sign * CosForIntervalPiBy4((1.0 - fractional) * Pi, 0.0);
                    }
                } else if (ax >= 6.103515625E-05) {         // |x| >= 2^-14
                    result = CosForIntervalPiBy4(x * Pi, 0.0);
                } else if (ax >= 7.450580596923828E-09) {   // |x| >= 2^-27
                    double value = x * Pi;
                    result = 1.0 - (value * value * 0.5);
                } else {
                    result = 1.0;
                }
            } else if (ax < 9'007'199'254'740'992.0) {  // |x| < 2^53
                // x is an integer
                std::uint64_t bits = std::bit_cast<std::uint64_t>(ax);
                result = ((bits & 1ull) != 0) ? -1.0 : +1.0;
            } else {
                // x is an even integer
                result = 1.0;
            }
        } else {
            result = NaN;
        }

        return result;
    }

    /**
     * @brief Returns the tangent of @p x * Pi. C++ counterpart of .NET Double.TanPi(double).
     *
     * Ported from the .NET `Double.TanPi` implementation (based on `tanpi` from
     * amd/aocl-libm-ose). Honours exact turn boundaries: integer turns return a
     * sign-carried zero, half turns return `±Infinity`, and non-finite inputs
     * return NaN.
     */
    [[nodiscard]] static double TanPi(double x) noexcept {
        double result;

        if (IsFinite(x)) {
            double ax = Abs(x);
            double sign = (x > 0.0) ? +1.0 : -1.0;

            if (ax < 4'503'599'627'370'496.0) {         // |x| < 2^52
                if (ax > 0.25) {
                    std::int64_t integral = static_cast<std::int64_t>(ax);
                    double fractional = ax - static_cast<double>(integral);

                    if (fractional <= 0.25) {
                        if (fractional != 0.00) {
                            result = sign * TanForIntervalPiBy4(fractional * Pi, 0.0, /*isReciprocal:*/ false);
                        } else {
                            result = sign * (((integral & 1) != 0) ? -0.0 : +0.0);
                        }
                    } else if (fractional <= 0.50) {
                        if (fractional != 0.50) {
                            result = -sign * TanForIntervalPiBy4((0.5 - fractional) * Pi, 0.0, /*isReciprocal:*/ true);
                        } else {
                            result = +sign * (((integral & 1) != 0) ? NegativeInfinity : PositiveInfinity);
                        }
                    } else if (fractional <= 0.75) {
                        result = +sign * TanForIntervalPiBy4((fractional - 0.5) * Pi, 0.0, /*isReciprocal:*/ true);
                    } else {
                        result = -sign * TanForIntervalPiBy4((1.0 - fractional) * Pi, 0.0, /*isReciprocal:*/ false);
                    }
                } else if (ax >= 6.103515625E-05) {         // |x| >= 2^-14
                    result = TanForIntervalPiBy4(x * Pi, 0.0, /*isReciprocal:*/ false);
                } else if (ax >= 7.450580596923828E-09) {   // |x| >= 2^-27
                    double value = x * Pi;
                    result = value + (value * value * value * (1.0 / 3.0));
                } else {
                    result = x * Pi;
                }
            } else if (ax < 9'007'199'254'740'992.0) {  // |x| < 2^53
                // x is an integer
                std::uint64_t bits = std::bit_cast<std::uint64_t>(ax);
                result = sign * (((bits & 1ull) != 0) ? -0.0 : +0.0);
            } else {
                // x is an even integer
                result = sign * 0.0;
            }
        } else {
            result = NaN;
        }

        return result;
    }

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
     *
     * Ported from the .NET `Double.SinCosPi` implementation (based on `sinpi`
     * and `cospi` from amd/aocl-libm-ose). Both fields honour the same exact
     * turn boundaries as SinPi/CosPi.
     */
    [[nodiscard]] static SinCosPiResult SinCosPi(double x) noexcept
    {
        double sinPi;
        double cosPi;

        if (IsFinite(x)) {
            double ax = Abs(x);

            if (ax < 4'503'599'627'370'496.0) {         // |x| < 2^52
                if (ax > 0.25) {
                    std::int64_t integral = static_cast<std::int64_t>(ax);

                    double fractional = ax - static_cast<double>(integral);
                    double sign = ((integral & 1) != 0) ? -1.0 : +1.0;

                    double sinSign = ((x > 0.0) ? +1.0 : -1.0) * sign;
                    double cosSign = sign;

                    if (fractional <= 0.25) {
                        if (fractional != 0.00) {
                            double value = fractional * Pi;

                            sinPi = sinSign * SinForIntervalPiBy4(value, 0.0);
                            cosPi = cosSign * CosForIntervalPiBy4(value, 0.0);
                        } else {
                            sinPi = x * 0.0;
                            cosPi = cosSign;
                        }
                    } else if (fractional <= 0.50) {
                        if (fractional != 0.50) {
                            double value = (0.5 - fractional) * Pi;

                            sinPi = sinSign * CosForIntervalPiBy4(value, 0.0);
                            cosPi = cosSign * SinForIntervalPiBy4(value, 0.0);
                        } else {
                            sinPi = sinSign;
                            cosPi = 0.0;
                        }
                    } else if (fractional <= 0.75) {
                        double value = (fractional - 0.5) * Pi;

                        sinPi = +sinSign * CosForIntervalPiBy4(value, 0.0);
                        cosPi = -cosSign * SinForIntervalPiBy4(value, 0.0);
                    } else {
                        double value = (1.0 - fractional) * Pi;

                        sinPi = +sinSign * SinForIntervalPiBy4(value, 0.0);
                        cosPi = -cosSign * CosForIntervalPiBy4(value, 0.0);
                    }
                } else if (ax >= 1.220703125E-4) {          // |x| >= 2^-13
                    double value = x * Pi;

                    sinPi = SinForIntervalPiBy4(value, 0.0);
                    cosPi = CosForIntervalPiBy4(value, 0.0);
                } else if (ax >= 7.450580596923828E-09) {   // |x| >= 2^-27
                    double value = x * Pi;
                    double valueSq = value * value;

                    sinPi = value - (valueSq * value * (1.0 / 6.0));
                    cosPi = 1.0 - (valueSq * 0.5);
                } else {
                    sinPi = x * Pi;
                    cosPi = 1.0;
                }
            } else if (ax < 9'007'199'254'740'992.0) {  // |x| < 2^53
                // x is an integer
                sinPi = x * 0.0;

                std::uint64_t bits = std::bit_cast<std::uint64_t>(ax);
                cosPi = ((bits & 1ull) != 0) ? -1.0 : +1.0;
            } else {
                // x is an even integer
                sinPi = x * 0.0;
                cosPi = 1.0;
            }
        } else {
            sinPi = NaN;
            cosPi = NaN;
        }

        return { sinPi, cosPi };
    }

private:
    // -----------------------------------------------------------------------
    // Pi-scaled trigonometric kernels (ported from .NET Double, based on
    // amd/aocl-libm-ose, BSD 3-Clause). Each evaluates the named function over
    // the reduced interval [0, Pi/4]. The @p xTail parameter carries the low
    // part of a two-word argument; the Pi-scaled callers above always pass 0.0
    // (their reduced arguments are single-word), but it is retained so the port
    // stays faithful to the reference.
    // -----------------------------------------------------------------------

    /** @brief Minimax cosine on [0, Pi/4] with a low-order tail correction. */
    [[nodiscard]] static double CosForIntervalPiBy4(double x, double xTail) noexcept
    {
        const double C1 = +0.41666666666666665390037E-1;    // approx: +1 / 4!
        const double C2 = -0.13888888888887398280412E-2;    // approx: -1 / 6!
        const double C3 = +0.248015872987670414957399E-4;   // approx: +1 / 8!
        const double C4 = -0.275573172723441909470836E-6;   // approx: -1 / 10!
        const double C5 = +0.208761463822329611076335E-8;   // approx: +1 / 12!
        const double C6 = -0.113826398067944859590880E-10;  // approx: -1 / 14!

        double xx = x * x;

        double tmp1 = 0.5 * xx;
        double tmp2 = 1.0 - tmp1;

        double result = C6;

        result = (result * xx) + C5;
        result = (result * xx) + C4;
        result = (result * xx) + C3;
        result = (result * xx) + C2;
        result = (result * xx) + C1;

        result *= (xx * xx);
        result += 1.0 - tmp2 - tmp1 - (x * xTail);
        result += tmp2;

        return result;
    }

    /** @brief Minimax sine on [0, Pi/4] with a low-order tail correction. */
    [[nodiscard]] static double SinForIntervalPiBy4(double x, double xTail) noexcept
    {
        const double C1 = -0.166666666666666646259241729;       // approx: -1 / 3!
        const double C2 = +0.833333333333095043065222816E-2;    // approx: +1 / 5!
        const double C3 = -0.19841269836761125688538679E-3;     // approx: -1 / 7!
        const double C4 = +0.275573161037288022676895908448E-5; // approx: +1 / 9!
        const double C5 = -0.25051132068021699772257377197E-7;  // approx: -1 / 11!
        const double C6 = +0.159181443044859136852668200E-9;    // approx: +1 / 13!

        double xx = x * x;
        double xxx = xx * x;

        double result = C6;

        result = (result * xx) + C5;
        result = (result * xx) + C4;
        result = (result * xx) + C3;
        result = (result * xx) + C2;

        if (xTail == 0.0) {
            result = (xx * result) + C1;
            result = (xxx * result) + x;
        } else {
            result = x - ((xx * ((0.5 * xTail) - (xxx * result))) - xTail - (xxx * C1));
        }

        return result;
    }

    /** @brief Remez [2, 3] tangent (or its negated reciprocal) on [0, Pi/4]. */
    [[nodiscard]] static double TanForIntervalPiBy4(double x, double xTail, bool isReciprocal) noexcept
    {
        // tan((pi/4) - x) = (1 - tan(x)) / (1 + tan(x)) is used to keep relative
        // precision for arguments close to (pi / 4).
        const double PiBy4Head = 7.85398163397448278999E-01;
        const double PiBy4Tail = 3.06161699786838240164E-17;

        int transform = 0;

        if (x > +0.68) {
            transform = 1;
            x = (PiBy4Head - x) + (PiBy4Tail - xTail);
            xTail = 0.0;
        } else if (x < -0.68) {
            transform = -1;
            x = (PiBy4Head + x) + (PiBy4Tail + xTail);
            xTail = 0.0;
        }

        double tmp1 = (x * x) + (2.0 * x * xTail);

        double denominator = -0.232371494088563558304549252913E-3;
        denominator = +0.260656620398645407524064091208E-1 + (denominator * tmp1);
        denominator = -0.515658515729031149329237816945E+0 + (denominator * tmp1);
        denominator = +0.111713747927937668539901657944E+1 + (denominator * tmp1);

        double numerator = +0.224044448537022097264602535574E-3;
        numerator = -0.229345080057565662883358588111E-1 + (numerator * tmp1);
        numerator = +0.372379159759792203640806338901E+0 + (numerator * tmp1);

        double tmp2 = x * tmp1;
        tmp2 *= numerator / denominator;
        tmp2 += xTail;

        double result = x + tmp2;

        if (transform != 0) {
            if (isReciprocal) {
                result = (transform * (2 * result / (result - 1))) - 1.0;
            } else {
                result = transform * (1.0 - (2 * result / (1 + result)));
            }
        } else if (isReciprocal) {
            // Compute -1.0 / (x + tmp2) accurately.
            std::uint64_t bits = std::bit_cast<std::uint64_t>(result);
            bits &= 0xFFFFFFFF00000000ull;

            double z1 = std::bit_cast<double>(bits);
            double z2 = tmp2 - (z1 - x);

            double reciprocal = -1.0 / result;

            bits = std::bit_cast<std::uint64_t>(reciprocal);
            bits &= 0xFFFFFFFF00000000ull;

            double reciprocalHead = std::bit_cast<double>(bits);
            result = reciprocalHead + (reciprocal * (1.0 + (reciprocalHead * z1) + (reciprocalHead * z2)));
        }

        return result;
    }

public:

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
    [[nodiscard]] static intcs ILogB(double x) noexcept {
        // SR-AUD-031 (#1859): .NET reserves Int32.MinValue for zero and returns
        // Int32.MaxValue for NaN and both infinities; std::ilogb's sentinels are
        // implementation-defined (NaN maps to INT_MIN on this toolchain, colliding with
        // zero). Classify explicitly, matching MathF::ILogB, before the finite std::ilogb.
        if (std::isnan(x) || std::isinf(x)) return std::numeric_limits<intcs>::max();
        if (x == 0.0) return std::numeric_limits<intcs>::min();
        return static_cast<intcs>(std::ilogb(x));
    }

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

private:
    static bool equalsIgnoreCaseAscii(std::string_view s, const char* token) noexcept {
        size_t n = std::char_traits<char>::length(token);
        if (s.size() != n) return false;
        for (size_t i = 0; i < n; ++i) {
            if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(token[i])))
                return false;
        }
        return true;
    }

    // .NET's default NumberStyles.Float allows leading/trailing whitespace via
    // AllowLeadingWhite | AllowTrailingWhite. The set is ASCII whitespace here
    // (space, tab, LF, VT, FF, CR); interior whitespace is never allowed.
    static bool isAsciiWhitespace(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
    }

    // Verified against Number.Parsing.cs's TryParseFloat: real .NET recognizes exactly
    // (case-insensitively) "Infinity"/"+Infinity"/"-Infinity" and "NaN"/"+NaN"/"-NaN" as
    // special tokens -- nothing else, including abbreviations like "inf" or "nan(...)". Prior
    // to this fix, this port only special-cased the exact-cased strings "NaN"/"Infinity"/
    // "-Infinity" and fell through to std::from_chars for everything else -- but from_chars's
    // floating-point grammar itself recognizes "inf"/"infinity"/"nan"/"nan(n-char-seq)"
    // case-insensitively regardless of chars_format (a C++ standard requirement, not an
    // implementation quirk), so inputs like "inf", "-inf", "INF", or "nan(123)" were silently
    // ACCEPTED here even though real .NET's double.Parse throws FormatException for all of
    // them. Confirmed via a standalone repro that TryParse("inf")/TryParse("nan(123)") both
    // returned true pre-fix.
    static bool tryParseCore(const std::string& s, double& result) noexcept {
        // Trim leading/trailing ASCII whitespace before parsing (SR-AUD-033
        // whitespace slice, ticket #1864): .NET's default double parse accepts
        // " 1.5 " and " NaN " but never interior whitespace or an
        // empty/all-whitespace string, all of which this trimmed view preserves.
        std::string_view sv{s};
        while (!sv.empty() && isAsciiWhitespace(sv.front())) sv.remove_prefix(1);
        while (!sv.empty() && isAsciiWhitespace(sv.back())) sv.remove_suffix(1);

        if (equalsIgnoreCaseAscii(sv, "NaN") || equalsIgnoreCaseAscii(sv, "+NaN") || equalsIgnoreCaseAscii(sv, "-NaN")) {
            result = std::numeric_limits<double>::quiet_NaN();
            return true;
        }
        if (equalsIgnoreCaseAscii(sv, "Infinity") || equalsIgnoreCaseAscii(sv, "+Infinity")) {
            result = std::numeric_limits<double>::infinity();
            return true;
        }
        if (equalsIgnoreCaseAscii(sv, "-Infinity")) {
            result = -std::numeric_limits<double>::infinity();
            return true;
        }
        // SharpRuntime::FromCharsFloat, not a bare std::from_chars call: Apple's libc++ omits
        // the floating-point std::from_chars overload entirely below a 13.3+ deployment target
        // (see PortableFromChars.hpp's own header comment) -- this stays correct either way
        // without forcing a higher minimum runtime macOS version.
        auto [ptr, ec] = SharpRuntime::FromCharsFloat(sv.data(), sv.data() + sv.size(), result);
        if (ec != std::errc{} || ptr != sv.data() + sv.size() || !std::isfinite(result)) {
            result = 0.0;
            return false;
        }
        return true;
    }

public:
    // -----------------------------------------------------------------------
    // Parse / TryParse
    // -----------------------------------------------------------------------

    /**
     * @brief Converts the string representation of a number to its double equivalent.
     *
     * Locale-independent (always uses '.'). Throws System::FormatException on failure.
     */
    static double Parse(const std::string& s) {
        double result{};
        if (!tryParseCore(s, result))
            throw System::FormatException("Input string was not in a correct format.");
        return result;
    }

    /**
     * @brief Tries to convert a string to a double. Returns false on failure.
     *
     * Locale-independent. Sets @p result to 0.0 on failure.
     */
    static bool TryParse(const std::string& s, double& result) {
        return tryParseCore(s, result);
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
        // SR-AUD-021 float slice (#1849 / CCF-006): guard the precision parse so a malformed
        // precision (e.g. "Fz", or an oversized width) surfaces as System::FormatException
        // rather than leaking std::invalid_argument/std::out_of_range, matching the integer
        // wrappers (#1847) and .NET's Format_BadFormatSpecifier.
        int prec = -1;
        if (format.size() > 1) {
            try {
                prec = std::stoi(format.substr(1));
            } catch (const std::exception&) {
                throw System::FormatException("Format specifier was invalid.");
            }
        }
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
        // SR-AUD-021 float slice (#1849 / CCF-006): an unrecognised specifier is a
        // FormatException in .NET, not a silent round-trip fallback (matching #1847 for the
        // integer wrappers). Only F/E/G/R/N (and lowercase) are implemented here; C/P/D/X/B and
        // any other letter are rejected loudly rather than returning a silently wrong value.
        throw System::FormatException("Format specifier was invalid.");
    }
};

} // namespace System
