// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Threading/CancellationToken.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/OperationCanceledException.hpp"
#include "System/Threading/CancellationTokenRegistration.hpp"
namespace System::Threading {
    void CancellationToken::ThrowIfCancellationRequested() const {
        if (state_->cancelled.load())
            throw System::OperationCanceledException();
    }

    CancellationTokenRegistration CancellationToken::Register(std::function<void()> callback) {
        // .NET's CancellationToken.Register(Action callback) forwards
        // `callback ?? throw new ArgumentNullException(nameof(callback))`, so the null
        // delegate is rejected before the token's state is consulted -- including when the
        // token is already cancelled, where the callback would otherwise run immediately.
        // This port used to store the empty function and let Cancel() skip it (and skip it
        // again on the already-cancelled fast path below), turning an invalid registration
        // into a silent no-op the caller could not detect (SR-AUD-198). Ticket #1951 /
        // CCF-011; see docs/EmptyCallableBoundaryPlan.md. The check precedes the lock, so
        // no id is allocated and nothing is recorded when it fires.
        if (!callback) throw System::ArgumentNullException("callback");
        // The cancelled-check and callback-map mutation share state_->mutex with Cancel()'s
        // equivalent critical section, so there is no TOCTOU window where a registration could
        // be lost because Cancel() ran between an unlocked check and the map insert.
        bool alreadyCancelled = false;
        intcs id = -1;
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if (state_->cancelled.load()) {
                alreadyCancelled = true;
            } else {
                id = state_->nextId++;
                state_->callbacks[id] = callback;
            }
        }
        if (alreadyCancelled) {
            if (callback) callback();
            return CancellationTokenRegistration();
        }
        return CancellationTokenRegistration(state_, id);
    }
} // namespace System::Threading
