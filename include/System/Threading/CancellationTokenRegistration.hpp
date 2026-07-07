// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <mutex>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IDisposable.hpp"
#include "System/Threading/CancellationToken.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /** Represents a callback registration with a CancellationToken that can be deregistered. */
    class CancellationTokenRegistration : public System::IDisposable {
        std::shared_ptr<Detail::CancellationState> state_;
        intcs id_ = -1;

    public:
        /** Constructs an inactive CancellationTokenRegistration (nothing to unregister). */
        CancellationTokenRegistration() = default;
        /** Constructs a registration tracking the callback identified by @p id within @p state. */
        CancellationTokenRegistration(std::shared_ptr<Detail::CancellationState> state, intcs id)
            : state_(std::move(state)), id_(id) {}

        /** Deregisters the callback associated with this registration. */
        void Dispose() override {
            if (!state_) return;
            {
                std::lock_guard<std::mutex> lock(state_->mutex);
                state_->callbacks.erase(id_);
            }
            state_.reset();
        }

        /** Attempts to deregister the associated callback; equivalent to Dispose. */
        void Unregister() { Dispose(); }

        /** Returns true if the registration has not been disposed and its callback is still pending. */
        [[nodiscard]] bool getIsActiveProperty() const {
            if (!state_) return false;
            std::lock_guard<std::mutex> lock(state_->mutex);
            return state_->callbacks.count(id_) != 0;
        }
    };

} // namespace System::Threading
