// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Threading {

    enum class ThreadState {
        Running         = 0,
        StopRequested   = 1,
        SuspendRequested = 2,
        Background      = 4,
        Unstarted       = 8,
        Stopped         = 16,
        WaitSleepJoin   = 32,
        Suspended       = 64,
        AbortRequested  = 128,
        Aborted         = 256,
    };

    inline ThreadState operator|(ThreadState a, ThreadState b) {
        return static_cast<ThreadState>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline ThreadState operator&(ThreadState a, ThreadState b) {
        return static_cast<ThreadState>(static_cast<int>(a) & static_cast<int>(b));
    }

} // namespace System::Threading
