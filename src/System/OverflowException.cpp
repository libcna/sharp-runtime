// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 6/5/25.
//

#include "System/OverflowException.hpp"

namespace System {

    OverflowException::OverflowException()
        : ArithmeticException("Arithmetic operation resulted in an overflow.") {
    }

    OverflowException::OverflowException(const char* str)
        : ArithmeticException(str) {
    }

} // namespace System