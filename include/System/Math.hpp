#pragma once

#include "CppDotNet/CppDotNetHelper.hpp"

namespace System
{
    using CppDotNet::intcs;

    /**
     * @brief Provides constants and static methods for trigonometric,
     * logarithmic, and other common mathematical functions.
     *
     * This class is a lightweight C++ emulation of .NET System.Math.
     *
     * @note Status: PARTIAL
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
        static constexpr double E = 2.7182818284590452354;

        /**
         * @brief Represents the ratio of the circumference of a circle
         * to its diameter.
         *
         * @note Status: Ported
         */
        static constexpr double PI = 3.14159265358979323846;

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
    };
}