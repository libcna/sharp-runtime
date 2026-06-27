// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <limits>
#include "System/MidpointRounding.hpp"

namespace System {

    /**
     * @brief Provides constants and static methods for trigonometric, logarithmic,
     * and other common mathematical functions for single-precision floating-point numbers.
     *
     * C++ counterpart of .NET System.MathF.
     * All members are static; instantiation is not allowed.
     */
    class MathF {
    public:
        /** @brief Deleted constructor — all members are static. */
        MathF() = delete;

        /** @brief Euler's number e. */
        static constexpr float E   = 2.71828182845904523536f;
        /** @brief The ratio of a circle's circumference to its diameter. */
        static constexpr float PI  = 3.14159265358979323846f;
        /** @brief Tau (2*PI), the ratio of circumference to radius. */
        static constexpr float Tau = 6.28318530717958647692f;

        /** @brief Returns the absolute value of @p x. */
        static float Abs(float x)                  { return std::abs(x); }
        /** @brief Returns the smallest integral value greater than or equal to @p x. */
        static float Ceiling(float x)              { return std::ceil(x); }
        /** @brief Returns the largest integral value less than or equal to @p x. */
        static float Floor(float x)                { return std::floor(x); }
        /** @brief Returns @p x rounded to the nearest integer. */
        static float Round(float x)                { return std::round(x); }
        /** @brief Returns the integral part of @p x, discarding the fractional digits. */
        static float Truncate(float x)             { return std::trunc(x); }
        /** @brief Returns the square root of @p x. */
        static float Sqrt(float x)                 { return std::sqrt(x); }
        /** @brief Returns the cube root of @p x. */
        static float Cbrt(float x)                 { return std::cbrt(x); }
        /** @brief Returns e raised to the power @p x. */
        static float Exp(float x)                  { return std::exp(x); }
        /** @brief Returns the natural (base-e) logarithm of @p x. */
        static float Log(float x)                  { return std::log(x); }

        /**
         * @brief Returns the logarithm of @p x in the specified base @p y.
         * @param x Value whose logarithm is to be found.
         * @param y Logarithm base.
         * @return log_y(x).
         */
        static float Log(float x, float y)         { return std::log(x) / std::log(y); }

        /** @brief Returns the base-2 logarithm of @p x. */
        static float Log2(float x)                 { return std::log2(x); }
        /** @brief Returns the base-10 logarithm of @p x. */
        static float Log10(float x)                { return std::log10(x); }

        /**
         * @brief Returns @p x raised to the power @p y.
         * @param x Base.
         * @param y Exponent.
         */
        static float Pow(float x, float y)         { return std::pow(x, y); }

        /** @brief Returns the sine of angle @p x (in radians). */
        static float Sin(float x)                  { return std::sin(x); }
        /** @brief Returns the cosine of angle @p x (in radians). */
        static float Cos(float x)                  { return std::cos(x); }
        /** @brief Returns the tangent of angle @p x (in radians). */
        static float Tan(float x)                  { return std::tan(x); }
        /** @brief Returns the arc sine of @p x in radians. */
        static float Asin(float x)                 { return std::asin(x); }
        /** @brief Returns the arc cosine of @p x in radians. */
        static float Acos(float x)                 { return std::acos(x); }
        /** @brief Returns the arc tangent of @p x in radians. */
        static float Atan(float x)                 { return std::atan(x); }

        /**
         * @brief Returns the angle whose tangent is @p y / @p x.
         * @param y Numerator (y-coordinate).
         * @param x Denominator (x-coordinate).
         * @return Angle in radians in [-PI, PI].
         */
        static float Atan2(float y, float x)       { return std::atan2(y, x); }

        /** @brief Returns the hyperbolic sine of @p x. */
        static float Sinh(float x)                 { return std::sinh(x); }
        /** @brief Returns the hyperbolic cosine of @p x. */
        static float Cosh(float x)                 { return std::cosh(x); }
        /** @brief Returns the hyperbolic tangent of @p x. */
        static float Tanh(float x)                 { return std::tanh(x); }
        /** @brief Returns the larger of @p x and @p y (NaN-safe). */
        static float Max(float x, float y)         { return std::fmax(x, y); }
        /** @brief Returns the smaller of @p x and @p y (NaN-safe). */
        static float Min(float x, float y)         { return std::fmin(x, y); }

        /**
         * @brief Clamps @p v to the range [@p min, @p max].
         * @param v   Value to clamp.
         * @param min Inclusive lower bound.
         * @param max Inclusive upper bound.
         */
        static float Clamp(float v, float min, float max) {
            return v < min ? min : (v > max ? max : v);
        }

        /** @brief Returns -1, 0, or 1 indicating the sign of @p x. */
        static int Sign(float x)                   { return x < 0.0f ? -1 : (x > 0.0f ? 1 : 0); }
        /** @brief Returns true if @p x is Not-a-Number (NaN). */
        static bool IsNaN(float x)                 { return std::isnan(x); }
        /** @brief Returns true if @p x is positive or negative infinity. */
        static bool IsInfinity(float x)            { return std::isinf(x); }
        /** @brief Returns true if @p x is positive infinity. */
        static bool IsPositiveInfinity(float x)    { return x == std::numeric_limits<float>::infinity(); }
        /** @brief Returns true if @p x is negative infinity. */
        static bool IsNegativeInfinity(float x)    { return x == -std::numeric_limits<float>::infinity(); }

        /**
         * @brief Returns the IEEE 754 remainder of @p x / @p y.
         * @param x Dividend.
         * @param y Divisor.
         */
        static float IEEERemainder(float x, float y) { return std::remainder(x, y); }

        /** @brief Returns the inverse hyperbolic cosine of @p x. */
        static float Acosh(float x)                { return std::acosh(x); }
        /** @brief Returns the inverse hyperbolic sine of @p x. */
        static float Asinh(float x)                { return std::asinh(x); }
        /** @brief Returns the inverse hyperbolic tangent of @p x. */
        static float Atanh(float x)                { return std::atanh(x); }
        /** @brief Returns a value with the magnitude of @p x and the sign of @p y. */
        static float CopySign(float x, float y)    { return std::copysign(x, y); }
        /** @brief Returns the smallest float greater than @p x. */
        static float BitIncrement(float x)         { return std::nextafter(x,  std::numeric_limits<float>::infinity()); }
        /** @brief Returns the largest float less than @p x. */
        static float BitDecrement(float x)         { return std::nextafter(x, -std::numeric_limits<float>::infinity()); }
        /** @brief Returns (x * y) + z, computed with a single rounding step. */
        static float FusedMultiplyAdd(float x, float y, float z) { return std::fma(x, y, z); }

        /**
         * @brief Rounds @p x to @p digits decimal places using away-from-zero midpoint.
         * @param x      Value to round.
         * @param digits Number of decimal places.
         */
        static float Round(float x, int digits) {
            float factor = std::pow(10.0f, static_cast<float>(digits));
            return std::round(x * factor) / factor;
        }

        /**
         * @brief Rounds @p x to an integer using the specified rounding convention.
         * @param x    Value to round.
         * @param mode Rounding convention.
         */
        static float Round(float x, MidpointRounding mode) {
            switch (mode) {
                case MidpointRounding::ToEven:             return std::nearbyintf(x);
                case MidpointRounding::AwayFromZero:       return std::roundf(x);
                case MidpointRounding::ToZero:             return std::truncf(x);
                case MidpointRounding::ToNegativeInfinity: return std::floorf(x);
                case MidpointRounding::ToPositiveInfinity: return std::ceilf(x);
                default:                                   return std::nearbyintf(x);
            }
        }

        /**
         * @brief Rounds @p x to @p digits decimal places using the specified rounding convention.
         * @param x      Value to round.
         * @param digits Number of decimal places.
         * @param mode   Rounding convention.
         */
        static float Round(float x, int digits, MidpointRounding mode) {
            float factor = std::pow(10.0f, static_cast<float>(digits));
            return Round(x * factor, mode) / factor;
        }

        /** @brief Returns true if @p x is finite (not NaN or infinity). */
        static bool IsFinite(float x)              { return std::isfinite(x); }
        /** @brief Returns true if @p x is a normal floating-point value (not zero, subnormal, NaN, or infinite). */
        static bool IsNormal(float x)              { return std::isnormal(x); }
        /** @brief Returns true if @p x is subnormal (denormalized). */
        static bool IsSubnormal(float x)           { return std::fpclassify(x) == FP_SUBNORMAL; }
        /** @brief Returns true if @p x is negative (including -0 and -infinity). */
        static bool IsNegative(float x)            { return std::signbit(x); }
        /** @brief Returns @p x multiplied by 2 raised to the power @p n. */
        static float ScaleB(float x, int n)        { return std::scalbn(x, n); }
        /** @brief Returns the base-2 integer exponent of @p x (i.e. floor(log2(|x|))). */
        static int   ILogB(float x)                { return std::ilogb(x); }
        /** @brief Returns an estimate of the reciprocal of @p x (1/x). */
        static float ReciprocalEstimate(float x)   { return 1.0f / x; }
        /** @brief Returns an estimate of the reciprocal square root of @p x (1/sqrt(x)). */
        static float ReciprocalSqrtEstimate(float x) { return 1.0f / std::sqrt(x); }

        /**
         * @brief Returns the value with the greater magnitude; if equal, returns the positive one.
         * @param x First value.
         * @param y Second value.
         */
        static float MaxMagnitude(float x, float y) {
            float ax = std::fabs(x), ay = std::fabs(y);
            if (ax > ay || std::isnan(ax)) return x;
            if (ax == ay) return std::signbit(x) ? y : x;
            return y;
        }

        /**
         * @brief Returns the value with the lesser magnitude; if equal, returns the negative one.
         * @param x First value.
         * @param y Second value.
         */
        static float MinMagnitude(float x, float y) {
            float ax = std::fabs(x), ay = std::fabs(y);
            if (ax < ay || std::isnan(ax)) return x;
            if (ax == ay) return std::signbit(x) ? x : y;
            return y;
        }

        /** Holds the result of SinCos — the sine and cosine of an angle computed simultaneously. */
        struct SinCosResult { float Sin; float Cos; };

        /**
         * @brief Computes the sine and cosine of @p x simultaneously.
         * @param x Angle in radians.
         * @return A SinCosResult with Sin and Cos fields.
         */
        static SinCosResult SinCos(float x) {
            return { std::sin(x), std::cos(x) };
        }
    };

} // namespace System
