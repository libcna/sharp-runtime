// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include "System/IProgress.hpp"

namespace System {

    /// @brief Progress reporter that invokes a callback when Report() is called.
    ///
    /// Mirrors .NET System.Progress<T>. Invoke Report() to notify all attached handlers.
    template<typename T>
    class Progress : public IProgress<T> {
        std::function<void(T)> handler_;
    public:
        /// Constructs an empty Progress with no handler attached.
        Progress() = default;
        /// @brief Constructs a Progress that invokes @p handler on each Report() call.
        /// @param handler Callback invoked with the reported value.
        explicit Progress(std::function<void(T)> handler) : handler_(std::move(handler)) {}

        /// @brief Reports a progress value, invoking all attached handlers.
        /// @param value The progress value to report.
        void Report(const T& value) override {
            if (handler_) handler_(value);
        }

        /// @brief Attaches an additional handler to be called on each Report().
        ///
        /// Simplified version — .NET uses SynchronizationContext for thread marshalling.
        /// @param handler Additional callback to attach.
        void addProgressChangedHandler(std::function<void(T)> handler) {
            auto old = handler_;
            handler_ = [old, handler](const T& v) {
                if (old) old(v);
                handler(v);
            };
        }
    };

} // namespace System
