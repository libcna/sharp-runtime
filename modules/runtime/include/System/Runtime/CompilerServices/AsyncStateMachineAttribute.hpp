// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Runtime/CompilerServices/StateMachineAttribute.hpp"

namespace System::Runtime::CompilerServices {

/**
 * Identifies the state machine generated for an asynchronous method.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.AsyncStateMachineAttribute.
 */
class AsyncStateMachineAttribute final : public StateMachineAttribute {
public:
    /** Initializes the attribute with the generated state-machine type. */
    explicit AsyncStateMachineAttribute(System::Type stateMachineType)
        : StateMachineAttribute(std::move(stateMachineType)) {}
};

} // namespace System::Runtime::CompilerServices
