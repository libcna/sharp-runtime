// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <functional>

#include "System/EventArgs.hpp"

namespace System::ComponentModel
{
    class CancelEventArgs : public System::EventArgs
    {
        bool cancel_ = false;

    public:
        /** Initializes event data with Cancel set to false. */
        CancelEventArgs() = default;

        /** Initializes event data with the supplied cancellation state. */
        explicit CancelEventArgs(bool cancel) : cancel_(cancel) {}

        /** Gets whether the operation should be cancelled. */
        [[nodiscard]] bool getCancelProperty() const noexcept { return cancel_; }

        /** Sets whether the operation should be cancelled. */
        void setCancelProperty(bool value) noexcept { cancel_ = value; }
    };

    /** @brief Represents the method that will handle the event raised when canceling an event. */
    using CancelEventHandler = std::function<void(void*, CancelEventArgs&)>;
}
