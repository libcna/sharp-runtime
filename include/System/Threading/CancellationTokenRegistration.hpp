// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <memory>
#include "System/IDisposable.hpp"

namespace System::Threading {

    // Represents a callback registered with a CancellationToken.
    class CancellationTokenRegistration : public System::IDisposable {
        std::shared_ptr<bool> active_;

    public:
        CancellationTokenRegistration() : active_(std::make_shared<bool>(true)) {}

        void Dispose() override {
            if (active_) *active_ = false;
        }

        void Unregister() { Dispose(); }

        [[nodiscard]] bool getIsActiveProperty() const {
            return active_ && *active_;
        }
    };

} // namespace System::Threading
