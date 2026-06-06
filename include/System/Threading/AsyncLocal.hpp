// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>

namespace System::Threading {

    // Ambient data local to a given asynchronous control flow.
    // In C++ without async/await this reduces to thread-local storage.
    template<typename T>
    class AsyncLocal {
        mutable thread_local static T value_;
        std::function<void(T, T, bool)> valueChangedHandler_;

    public:
        AsyncLocal() = default;
        explicit AsyncLocal(std::function<void(T /*previousValue*/, T /*currentValue*/, bool /*contextChanged*/)> handler)
            : valueChangedHandler_(std::move(handler)) {}

        [[nodiscard]] const T& getValueProperty() const { return value_; }

        void setValueProperty(const T& v) {
            if (valueChangedHandler_) valueChangedHandler_(value_, v, false);
            value_ = v;
        }
    };

    template<typename T>
    thread_local T AsyncLocal<T>::value_{};

} // namespace System::Threading
