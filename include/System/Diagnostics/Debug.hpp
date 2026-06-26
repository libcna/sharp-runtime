// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cassert>
#include <iostream>
#include <string>

namespace System::Diagnostics {

    /**
     * @brief Provides a set of methods and properties that help debug code.
     * 
     * Partial C++ counterpart of .NET System.Diagnostics.Debug.
     * In Release builds the Assert/Write/WriteLine methods are compiled out.
     * 
     * @note Status: Partial
     */
    class Debug {
    public:
        /** @brief Not instantiable — all members are static. */
        Debug() = delete;

#ifdef NDEBUG
        /** @brief No-op in release builds. */
        static void Assert(bool condition)                            {}
        /** @brief No-op in release builds. */
        static void Assert(bool condition, const char*)               {}
        /** @brief No-op in release builds. */
        static void Assert(bool condition, const std::string&)        {}
        /** @brief No-op in release builds. */
        static void Write(const char*)                                {}
        /** @brief No-op in release builds. */
        static void Write(const std::string&)                         {}
        /** @brief No-op in release builds. */
        static void WriteLine(const char*)                            {}
        /** @brief No-op in release builds. */
        static void WriteLine(const std::string&)                     {}
        /** @brief No-op in release builds. */
        static void WriteLine()                                       {}
        /** @brief No-op in release builds. */
        static void Fail(const char*)                                 {}
        /** @brief No-op in release builds. */
        static void Fail(const std::string&)                          {}
#else
        /**
         * @brief Checks @p condition and aborts if false.
         * @param condition The expression that is expected to be true.
         */
        static void Assert(bool condition) {
            assert(condition);
        }
        /**
         * @brief Checks @p condition; prints @p message to stderr and aborts if false.
         * @param condition The expression that is expected to be true.
         * @param message   Diagnostic message emitted on failure.
         */
        static void Assert(bool condition, const char* message) {
            if (!condition) {
                std::cerr << "Debug::Assert failed: " << message << '\n';
                assert(false);
            }
        }
        /**
         * @brief Checks @p condition; prints @p message to stderr and aborts if false.
         * @param condition The expression that is expected to be true.
         * @param message   Diagnostic message emitted on failure.
         */
        static void Assert(bool condition, const std::string& message) {
            Assert(condition, message.c_str());
        }

        /** @brief Writes @p message to stdout without a trailing newline. */
        static void Write(const char* message)        { std::cout << message; }
        /** @brief Writes @p message to stdout without a trailing newline. */
        static void Write(const std::string& message) { std::cout << message; }

        /** @brief Writes a blank line to stdout. */
        static void WriteLine()                        { std::cout << '\n'; }
        /** @brief Writes @p message followed by a newline to stdout. */
        static void WriteLine(const char* message)    { std::cout << message << '\n'; }
        /** @brief Writes @p msg followed by a newline to stdout. */
        static void WriteLine(const std::string& msg) { std::cout << msg    << '\n'; }

        /**
         * @brief Emits @p message to stderr and aborts unconditionally.
         * @param message Failure description.
         */
        static void Fail(const char* message) {
            std::cerr << "Debug::Fail: " << message << '\n';
            assert(false);
        }
        /** @brief Emits @p message to stderr and aborts unconditionally. */
        static void Fail(const std::string& message) { Fail(message.c_str()); }
#endif
    };

} // namespace System::Diagnostics
