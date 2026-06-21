// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include "System/ArgumentException.hpp"

namespace System {

/**
 * @brief The exception that is thrown when the value of an argument is outside
 *        the allowable range of values as defined by the invoked method.
 *
 * C++ counterpart of .NET System.ArgumentOutOfRangeException.
 * Derives from ArgumentException and adds an optional actual-value string that
 * identifies the out-of-range value.
 */
class ArgumentOutOfRangeException : public ArgumentException {
public:
    /**
     * @brief Initializes a new instance with the default error message.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException().
     */
    ArgumentOutOfRangeException();

    /**
     * @brief Initializes a new instance with the specified error message.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException(string).
     * @param message A null-terminated string that describes the error.
     */
    explicit ArgumentOutOfRangeException(const char* message);

    /**
     * @brief Initializes a new instance with the specified error message.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException(string).
     * @param message A string that describes the error.
     */
    explicit ArgumentOutOfRangeException(const std::string& message);

    /**
     * @brief Initializes a new instance with an error message and inner exception.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException(string, Exception).
     * @param message The error message.
     * @param inner   The exception that is the cause of the current exception.
     */
    ArgumentOutOfRangeException(const std::string& message,
                                const std::exception& inner);

    /**
     * @brief Initializes a new instance with a parameter name, actual value, and message.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException(string, object, string).
     * @param paramName   The name of the parameter that caused the exception.
     * @param actualValue The value of the argument that caused the exception (as string).
     * @param message     The error message.
     */
    ArgumentOutOfRangeException(const std::string& paramName,
                                const std::string& actualValue,
                                const std::string& message);

    /**
     * @brief Gets a string representation of the actual value that caused the exception.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ActualValue.
     * Returns an empty string if no actual value was provided.
     * @return The actual value, or an empty string.
     */
    [[nodiscard]] virtual std::string getActualValueProperty() const { return actualValue_; }

    // -----------------------------------------------------------------------
    // Static guard helpers  (C++ counterparts of .NET ThrowIf* methods)
    // -----------------------------------------------------------------------

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is zero.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfZero.
     * @tparam T  An arithmetic type.
     * @param value     The value to check.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfZero(T value, const std::string& paramName = "")
    {
        if (value == T{})
            throw ArgumentOutOfRangeException(paramName, std::to_string(value),
                "'" + paramName + "' must be a non-zero value.");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is negative.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfNegative.
     * @tparam T  An arithmetic type.
     * @param value     The value to check.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfNegative(T value, const std::string& paramName = "")
    {
        if (value < T{})
            throw ArgumentOutOfRangeException(paramName, std::to_string(value),
                "'" + paramName + "' must be a non-negative value.");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is negative or zero.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfNegativeOrZero.
     * @tparam T  An arithmetic type.
     * @param value     The value to check.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfNegativeOrZero(T value, const std::string& paramName = "")
    {
        if (value <= T{})
            throw ArgumentOutOfRangeException(paramName, std::to_string(value),
                "'" + paramName + "' must be a positive value.");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is greater than @p other.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfGreaterThan.
     * @tparam T  A totally ordered type.
     * @param value     The value to check.
     * @param other     The upper bound (exclusive).
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfGreaterThan(T value, T other, const std::string& paramName = "")
    {
        if (value > other)
            throw ArgumentOutOfRangeException(paramName, std::to_string(value),
                "'" + paramName + "' must be less than or equal to " + std::to_string(other) + ".");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is greater than or equal to @p other.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfGreaterThanOrEqual.
     * @tparam T  A totally ordered type.
     * @param value     The value to check.
     * @param other     The exclusive upper bound.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfGreaterThanOrEqual(T value, T other, const std::string& paramName = "")
    {
        if (value >= other)
            throw ArgumentOutOfRangeException(paramName, std::to_string(value),
                "'" + paramName + "' must be less than " + std::to_string(other) + ".");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is less than @p other.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfLessThan.
     * @tparam T  A totally ordered type.
     * @param value     The value to check.
     * @param other     The lower bound (exclusive).
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfLessThan(T value, T other, const std::string& paramName = "")
    {
        if (value < other)
            throw ArgumentOutOfRangeException(paramName, std::to_string(value),
                "'" + paramName + "' must be greater than or equal to " + std::to_string(other) + ".");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is less than or equal to @p other.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfLessThanOrEqual.
     * @tparam T  A totally ordered type.
     * @param value     The value to check.
     * @param other     The inclusive lower bound.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfLessThanOrEqual(T value, T other, const std::string& paramName = "")
    {
        if (value <= other)
            throw ArgumentOutOfRangeException(paramName, std::to_string(value),
                "'" + paramName + "' must be greater than " + std::to_string(other) + ".");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value equals @p other.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfEqual.
     * @tparam T  An equality-comparable type.
     * @param value     The value to check.
     * @param other     The value it must not equal.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfEqual(T value, T other, const std::string& paramName = "")
    {
        if (value == other)
            throw ArgumentOutOfRangeException(paramName, std::to_string(value),
                "'" + paramName + "' must not equal " + std::to_string(other) + ".");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value does not equal @p other.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfNotEqual.
     * @tparam T  An equality-comparable type.
     * @param value     The value to check.
     * @param other     The value it must equal.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfNotEqual(T value, T other, const std::string& paramName = "")
    {
        if (value != other)
            throw ArgumentOutOfRangeException(paramName, std::to_string(value),
                "'" + paramName + "' must equal " + std::to_string(other) + ".");
    }

private:
    std::string actualValue_;
};

} // namespace System
