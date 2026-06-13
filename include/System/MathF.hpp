// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <limits>

namespace System {

    /// Single-precision counterpart of System.Math.
    class MathF {
    public:
        /// Deleted constructor — all members are static.
        MathF() = delete;

        static constexpr float E   = 2.71828182845904523536f; ///< Euler's number e.
        static constexpr float PI  = 3.14159265358979323846f; ///< The ratio of a circle's circumference to its diameter.
        static constexpr float Tau = 6.28318530717958647692f; ///< Tau (2*PI), the ratio of circumference to radius.

        /// Returns the absolute value of @p x.
        static float Abs(float x)                  { return std::abs(x); }
        /// Returns the smallest integral value greater than or equal to @p x.
        static float Ceiling(float x)              { return std::ceil(x); }
        /// Returns the largest integral value less than or equal to @p x.
        static float Floor(float x)                { return std::floor(x); }
        /// Rounds @p x to the nearest integer.
        static float Round(float x)                { return std::round(x); }
        /// Returns the integral part of @p x, discarding the fractional digits.
        static float Truncate(float x)             { return std::trunc(x); }
        /// Returns the square root of @p x.
        static float Sqrt(float x)                 { return std::sqrt(x); }
        /// Returns the cube root of @p x.
        static float Cbrt(float x)                 { return std::cbrt(x); }
        /// Returns e raised to the power @p x.
        static float Exp(float x)                  { return std::exp(x); }
        /// Returns the natural (base-e) logarithm of @p x.
        static float Log(float x)                  { return std::log(x); }
        /// @brief Returns the logarithm of @p x in the specified base @p y.
        /// @param x Value whose logarithm is to be found.
        /// @param y Logarithm base.
        /// @return log_y(x).
        static float Log(float x, float y)         { return std::log(x) / std::log(y); }
        /// Returns the base-2 logarithm of @p x.
        static float Log2(float x)                 { return std::log2(x); }
        /// Returns the base-10 logarithm of @p x.
        static float Log10(float x)                { return std::log10(x); }
        /// @brief Returns @p x raised to the power @p y.
        /// @param x Base.
        /// @param y Exponent.
        static float Pow(float x, float y)         { return std::pow(x, y); }
        /// Returns the sine of angle @p x (in radians).
        static float Sin(float x)                  { return std::sin(x); }
        /// Returns the cosine of angle @p x (in radians).
        static float Cos(float x)                  { return std::cos(x); }
        /// Returns the tangent of angle @p x (in radians).
        static float Tan(float x)                  { return std::tan(x); }
        /// Returns the arc sine of @p x in radians.
        static float Asin(float x)                 { return std::asin(x); }
        /// Returns the arc cosine of @p x in radians.
        static float Acos(float x)                 { return std::acos(x); }
        /// Returns the arc tangent of @p x in radians.
        static float Atan(float x)                 { return std::atan(x); }
        /// @brief Returns the angle whose tangent is @p y / @p x.
        /// @param y Numerator (y-coordinate).
        /// @param x Denominator (x-coordinate).
        /// @return Angle in radians in [-PI, PI].
        static float Atan2(float y, float x)       { return std::atan2(y, x); }
        /// Returns the hyperbolic sine of @p x.
        static float Sinh(float x)                 { return std::sinh(x); }
        /// Returns the hyperbolic cosine of @p x.
        static float Cosh(float x)                 { return std::cosh(x); }
        /// Returns the hyperbolic tangent of @p x.
        static float Tanh(float x)                 { return std::tanh(x); }
        /// Returns the larger of two values @p x and @p y (NaN-safe).
        static float Max(float x, float y)         { return std::fmax(x, y); }
        /// Returns the smaller of two values @p x and @p y (NaN-safe).
        static float Min(float x, float y)         { return std::fmin(x, y); }
        /// @brief Clamps @p v to the range [@p min, @p max].
        /// @param v Value to clamp.
        /// @param min Inclusive lower bound.
        /// @param max Inclusive upper bound.
        static float Clamp(float v, float min, float max) {
            return v < min ? min : (v > max ? max : v);
        }
        /// Returns -1, 0, or 1 indicating the sign of @p x.
        static float Sign(float x)                 { return x < 0 ? -1.0f : (x > 0 ? 1.0f : 0.0f); }
        /// Returns true if @p x is Not-a-Number (NaN).
        static bool IsNaN(float x)                 { return std::isnan(x); }
        /// Returns true if @p x is positive or negative infinity.
        static bool IsInfinity(float x)            { return std::isinf(x); }
        /// Returns true if @p x is positive infinity.
        static bool IsPositiveInfinity(float x)    { return x == std::numeric_limits<float>::infinity(); }
        /// Returns true if @p x is negative infinity.
        static bool IsNegativeInfinity(float x)    { return x == -std::numeric_limits<float>::infinity(); }
        /// @brief Returns the IEEE 754 remainder of @p x / @p y.
        /// @param x Dividend.
        /// @param y Divisor.
        static float IEEERemainder(float x, float y) { return std::remainder(x, y); }
        /// Returns the inverse hyperbolic cosine of @p x.
        static float Acosh(float x)                { return std::acosh(x); }
        /// Returns the inverse hyperbolic sine of @p x.
        static float Asinh(float x)                { return std::asinh(x); }
        /// Returns the inverse hyperbolic tangent of @p x.
        static float Atanh(float x)                { return std::atanh(x); }
        /// Returns a value with the magnitude of @p x and the sign of @p y.
        static float CopySign(float x, float y)    { return std::copysign(x, y); }
        /// Returns the smallest float greater than @p x.
        static float BitIncrement(float x)         { return std::nextafter(x,  std::numeric_limits<float>::infinity()); }
        /// Returns the largest float less than @p x.
        static float BitDecrement(float x)         { return std::nextafter(x, -std::numeric_limits<float>::infinity()); }
        /// Returns (x * y) + z, computed with a single rounding step.
        static float FusedMultiplyAdd(float x, float y, float z) { return std::fma(x, y, z); }
        /// Rounds @p x to @p digits decimal places.
        static float Round(float x, int digits)    {
            float factor = std::pow(10.0f, static_cast<float>(digits));
            return std::round(x * factor) / factor;
        }
    };

} // namespace System
