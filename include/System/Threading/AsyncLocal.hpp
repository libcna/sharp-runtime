// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>

#include "System/Threading/AsyncLocalValueChangedArgs.hpp"

namespace System::Threading {

    /** Represents ambient data that is local to a given asynchronous control flow; backed by thread-local storage in C++. */
    template<typename T>
    class AsyncLocal {
        static thread_local T value_;
        std::function<void(const AsyncLocalValueChangedArgs<T>&)> valueChangedHandler_;

    public:
        /** Constructs an AsyncLocal without a value-changed handler. */
        AsyncLocal() = default;
        /** Constructs an AsyncLocal that invokes handler whenever the value changes. */
        explicit AsyncLocal(std::function<void(const AsyncLocalValueChangedArgs<T>&)> handler)
            : valueChangedHandler_(std::move(handler)) {}

        /** Returns the current ambient value for the executing thread. */
        [[nodiscard]] const T& getValueProperty() const { return value_; }

        /** Sets the ambient value for the executing thread. */
        void setValueProperty(const T& v) {
            if (valueChangedHandler_) valueChangedHandler_(AsyncLocalValueChangedArgs<T>(value_, v, false));
            value_ = v;
        }
    };

    template<typename T>
    thread_local T AsyncLocal<T>::value_{};

} // namespace System::Threading
