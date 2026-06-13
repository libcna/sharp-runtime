// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ArithmeticException.hpp"

namespace System {

    /// Exception thrown when a floating-point value is not finite (NaN or infinity), mirroring .NET NotFiniteNumberException.
    class NotFiniteNumberException : public ArithmeticException {
        double offending_ = 0.0;
    public:
        /// Constructs the exception with a default message and offending value of 0.
        NotFiniteNumberException() : ArithmeticException("The number encountered was not a finite quantity.") {}
        /// @brief Constructs the exception with the non-finite value that triggered it.
        /// @param offendingNumber The NaN or infinite value that caused the exception.
        explicit NotFiniteNumberException(double offendingNumber)
            : ArithmeticException("The number encountered was not a finite quantity."), offending_(offendingNumber) {}
        /// @brief Constructs the exception with a custom message and the offending value.
        /// @param message Descriptive error message.
        /// @param offendingNumber The NaN or infinite value that caused the exception.
        NotFiniteNumberException(const std::string& message, double offendingNumber)
            : ArithmeticException(message), offending_(offendingNumber) {}

        /// Returns the floating-point value (NaN or infinity) that caused the exception.
        [[nodiscard]] double getOffendingNumberProperty() const { return offending_; }
    };

} // namespace System
