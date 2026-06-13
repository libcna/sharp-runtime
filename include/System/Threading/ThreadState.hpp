// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Threading {

    /// Specifies the execution states of a Thread.
    enum class ThreadState {
        /// The thread is running.
        Running         = 0,
        /// The thread is being requested to stop.
        StopRequested   = 1,
        /// The thread is being requested to suspend.
        SuspendRequested = 2,
        /// The thread is executing as a background thread.
        Background      = 4,
        /// The thread has not been started.
        Unstarted       = 8,
        /// The thread has stopped.
        Stopped         = 16,
        /// The thread is blocked in a Wait, Sleep, or Join call.
        WaitSleepJoin   = 32,
        /// The thread has been suspended.
        Suspended       = 64,
        /// An abort has been requested.
        AbortRequested  = 128,
        /// The thread has been aborted.
        Aborted         = 256,
    };

    /// Returns the bitwise OR of two ThreadState values.
    inline ThreadState operator|(ThreadState a, ThreadState b) {
        return static_cast<ThreadState>(static_cast<int>(a) | static_cast<int>(b));
    }
    /// Returns the bitwise AND of two ThreadState values.
    inline ThreadState operator&(ThreadState a, ThreadState b) {
        return static_cast<ThreadState>(static_cast<int>(a) & static_cast<int>(b));
    }

} // namespace System::Threading
