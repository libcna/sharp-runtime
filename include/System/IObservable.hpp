// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>

namespace System {

    template<typename T>
    class IObserver;

    template<typename T>
    class IDisposable;

    template<typename T>
    class IObservable {
    public:
        virtual ~IObservable() = default;
        virtual std::shared_ptr<IDisposable<T>> Subscribe(std::shared_ptr<IObserver<T>> observer) = 0;
    };

} // namespace System
