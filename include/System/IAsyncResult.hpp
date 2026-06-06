// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    class IAsyncResult {
    public:
        virtual ~IAsyncResult() = default;
        [[nodiscard]] virtual bool getIsCompletedProperty() const = 0;
        [[nodiscard]] virtual bool getCompletedSynchronouslyProperty() const = 0;
    };

} // namespace System
