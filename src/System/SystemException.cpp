// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 6/5/25.
//

#include "System/SystemException.hpp"

namespace System {

    SystemException::SystemException()
        : Exception("System error.") {}

    SystemException::SystemException(const char* str)
        : Exception(str) {}

    SystemException::SystemException(const std::string& str)
        : Exception(str) {}

    SystemException::SystemException(const std::string& str, std::exception_ptr inner)
        : Exception(str, inner) {}

} // namespace System
