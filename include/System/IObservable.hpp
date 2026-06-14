// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include "System/IDisposable.hpp"
#include "System/IObserver.hpp"

namespace System {

    /**
     * @brief Defines a provider for push-based notification.
     *
     * C++ counterpart of .NET System.IObservable<T>.
     * @tparam T The object that provides notification information.
     */
    template<typename T>
    class IObservable {
    public:
        virtual ~IObservable() = default;

        /**
         * @brief Notifies the provider that an observer is to receive notifications.
         * @param observer The object that is to receive notifications.
         * @return A shared_ptr to an IDisposable that allows observers to stop receiving
         *         notifications before the provider has finished sending them.
         */
        virtual std::shared_ptr<IDisposable> Subscribe(std::shared_ptr<IObserver<T>> observer) = 0;
    };

} // namespace System
