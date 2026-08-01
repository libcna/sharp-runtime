// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/SystemException.hpp"

namespace System::Runtime {

    class AmbiguousImplementationException : public System::SystemException {
    public:
        AmbiguousImplementationException()
            : System::SystemException("Ambiguous implementation found.") {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x8013106Au)); // COR_E_AMBIGUOUSIMPLEMENTATION
        }
        explicit AmbiguousImplementationException(const std::string& message)
            : System::SystemException(message) {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x8013106Au)); // COR_E_AMBIGUOUSIMPLEMENTATION
        }
    };

} // namespace System::Runtime
