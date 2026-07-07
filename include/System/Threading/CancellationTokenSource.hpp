// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Threading/CancellationToken.hpp"

namespace System::Threading {

    /**
     * @brief Signals to a CancellationToken that it should be cancelled.
     *
     * Partial C++ counterpart of .NET System.Threading.CancellationTokenSource.
     *
     * @note Status: Partial — no timer-based (delayed) cancellation, no linked token sources.
     */
    class CancellationTokenSource {
        std::shared_ptr<Detail::CancellationState> state_ = std::make_shared<Detail::CancellationState>();
        bool disposed_ = false;
    public:
        /** Initializes a new CancellationTokenSource. */
        CancellationTokenSource() = default;

        /** Returns the CancellationToken associated with this source. */
        [[nodiscard]] CancellationToken getTokenProperty() const {
            return CancellationToken(state_);
        }

        /** Returns true if cancellation has been requested. */
        [[nodiscard]] bool getIsCancellationRequestedProperty() const { return state_->cancelled.load(); }

        /** Signals cancellation to all linked CancellationToken holders and runs their registered callbacks. */
        void Cancel() {
            std::vector<std::function<void()>> callbacksToRun;
            {
                // Checking-and-setting `cancelled` under the same lock used by Register() closes
                // the TOCTOU window where a registration could otherwise be added after Cancel()
                // already decided there was nothing to run.
                std::lock_guard<std::mutex> lock(state_->mutex);
                if (state_->cancelled.exchange(true)) return;
                callbacksToRun.reserve(state_->callbacks.size());
                for (auto& [id, callback] : state_->callbacks) callbacksToRun.push_back(callback);
                state_->callbacks.clear();
            }
            for (auto& callback : callbacksToRun)
                if (callback) callback();
        }

        /** Releases resources used by this CancellationTokenSource. */
        void Dispose() { disposed_ = true; }
    };

} // namespace System::Threading
