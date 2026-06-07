// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <iostream>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;
    using SharpRuntime::Single;

    /**
     * @brief Represents the standard input, output, and error streams.
     *
     * Partial C++ counterpart of .NET System.Console.
     *
     * @note Status: Partial
     */
    class Console {
    public:
        Console() = delete;

        static inline const std::string NewLine =
#ifdef _WIN32
            "\r\n";
#else
            "\n";
#endif

        // --- Write ---
        static void Write(const std::string& value) { std::cout << value; }
        static void Write(const char* value)         { std::cout << value; }
        static void Write(char value)                { std::cout << value; }
        static void Write(intcs value)               { std::cout << value; }
        static void Write(longcs value)              { std::cout << value; }
        static void Write(double value)              { std::cout << value; }
        static void Write(float value)               { std::cout << value; }
        static void Write(bool value)                { std::cout << (value ? "True" : "False"); }

        // --- WriteLine ---
        static void WriteLine()                      { std::cout << NewLine; }
        static void WriteLine(const std::string& v)  { std::cout << v << NewLine; }
        static void WriteLine(const char* v)         { std::cout << v << NewLine; }
        static void WriteLine(char v)                { std::cout << v << NewLine; }
        static void WriteLine(intcs v)               { std::cout << v << NewLine; }
        static void WriteLine(longcs v)              { std::cout << v << NewLine; }
        static void WriteLine(double v)              { std::cout << v << NewLine; }
        static void WriteLine(float v)               { std::cout << v << NewLine; }
        static void WriteLine(bool v)                { std::cout << (v ? "True" : "False") << NewLine; }

        // --- Error ---
        static void Error_Write(const std::string& v)      { std::cerr << v; }
        static void Error_WriteLine(const std::string& v)  { std::cerr << v << NewLine; }

        // --- Read ---
        [[nodiscard]] static std::string ReadLine() {
            std::string line;
            std::getline(std::cin, line);
            return line;
        }
    };

} // namespace System
