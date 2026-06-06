// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 6/5/25.
//

#include "System/UnauthorizedAccessException.hpp"

namespace System {

    UnauthorizedAccessException::UnauthorizedAccessException()
        : SystemException() {
    }

    UnauthorizedAccessException::UnauthorizedAccessException(const char* str)
        : SystemException(str) {
    }

    UnauthorizedAccessException::UnauthorizedAccessException(const std::string& str)
        : SystemException(str) {
    }

} // namespace System