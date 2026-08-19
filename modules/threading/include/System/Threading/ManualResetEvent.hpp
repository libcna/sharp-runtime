// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Threading/EventResetMode.hpp"
#include "System/Threading/EventWaitHandle.hpp"

namespace System::Threading {

    /**
     * @brief Represents a thread synchronization event that must be reset manually.
     *
     * C++ counterpart of .NET `System.Threading.ManualResetEvent`, which is
     * `public sealed class ManualResetEvent : EventWaitHandle` whose entire body is
     * `public ManualResetEvent(bool initialState) : base(initialState, EventResetMode.ManualReset)`
     * (`ManualResetEvent.cs:6-9`). It declares **no members of its own**.
     *
     * @note **Derived since ticket #1958 / SR-AUD-209 (2026-08-19)**; see AutoResetEvent for the
     * full account. `WaitOne()` now returns `bool` rather than `void`, `sizeof` grows and a
     * vtable appears, so consumers must rebuild.
     */
    class ManualResetEvent final : public EventWaitHandle {
    public:
        /**
         * @param initialState If true, the event starts in the signaled state.
         *
         * The parameter has no default, as .NET's has none; the default this port used to
         * publish had zero measured call sites.
         */
        explicit ManualResetEvent(bool initialState)
            : EventWaitHandle(initialState, EventResetMode::ManualReset) {}
    };

} // namespace System::Threading
