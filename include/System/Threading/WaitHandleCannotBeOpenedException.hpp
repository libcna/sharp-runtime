// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Exception.hpp"

namespace System::Threading {

    class WaitHandleCannotBeOpenedException : public System::Exception {
    public:
        WaitHandleCannotBeOpenedException() : Exception("No handle of the given name exists.") {}
        explicit WaitHandleCannotBeOpenedException(const std::string& message) : Exception(message) {}
        WaitHandleCannotBeOpenedException(const std::string& message, const std::exception& inner)
            : Exception(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Threading
