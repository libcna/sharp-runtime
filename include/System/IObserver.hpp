// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>

namespace System {

    /**
     * @brief Provides a mechanism for receiving push-based notifications.
     *
     * C++ counterpart of .NET System.IObserver<T>.
     * @tparam T The object that provides notification information.
     */
    template<typename T>
    class IObserver {
    public:
        virtual ~IObserver() = default;

        /** @brief Provides the observer with new data. */
        virtual void OnNext(const T& value) = 0;

        /** @brief Notifies the observer that the provider has experienced an error condition. */
        virtual void OnError(const std::exception& error) = 0;

        /** @brief Notifies the observer that the provider has finished sending push-based notifications. */
        virtual void OnCompleted() = 0;
    };

} // namespace System
