// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include "System/EventArgs.hpp"

namespace System::Threading {

    /// Provides data for the ThreadException event.
    class ThreadExceptionEventArgs : public System::EventArgs {
        std::exception_ptr exception_;
    public:
        /// Constructs ThreadExceptionEventArgs from the given exception pointer.
        explicit ThreadExceptionEventArgs(std::exception_ptr ex) : exception_(std::move(ex)) {}

        /// Returns the exception that caused the event.
        [[nodiscard]] std::exception_ptr getExceptionProperty() const { return exception_; }
    };

} // namespace System::Threading
