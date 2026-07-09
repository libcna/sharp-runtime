// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <unordered_map>

#include "System/Threading/AsyncLocalValueChangedArgs.hpp"

namespace System::Threading {

    /** Represents ambient data that is local to a given asynchronous control flow; backed by thread-local storage in C++. */
    template<typename T>
    class AsyncLocal {
        static std::unordered_map<const AsyncLocal*, T>& storageMap() {
            static thread_local std::unordered_map<const AsyncLocal*, T> map;
            return map;
        }

        std::function<void(const AsyncLocalValueChangedArgs<T>&)> valueChangedHandler_;

    public:
        /** Constructs an AsyncLocal without a value-changed handler. */
        AsyncLocal() = default;
        /** Constructs an AsyncLocal that invokes handler whenever the value changes. */
        explicit AsyncLocal(std::function<void(const AsyncLocalValueChangedArgs<T>&)> handler)
            : valueChangedHandler_(std::move(handler)) {}

        /** Destroys this instance's slot in the current thread's storage. */
        ~AsyncLocal() { storageMap().erase(this); }

        /** Returns the current ambient value for the executing thread. */
        [[nodiscard]] const T& getValueProperty() const {
            static const T defaultValue{};
            auto& map = storageMap();
            auto it = map.find(this);
            return it == map.end() ? defaultValue : it->second;
        }

        /**
         * @brief Sets the ambient value for the executing thread.
         *
         * C++ counterpart of .NET AsyncLocal<T>.Value setter, which is a complete no-op
         * (no mutation, no change notification) when the new value equals the previous one
         * (ExecutionContext.SetLocalValue: `if (previousValue == newValue) return;`).
         */
        void setValueProperty(const T& v) {
            auto& map = storageMap();
            auto it = map.find(this);
            T previous = (it == map.end()) ? T{} : it->second;
            if (previous == v) return;
            if (valueChangedHandler_) valueChangedHandler_(AsyncLocalValueChangedArgs<T>(previous, v, false));
            map[this] = v;
        }
    };

} // namespace System::Threading
