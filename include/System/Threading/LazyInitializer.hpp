// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <mutex>

namespace System::Threading {

    class LazyInitializer {
    public:
        LazyInitializer() = delete;

        // Thread-safe lazy initialization using double-checked locking.
        template<typename T>
        static T& EnsureInitialized(T*& target) {
            if (!target) {
                static std::mutex mtx;
                std::lock_guard<std::mutex> lock(mtx);
                if (!target) target = new T();
            }
            return *target;
        }

        template<typename T>
        static T& EnsureInitialized(T*& target, std::function<T*()> valueFactory) {
            if (!target) {
                static std::mutex mtx;
                std::lock_guard<std::mutex> lock(mtx);
                if (!target) target = valueFactory();
            }
            return *target;
        }
    };

} // namespace System::Threading
