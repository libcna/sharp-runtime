// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include "System/IProgress.hpp"

namespace System {

    template<typename T>
    class Progress : public IProgress<T> {
        std::function<void(T)> handler_;
    public:
        Progress() = default;
        explicit Progress(std::function<void(T)> handler) : handler_(std::move(handler)) {}

        void Report(const T& value) override {
            if (handler_) handler_(value);
        }

        // Attach/detach handlers (simplified — .NET uses SynchronizationContext).
        void addProgressChangedHandler(std::function<void(T)> handler) {
            auto old = handler_;
            handler_ = [old, handler](const T& v) {
                if (old) old(v);
                handler(v);
            };
        }
    };

} // namespace System
