// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include "System/EventArgs.hpp"

namespace System {

    class UnhandledExceptionEventArgs : public EventArgs {
        std::exception_ptr exception_;
        bool isTerminating_;

    public:
        UnhandledExceptionEventArgs(std::exception_ptr ex, bool isTerminating)
            : exception_(ex), isTerminating_(isTerminating) {}

        [[nodiscard]] std::exception_ptr getExceptionObjectProperty() const { return exception_; }
        [[nodiscard]] bool               getIsTerminatingProperty()   const { return isTerminating_; }
    };

} // namespace System
