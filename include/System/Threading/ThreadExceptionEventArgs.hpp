// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include "System/EventArgs.hpp"

namespace System::Threading {

    class ThreadExceptionEventArgs : public System::EventArgs {
        std::exception_ptr exception_;
    public:
        explicit ThreadExceptionEventArgs(std::exception_ptr ex) : exception_(std::move(ex)) {}

        [[nodiscard]] std::exception_ptr getExceptionProperty() const { return exception_; }
    };

} // namespace System::Threading
