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
        Debug() = delete;

#ifdef NDEBUG
        static void Assert(bool condition)                            {}
        static void Assert(bool condition, const char*)               {}
        static void Assert(bool condition, const std::string&)        {}
        static void Write(const char*)                                {}
        static void Write(const std::string&)                         {}
        static void WriteLine(const char*)                            {}
        static void WriteLine(const std::string&)                     {}
        static void WriteLine()                                       {}
        static void Fail(const char*)                                 {}
        static void Fail(const std::string&)                          {}
#else
        static void Assert(bool condition) {
            assert(condition);
        }
        static void Assert(bool condition, const char* message) {
            if (!condition) {
                std::cerr << "Debug::Assert failed: " << message << '\n';
                assert(false);
            }
        }
        static void Assert(bool condition, const std::string& message) {
            Assert(condition, message.c_str());
        }

        static void Write(const char* message)        { std::cout << message; }
        static void Write(const std::string& message) { std::cout << message; }

        static void WriteLine()                        { std::cout << '\n'; }
        static void WriteLine(const char* message)    { std::cout << message << '\n'; }
        static void WriteLine(const std::string& msg) { std::cout << msg    << '\n'; }

        static void Fail(const char* message) {
            std::cerr << "Debug::Fail: " << message << '\n';
            assert(false);
        }
        static void Fail(const std::string& message) { Fail(message.c_str()); }
#endif
    };

} // namespace System::Diagnostics
