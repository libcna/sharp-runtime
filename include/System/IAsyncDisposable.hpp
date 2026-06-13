// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    /// Provides a mechanism for releasing unmanaged resources asynchronously.
    class IAsyncDisposable {
    public:
        /// Virtual destructor for safe polymorphic destruction.
        virtual ~IAsyncDisposable() = default;
        /// Releases resources asynchronously (approximated synchronously in this C++ port).
        virtual void DisposeAsync() = 0;
    };

} // namespace System
