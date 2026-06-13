// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <array>
#include <charconv>
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
        /// Deleted constructor; all members are static.
        Console() = delete;

        /// The newline string used by the current environment.
        static inline const std::string NewLine =
#ifdef _WIN32
            "\r\n";
#else
            "\n";
#endif

        // --- Write ---
        /// Writes the specified string to the standard output stream.
        static void Write(const std::string& value) { std::cout << value; }
        /// Writes the specified C-string to the standard output stream.
        static void Write(const char* value)         { std::cout << value; }
        /// Writes the specified character to the standard output stream.
        static void Write(char value)                { std::cout << value; }
        /// Writes the specified integer to the standard output stream.
        static void Write(intcs value)               { std::cout << value; }
        /// Writes the specified 64-bit integer to the standard output stream.
        static void Write(longcs value)              { std::cout << value; }
        /// Writes the specified double to the standard output stream.
        static void Write(double value) {
            std::array<char, 64> buf;
            auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
            if (ec == std::errc{}) std::cout.write(buf.data(), ptr - buf.data());
            else std::cout << value;
        }
        /// Writes the specified float to the standard output stream.
        static void Write(float value) {
            std::array<char, 32> buf;
            auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
            if (ec == std::errc{}) std::cout.write(buf.data(), ptr - buf.data());
            else std::cout << value;
        }
        /// Writes the specified Boolean to the standard output stream.
        static void Write(bool value)                { std::cout << (value ? "True" : "False"); }

        // --- WriteLine ---
        /// Writes the current line terminator to the standard output stream.
        static void WriteLine()                      { std::cout << NewLine; }
        /// Writes the specified string followed by a line terminator.
        static void WriteLine(const std::string& v)  { std::cout << v << NewLine; }
        /// Writes the specified C-string followed by a line terminator.
        static void WriteLine(const char* v)         { std::cout << v << NewLine; }
        /// Writes the specified character followed by a line terminator.
        static void WriteLine(char v)                { std::cout << v << NewLine; }
        /// Writes the specified integer followed by a line terminator.
        static void WriteLine(intcs v)               { std::cout << v << NewLine; }
        /// Writes the specified 64-bit integer followed by a line terminator.
        static void WriteLine(longcs v)              { std::cout << v << NewLine; }
        /// Writes the specified double followed by a line terminator.
        static void WriteLine(double v) { Write(v); std::cout << NewLine; }
        /// Writes the specified float followed by a line terminator.
        static void WriteLine(float v)  { Write(v); std::cout << NewLine; }
        /// Writes the specified Boolean followed by a line terminator.
        static void WriteLine(bool v)                { std::cout << (v ? "True" : "False") << NewLine; }

        // --- Error ---
        /// Writes the specified string to the standard error stream.
        static void Error_Write(const std::string& v)      { std::cerr << v; }
        /// Writes the specified string followed by a line terminator to the standard error stream.
        static void Error_WriteLine(const std::string& v)  { std::cerr << v << NewLine; }

        // --- Read ---
        /// Reads the next line of characters from the standard input stream.
        [[nodiscard]] static std::string ReadLine() {
            std::string line;
            std::getline(std::cin, line);
            return line;
        }
    };

} // namespace System
