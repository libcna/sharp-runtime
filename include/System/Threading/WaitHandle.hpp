// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IDisposable.hpp"

namespace System::Threading {

    // Abstract base class for OS synchronization handles.
    class WaitHandle : public System::IDisposable {
    public:
        static constexpr int WaitTimeout = 258;   // WAIT_TIMEOUT on Windows
        static constexpr int InvalidHandle = -1;

        virtual ~WaitHandle() = default;

        virtual bool WaitOne() = 0;
        virtual bool WaitOne(int millisecondsTimeout) = 0;

        // WaitAll / WaitAny — simplified; subclasses override as needed.
        void Dispose() override {}

        void Close() { Dispose(); }
    };

} // namespace System::Threading
