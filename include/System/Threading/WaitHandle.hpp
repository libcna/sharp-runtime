// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IDisposable.hpp"

namespace System::Threading {

    /** Abstract base class for OS synchronisation handles. */
    class WaitHandle : public System::IDisposable {
    public:
        /** Return value from a timed wait that expired before the handle was signalled. */
        static constexpr int WaitTimeout = 258;   // WAIT_TIMEOUT on Windows
        /** Sentinel for an invalid native handle. */
        static constexpr int InvalidHandle = -1;

        /** Destroys the WaitHandle. */
        virtual ~WaitHandle() = default;

        /** Blocks the current thread until the handle is signalled. */
        virtual bool WaitOne() = 0;
        /** Blocks the current thread until the handle is signalled or millisecondsTimeout elapses. */
        virtual bool WaitOne(int millisecondsTimeout) = 0;

        /** Releases resources held by this WaitHandle. */
        void Dispose() override {}

        /** Closes the WaitHandle by calling Dispose. */
        void Close() { Dispose(); }
    };

} // namespace System::Threading
