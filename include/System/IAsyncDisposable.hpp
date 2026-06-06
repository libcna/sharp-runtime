// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    class IAsyncDisposable {
    public:
        virtual ~IAsyncDisposable() = default;
        // In C++ we approximate DisposeAsync() with a synchronous Dispose since we have no async/await.
        virtual void DisposeAsync() = 0;
    };

} // namespace System
