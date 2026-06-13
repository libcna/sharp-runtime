// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include "System/EventArgs.hpp"

namespace System {

    /// Event arguments passed to an unhandled-exception handler, mirroring .NET UnhandledExceptionEventArgs.
    class UnhandledExceptionEventArgs : public EventArgs {
        std::exception_ptr exception_;
        bool isTerminating_;

    public:
        /// @brief Constructs the event args with the offending exception and a termination flag.
        /// @param ex Exception pointer wrapping the unhandled exception.
        /// @param isTerminating True if the runtime is about to terminate as a result.
        UnhandledExceptionEventArgs(std::exception_ptr ex, bool isTerminating)
            : exception_(ex), isTerminating_(isTerminating) {}

        /// Returns the exception that caused the event.
        [[nodiscard]] std::exception_ptr getExceptionObjectProperty() const { return exception_; }
        /// Returns true if the process is terminating due to the unhandled exception.
        [[nodiscard]] bool               getIsTerminatingProperty()   const { return isTerminating_; }
    };

} // namespace System
