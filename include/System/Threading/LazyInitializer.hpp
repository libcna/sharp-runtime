// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <mutex>

namespace System::Threading {

    /// Provides lazy-initialization routines.
    class LazyInitializer {
    public:
        /// Prevents instantiation — all members are static.
        LazyInitializer() = delete;

        /// Initializes target using its default constructor if it is null; returns the initialized value.
        template<typename T>
        static T& EnsureInitialized(T*& target) {
            if (!target) {
                static std::mutex mtx;
                std::lock_guard<std::mutex> lock(mtx);
                if (!target) target = new T();
            }
            return *target;
        }

        /// Initializes target using valueFactory if it is null; returns the initialized value.
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
