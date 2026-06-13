// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>

namespace System::Threading {

    /// A delegate type for sending or posting callbacks to a SynchronizationContext.
    using SendOrPostCallback = std::function<void(void*)>;

    /// Provides the basic functionality required to propagate a synchronisation context in various synchronisation models.
    class SynchronizationContext {
    public:
        /// Destroys the SynchronizationContext.
        virtual ~SynchronizationContext() = default;

        /// Dispatches an asynchronous message to the context (runs synchronously in this stub).
        virtual void Post(SendOrPostCallback d, void* state) {
            if (d) d(state);
        }

        /// Dispatches a synchronous message to the context.
        virtual void Send(SendOrPostCallback d, void* state) {
            if (d) d(state);
        }

        /// Returns the synchronization context for the current thread (always nullptr in this stub).
        static SynchronizationContext* getCurrent() { return nullptr; }

        /// Sets the synchronization context for the current thread (no-op in this stub).
        static void SetSynchronizationContext(SynchronizationContext* /*syncContext*/) {}
    };

} // namespace System::Threading
