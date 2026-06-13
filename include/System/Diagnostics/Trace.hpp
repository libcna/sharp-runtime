// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <iostream>
#include <string>

namespace System::Diagnostics {

    /// @brief Provides a set of methods that help you trace code execution.
    ///
    /// Unlike Debug, Trace output is NOT stripped in release builds.
    /// Partial C++ counterpart of .NET System.Diagnostics.Trace.
    ///
    /// @note Status: Partial — writes to std::cerr; no TraceListeners.
    class Trace {
    public:
        /// @brief Not instantiable — all members are static.
        Trace() = delete;

        /// @brief Writes @p message to stderr without a trailing newline.
        static void Write(const std::string& message)   { std::cerr << message; }
        /// @brief Writes @p message to stderr followed by a newline.
        static void WriteLine(const std::string& message) { std::cerr << message << '\n'; }
        /// @brief Writes a blank line to stderr.
        static void WriteLine()                         { std::cerr << '\n'; }

        /// @brief Writes an informational @p message prefixed with [Info].
        static void TraceInformation(const std::string& message) {
            std::cerr << "[Info]  " << message << '\n';
        }
        /// @brief Writes a warning @p message prefixed with [Warn].
        static void TraceWarning(const std::string& message) {
            std::cerr << "[Warn]  " << message << '\n';
        }
        /// @brief Writes an error @p message prefixed with [Error].
        static void TraceError(const std::string& message) {
            std::cerr << "[Error] " << message << '\n';
        }

        /// @brief Writes to stderr if @p condition is false.
        /// @param condition The expression that is expected to be true.
        /// @param message   Optional message appended to the failure line.
        static void Assert(bool condition, const std::string& message = "") {
            if (!condition) {
                std::cerr << "[Trace Assert Failed]";
                if (!message.empty()) std::cerr << ' ' << message;
                std::cerr << '\n';
            }
        }

        /// @brief Writes a failure @p message prefixed with [Trace Fail].
        static void Fail(const std::string& message) {
            std::cerr << "[Trace Fail] " << message << '\n';
        }

        /// @brief Flushes the stderr buffer.
        static void Flush() { std::cerr.flush(); }
    };

} // namespace System::Diagnostics
