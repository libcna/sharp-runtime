// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /** @brief Internal shared cancellation state; not part of the public API. */
    namespace Detail {
        struct CancellationState {
            std::atomic<bool> cancelled{false};
            std::mutex mutex;
            // Ordered (not unordered_map) so Cancel() can walk callbacks in reverse key order.
            // nextId is monotonically increasing per Register() call, so key order == registration
            // order and reverse key order == LIFO (most-recently-registered fires first), matching
            // CancellationTokenSource.cs's ExecuteCallbackHandlers.
            std::map<intcs, std::function<void()>> callbacks;
            intcs nextId = 0;
        };
    }

    class CancellationTokenRegistration;

    /**
     * @brief Propagates notification that operations should be cancelled.
     *
     * Partial C++ counterpart of .NET System.Threading.CancellationToken.
     *
     * @note Status: Partial
     */
    class CancellationToken {
        std::shared_ptr<Detail::CancellationState> state_;
    public:
        /** Constructs a non-cancelled CancellationToken. */
        CancellationToken() : state_(std::make_shared<Detail::CancellationState>()) {}
        /** Constructs a CancellationToken backed by the given shared state. */
        explicit CancellationToken(std::shared_ptr<Detail::CancellationState> state) : state_(std::move(state)) {}

        /** Returns true if cancellation has been requested. */
        [[nodiscard]] bool getIsCancellationRequestedProperty() const { return state_->cancelled.load(); }

        /** Throws OperationCanceledException if cancellation has been requested. */
        void ThrowIfCancellationRequested() const;

        /**
         * @brief Registers a callback invoked when this token is cancelled.
         *
         * If the token is already cancelled, @p callback runs synchronously before this method
         * returns. Otherwise it runs synchronously on whichever thread calls
         * CancellationTokenSource::Cancel().
         */
        CancellationTokenRegistration Register(std::function<void()> callback);

        /** Returns a CancellationToken that is never in the cancelled state. */
        static const CancellationToken& None() {
            static CancellationToken none;
            return none;
        }

        friend class CancellationTokenSource;
    };

} // namespace System::Threading
