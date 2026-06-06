// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    template<typename T>
    class IObserver {
    public:
        virtual ~IObserver() = default;
        virtual void OnNext(const T& value) = 0;
        virtual void OnError(const std::exception& error) = 0;
        virtual void OnCompleted() = 0;
    };

} // namespace System
