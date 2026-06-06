// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>

namespace System::Threading {

    using SendOrPostCallback = std::function<void(void*)>;

    // Minimal stub: runs callbacks synchronously on the calling thread.
    class SynchronizationContext {
    public:
        virtual ~SynchronizationContext() = default;

        virtual void Post(SendOrPostCallback d, void* state) {
            if (d) d(state);
        }

        virtual void Send(SendOrPostCallback d, void* state) {
            if (d) d(state);
        }

        static SynchronizationContext* getCurrent() { return nullptr; }

        static void SetSynchronizationContext(SynchronizationContext* /*syncContext*/) {}
    };

} // namespace System::Threading
