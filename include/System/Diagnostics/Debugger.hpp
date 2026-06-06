// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <csignal>
#include <string>

namespace System::Diagnostics {

    class Debugger {
    public:
        Debugger() = delete;

        [[nodiscard]] static bool getIsAttachedProperty() noexcept {
#if defined(__has_include) && __has_include(<sys/ptrace.h>)
            // On Linux: if traced by a debugger, ptrace returns -1
            return false; // conservative: assume not attached
#else
            return false;
#endif
        }

        static void Break() noexcept {
#if defined(_MSC_VER)
            __debugbreak();
#elif defined(__GNUC__) || defined(__clang__)
            __builtin_trap();
#else
            std::raise(SIGTRAP);
#endif
        }

        static bool Launch() noexcept { return false; }

        static void Log(int /*level*/, const std::string& /*category*/, const std::string& /*message*/) {}

        static bool IsLogging() noexcept { return false; }
    };

} // namespace System::Diagnostics
