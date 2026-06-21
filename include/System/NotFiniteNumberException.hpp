// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ArithmeticException.hpp"

namespace System {

    /**
     * @brief The exception thrown when a floating-point value is positive infinity,
     * negative infinity, or Not-a-Number (NaN).
     *
     * C++ counterpart of .NET System.NotFiniteNumberException.
     */
    class NotFiniteNumberException : public ArithmeticException {
        double offending_ = 0.0;
    public:
        /** @brief Initializes a new instance with the default not-finite message. */
        NotFiniteNumberException() : ArithmeticException("The number encountered was not a finite quantity.") {}
        /**
         * @brief Initializes a new instance with the non-finite value that triggered it.
         * @param offendingNumber The NaN or infinite value that caused the exception.
         */
        explicit NotFiniteNumberException(double offendingNumber)
            : ArithmeticException("The number encountered was not a finite quantity."), offending_(offendingNumber) {}
        /**
         * @brief Initializes a new instance with a custom message and the offending value.
         * @param message Descriptive error message.
         * @param offendingNumber The NaN or infinite value that caused the exception.
         */
        NotFiniteNumberException(const std::string& message, double offendingNumber)
            : ArithmeticException(message), offending_(offendingNumber) {}

        /** @brief Returns the floating-point value (NaN or infinity) that caused the exception. */
        [[nodiscard]] double getOffendingNumberProperty() const { return offending_; }
    };

} // namespace System
