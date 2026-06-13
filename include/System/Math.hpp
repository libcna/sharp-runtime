// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstdint>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System
{
    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /**
     * @brief Provides constants and static methods for trigonometric,
     * logarithmic, and other common mathematical functions.
     *
     * C++ counterpart of .NET System.Math.
     *
     * @note Status: DONE
     */
    class Math
    {
    public:
        Math() = delete;
        ~Math() = delete;

        /**
         * @brief Represents the base of natural logarithms.
         *
         * @note Status: Ported
         */
        static constexpr double E   = 2.71828'18284'59045'2354;  ///< Base of natural logarithms.
        static constexpr double PI  = 3.14159'26535'89793'23846; ///< Ratio of a circle's circumference to its diameter.
        static constexpr double Tau = 6.28318'53071'79586'47692; ///< Two times PI (full circle in radians).

        /**
         * @brief Returns the sine of the specified angle.
         *
         * The angle is specified in radians.
         *
         * @param value Angle in radians.
         * @return Sine of the angle.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static double Sin(double value);

        /**
         * @brief Returns the cosine of the specified angle.
         *
         * The angle is specified in radians.
         *
         * @param value Angle in radians.
         * @return Cosine of the angle.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static double Cos(double value);

        /**
         * @brief Returns the tangent of the specified angle.
         *
         * The angle is specified in radians.
         *
         * @param value Angle in radians.
         * @return Tangent of the angle.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static double Tan(double value);

        /**
         * @brief Returns the square root of a specified number.
         *
         * @param value Input value.
         * @return Square root of the input value.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static double Sqrt(double value);

        /**
         * @brief Returns the absolute value of a double-precision number.
         *
         * @param value Input value.
         * @return Absolute value.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static double Abs(double value);

        /**
         * @brief Returns the absolute value of a 32-bit signed integer.
         *
         * @param value Input value.
         * @return Absolute value.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static intcs Abs(intcs value);

        /**
         * @brief Returns the smaller of two 32-bit signed integers.
         *
         * @param a First value.
         * @param b Second value.
         * @return Smaller value.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static intcs Min(intcs a, intcs b);

        /**
         * @brief Returns the smaller of two double-precision numbers.
         *
         * @param a First value.
         * @param b Second value.
         * @return Smaller value.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static double Min(double a, double b);

        /**
         * @brief Returns the larger of two 32-bit signed integers.
         *
         * @param a First value.
         * @param b Second value.
         * @return Larger value.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static intcs Max(intcs a, intcs b);

        /**
         * @brief Returns the larger of two double-precision numbers.
         *
         * @param a First value.
         * @param b Second value.
         * @return Larger value.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static double Max(double a, double b);

        /**
         * @brief Restricts an integer value to the specified range.
         *
         * @param value Value to clamp.
         * @param min Minimum allowed value.
         * @param max Maximum allowed value.
         * @return Clamped value.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static intcs Clamp(intcs value, intcs min, intcs max);

        /**
         * @brief Restricts a double value to the specified range.
         *
         * @param value Value to clamp.
         * @param min Minimum allowed value.
         * @param max Maximum allowed value.
         * @return Clamped value.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static double Clamp(double value, double min, double max);

        /// Returns the absolute value of a 64-bit signed integer.
        [[nodiscard]] static longcs Abs(longcs value);
        /// Returns the smaller of two 64-bit signed integers.
        [[nodiscard]] static longcs Min(longcs a, longcs b);
        /// Returns the larger of two 64-bit signed integers.
        [[nodiscard]] static longcs Max(longcs a, longcs b);
        /// Clamps a 64-bit signed integer to [@p min, @p max].
        [[nodiscard]] static longcs Clamp(longcs value, longcs min, longcs max);

        /**
         * @brief Rounds a double-precision floating-point value to the nearest integer.
         *
         * @param value Input value.
         * @return Rounded value.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static double Round(double value);

        /**
         * @brief Returns the largest integer less than or equal to the specified number.
         *
         * @param value Input value.
         * @return Floor of the input value.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static double Floor(double value);

        /**
         * @brief Returns the smallest integer greater than or equal to the specified number.
         *
         * @param value Input value.
         * @return Ceiling of the input value.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static double Ceiling(double value);

        /**
         * @brief Returns a specified number raised to the specified power.
         *
         * @param x Base value.
         * @param y Exponent value.
         * @return x raised to the power y.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static double Pow(double x, double y);

        /// @brief Returns the natural (base-e) logarithm of @p d.
        [[nodiscard]] static double Log(double d);
        /// @brief Returns the logarithm of @p a in the specified @p newBase.
        [[nodiscard]] static double Log(double a, double newBase);
        /// @brief Returns the base-2 logarithm of @p x.
        [[nodiscard]] static double Log2(double x);
        /// @brief Returns the base-10 logarithm of @p d.
        [[nodiscard]] static double Log10(double d);
        /// @brief Returns e raised to the power @p d.
        [[nodiscard]] static double Exp(double d);

        /// @brief Returns the angle whose sine is @p d (in radians).
        [[nodiscard]] static double Asin(double d);
        /// @brief Returns the angle whose cosine is @p d (in radians).
        [[nodiscard]] static double Acos(double d);
        /// @brief Returns the angle whose tangent is @p d (in radians).
        [[nodiscard]] static double Atan(double d);
        /// @brief Returns the angle (in radians) whose tangent is the quotient of @p y and @p x.
        [[nodiscard]] static double Atan2(double y, double x);

        /// @brief Returns the hyperbolic sine of @p value.
        [[nodiscard]] static double Sinh(double value);
        /// @brief Returns the hyperbolic cosine of @p value.
        [[nodiscard]] static double Cosh(double value);
        /// @brief Returns the hyperbolic tangent of @p value.
        [[nodiscard]] static double Tanh(double value);

        /// @brief Returns an integer indicating the sign of a 32-bit integer (-1, 0, or 1).
        [[nodiscard]] static intcs Sign(intcs value);
        /// @brief Returns an integer indicating the sign of a double (-1, 0, or 1).
        [[nodiscard]] static intcs Sign(double value);

        /// @brief Returns the integral part of @p d (discards the fractional part).
        [[nodiscard]] static double Truncate(double d);

        /// @brief Returns the IEEE 754 remainder of @p x divided by @p y.
        [[nodiscard]] static double IEEERemainder(double x, double y);

        /// @brief Divides @p a by @p b and stores the remainder in @p result; returns the quotient.
        static intcs DivRem(intcs a, intcs b, intcs& result);

        /// @brief Returns the 64-bit product of two 32-bit integers.
        [[nodiscard]] static longcs BigMul(intcs a, intcs b);

        /// @brief Returns @p x multiplied by 2 raised to the power @p n (scalbn).
        [[nodiscard]] static double ScaleB(double x, intcs n);

        /// @brief Returns the cube root of @p x.
        [[nodiscard]] static double Cbrt(double x);

        /// @brief Returns the angle whose hyperbolic cosine is @p d.
        [[nodiscard]] static double Acosh(double d);

        /// @brief Returns the angle whose hyperbolic sine is @p d.
        [[nodiscard]] static double Asinh(double d);

        /// @brief Returns the angle whose hyperbolic tangent is @p d.
        [[nodiscard]] static double Atanh(double d);

        /// @brief Rounds @p value to @p digits decimal places using banker's rounding.
        [[nodiscard]] static double Round(double value, intcs digits);

        /// @brief Returns a value with the magnitude of @p x and the sign of @p y.
        [[nodiscard]] static double CopySign(double x, double y);

        /// @brief Returns the smallest value greater than @p x.
        [[nodiscard]] static double BitIncrement(double x);

        /// @brief Returns the largest value less than @p x.
        [[nodiscard]] static double BitDecrement(double x);

        /// @brief Returns @p x × @p y + @p z computed as a single fused operation.
        [[nodiscard]] static double FusedMultiplyAdd(double x, double y, double z);
    };
}