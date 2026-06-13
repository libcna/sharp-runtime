// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <csignal>
#include <string>

namespace System::Diagnostics {

    /// @brief Enables communication with a debugger.
    ///
    /// Partial C++ counterpart of .NET System.Diagnostics.Debugger.
    class Debugger {
    public:
        /// @brief Not instantiable — all members are static.
        Debugger() = delete;

        /// @return true if a debugger is attached to the process; always false in this implementation.
        [[nodiscard]] static bool getIsAttachedProperty() noexcept {
#if defined(__has_include) && __has_include(<sys/ptrace.h>)
            // On Linux: if traced by a debugger, ptrace returns -1
            return false; // conservative: assume not attached
#else
            return false;
#endif
        }

        /// @brief Signals the debugger to break (triggers a debug trap).
        ///
        /// Uses `__debugbreak()` on MSVC, `__builtin_trap()` on GCC/Clang,
        /// or SIGTRAP elsewhere.
        static void Break() noexcept {
#if defined(_MSC_VER)
            __debugbreak();
#elif defined(__GNUC__) || defined(__clang__)
            __builtin_trap();
#else
            std::raise(SIGTRAP);
#endif
        }

        /// @brief Attempts to launch a debugger and attach it to the process.
        /// @return Always false — launching is not supported in this implementation.
        static bool Launch() noexcept { return false; }

        /// @brief Sends a log message to the attached debugger; no-op if none is attached.
        /// @param level    Severity level (ignored in this implementation).
        /// @param category Log category (ignored in this implementation).
        /// @param message  Message text (ignored in this implementation).
        static void Log(int /*level*/, const std::string& /*category*/, const std::string& /*message*/) {}

        /// @return false — logging to the debugger is not supported in this implementation.
        static bool IsLogging() noexcept { return false; }
    };

} // namespace System::Diagnostics
