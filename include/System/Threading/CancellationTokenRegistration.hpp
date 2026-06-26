// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <memory>
#include "System/IDisposable.hpp"

namespace System::Threading {

    /** Represents a callback registration with a CancellationToken that can be deregistered. */
    class CancellationTokenRegistration : public System::IDisposable {
        std::shared_ptr<bool> active_;

    public:
        /** Constructs an active CancellationTokenRegistration. */
        CancellationTokenRegistration() : active_(std::make_shared<bool>(true)) {}

        /** Deregisters the callback associated with this registration. */
        void Dispose() override {
            if (active_) *active_ = false;
        }

        /** Attempts to deregister the associated callback; equivalent to Dispose. */
        void Unregister() { Dispose(); }

        /** Returns true if the registration has not been disposed. */
        [[nodiscard]] bool getIsActiveProperty() const {
            return active_ && *active_;
        }
    };

} // namespace System::Threading
