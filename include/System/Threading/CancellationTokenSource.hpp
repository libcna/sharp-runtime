// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include <memory>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/AggregateException.hpp"
#include "System/ObjectDisposedException.hpp"
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

        void ThrowIfDisposed() const {
            if (disposed_) throw System::ObjectDisposedException("CancellationTokenSource", "The CancellationTokenSource has been disposed.");
        }

    public:
        /** Initializes a new CancellationTokenSource. */
        CancellationTokenSource() = default;

        /**
         * @brief Returns the CancellationToken associated with this source.
         * @throws System::ObjectDisposedException if this source has been disposed.
         */
        [[nodiscard]] CancellationToken getTokenProperty() const {
            ThrowIfDisposed();
            return CancellationToken(state_);
        }

        /** Returns true if cancellation has been requested. */
        [[nodiscard]] bool getIsCancellationRequestedProperty() const { return state_->cancelled.load(); }

        /**
         * @brief Signals cancellation to all linked CancellationToken holders and runs their registered callbacks.
         * @throws System::ObjectDisposedException if this source has been disposed.
         * @throws System::AggregateException if one or more callbacks threw; every callback still
         * runs regardless (verified against CancellationTokenSource.cs's ExecuteCallbackHandlers).
         */
        void Cancel() {
            ThrowIfDisposed();
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
            std::vector<std::exception_ptr> exceptions;
            for (auto& callback : callbacksToRun) {
                if (!callback) continue;
                try {
                    callback();
                } catch (...) {
                    exceptions.push_back(std::current_exception());
                }
            }
            if (!exceptions.empty()) throw System::AggregateException(std::move(exceptions));
        }

        /** Releases resources used by this CancellationTokenSource. */
        void Dispose() { disposed_ = true; }
    };

} // namespace System::Threading
