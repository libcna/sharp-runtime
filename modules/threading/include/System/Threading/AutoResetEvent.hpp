// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Threading/EventResetMode.hpp"
#include "System/Threading/EventWaitHandle.hpp"

namespace System::Threading {

    /**
     * @brief Represents a thread synchronization event that resets automatically after
     *        releasing a single waiting thread.
     *
     * C++ counterpart of .NET `System.Threading.AutoResetEvent`, which is
     * `public sealed class AutoResetEvent : EventWaitHandle` whose entire body is
     * `public AutoResetEvent(bool initialState) : base(initialState, EventResetMode.AutoReset)`
     * (`AutoResetEvent.cs:6-9`). It declares **no members of its own**; `Set`, `Reset`,
     * `WaitOne`, `Close` and `Dispose` are all inherited.
     *
     * @note **Derived since ticket #1958 / SR-AUD-209 (2026-08-19).** This class used to have
     * no base and no vtable, holding its own mutex, condition variable and signalled flag --
     * a third copy of logic `EventWaitHandle` already had. Because it was not a `WaitHandle`,
     * `WaitHandle::WaitAll`/`WaitAny` **could not accept it at all**, which is what made
     * SR-AUD-209 the one finding in the namespace that left a documented API unusable.
     *
     * `WaitOne()` consequently returns `bool` rather than `void`. Nothing that compiled stops
     * compiling -- ignoring a returned value is legal -- so the change is a widening at every
     * call site, but `sizeof` grows and a vtable appears, so consumers must rebuild.
     */
    class AutoResetEvent final : public EventWaitHandle {
    public:
        /**
         * @param initialState If true, the event starts in the signaled state.
         *
         * The parameter has no default, as .NET's has none. The default this port used to
         * publish had **zero** call sites, measured across `modules/`, `test/` and both
         * downstream consumers, so removing it migrates nothing.
         */
        explicit AutoResetEvent(bool initialState)
            : EventWaitHandle(initialState, EventResetMode::AutoReset) {}
    };

} // namespace System::Threading
