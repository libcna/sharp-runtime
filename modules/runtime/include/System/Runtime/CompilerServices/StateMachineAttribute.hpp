// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <utility>
#include "System/Attribute.hpp"
#include "System/Type.hpp"

namespace System::Runtime::CompilerServices {

/**
 * Specifies the type of state machine generated for an asynchronous or iterator method.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.StateMachineAttribute. C++ has no
 * compiler-generated CLR state-machine metadata, so this attribute only retains the supplied type.
 */
class StateMachineAttribute : public System::Attribute {
    System::Type stateMachineType_;

public:
    /** Initializes the attribute with the generated state-machine type. */
    explicit StateMachineAttribute(System::Type stateMachineType)
        : stateMachineType_(std::move(stateMachineType)) {}

    /** Gets the type of the generated state machine. */
    [[nodiscard]] const System::Type& getStateMachineTypeProperty() const noexcept {
        return stateMachineType_;
    }
};

} // namespace System::Runtime::CompilerServices
