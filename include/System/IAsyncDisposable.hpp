// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    /**
     * @brief Provides a mechanism for releasing unmanaged resources asynchronously.
     *
     * C++ counterpart of .NET System.IAsyncDisposable.
     * In this port DisposeAsync() runs synchronously — the async aspect is not modelled.
     */
    class IAsyncDisposable {
    public:
        /** @brief Virtual destructor for safe polymorphic destruction. */
        virtual ~IAsyncDisposable() = default;
        /** @brief Releases resources asynchronously (approximated synchronously in this C++ port). */
        virtual void DisposeAsync() = 0;
    };

} // namespace System
