// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/OperationCanceledException.hpp"
namespace System {
    OperationCanceledException::OperationCanceledException() : SystemException("The operation was canceled.") {}
    OperationCanceledException::OperationCanceledException(const char* message) : SystemException(message) {}
    OperationCanceledException::OperationCanceledException(const std::string& message) : SystemException(message) {}
} // namespace System
