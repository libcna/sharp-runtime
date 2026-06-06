// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    template<typename T>
    class IProgress {
    public:
        virtual ~IProgress() = default;
        virtual void Report(const T& value) = 0;
    };

} // namespace System
