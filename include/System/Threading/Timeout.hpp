// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Threading {

    class Timeout {
    public:
        Timeout() = delete;

        static constexpr int Infinite = -1;
        static constexpr long long InfiniteTimeSpan = -10000LL; // -1 ms in TimeSpan ticks
    };

} // namespace System::Threading
