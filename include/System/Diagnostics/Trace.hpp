// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <iostream>
#include <string>

namespace System::Diagnostics {

    /**
     * @brief Provides a set of methods that help you trace code execution.
     *
     * Unlike Debug, Trace output is NOT stripped in release builds.
     * Partial C++ counterpart of .NET System.Diagnostics.Trace.
     *
     * @note Status: Partial — writes to std::cerr; no TraceListeners.
     */
    class Trace {
    public:
        Trace() = delete;

        static void Write(const std::string& message)   { std::cerr << message; }
        static void WriteLine(const std::string& message) { std::cerr << message << '\n'; }
        static void WriteLine()                         { std::cerr << '\n'; }

        static void TraceInformation(const std::string& message) {
            std::cerr << "[Info]  " << message << '\n';
        }
        static void TraceWarning(const std::string& message) {
            std::cerr << "[Warn]  " << message << '\n';
        }
        static void TraceError(const std::string& message) {
            std::cerr << "[Error] " << message << '\n';
        }

        static void Assert(bool condition, const std::string& message = "") {
            if (!condition) {
                std::cerr << "[Trace Assert Failed]";
                if (!message.empty()) std::cerr << ' ' << message;
                std::cerr << '\n';
            }
        }

        static void Fail(const std::string& message) {
            std::cerr << "[Trace Fail] " << message << '\n';
        }

        static void Flush() { std::cerr.flush(); }
    };

} // namespace System::Diagnostics
