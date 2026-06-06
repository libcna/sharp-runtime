// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ArithmeticException.hpp"

namespace System {

    class NotFiniteNumberException : public ArithmeticException {
        double offending_ = 0.0;
    public:
        NotFiniteNumberException() : ArithmeticException("The number encountered was not a finite quantity.") {}
        explicit NotFiniteNumberException(double offendingNumber)
            : ArithmeticException("The number encountered was not a finite quantity."), offending_(offendingNumber) {}
        NotFiniteNumberException(const std::string& message, double offendingNumber)
            : ArithmeticException(message), offending_(offendingNumber) {}

        [[nodiscard]] double getOffendingNumberProperty() const { return offending_; }
    };

} // namespace System
