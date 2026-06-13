// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    /// Represents the status of an asynchronous operation.
    class IAsyncResult {
    public:
        /// Virtual destructor for safe polymorphic destruction.
        virtual ~IAsyncResult() = default;
        /// Returns true if the asynchronous operation has completed.
        [[nodiscard]] virtual bool getIsCompletedProperty() const = 0;
        /// Returns true if the asynchronous operation completed synchronously.
        [[nodiscard]] virtual bool getCompletedSynchronouslyProperty() const = 0;
    };

} // namespace System
