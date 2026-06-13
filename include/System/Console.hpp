// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <array>
#include <charconv>
#include <cstdio>
#include <iostream>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ConsoleColor.hpp"
#include "System/String.hpp"

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

        // --- Write (format) ---
        /// Writes a formatted string with one integer argument.
        static void Write(const std::string& format, intcs arg0)
            { Write(System::String::Format(format, arg0)); }
        /// Writes a formatted string with one double argument.
        static void Write(const std::string& format, double arg0)
            { Write(System::String::Format(format, arg0)); }
        /// Writes a formatted string with one string argument.
        static void Write(const std::string& format, const std::string& arg0)
            { Write(System::String::Format(format, arg0)); }
        /// Writes a formatted string with two integer arguments.
        static void Write(const std::string& format, intcs arg0, intcs arg1)
            { Write(System::String::Format(format, arg0, arg1)); }
        /// Writes a formatted string with two string arguments.
        static void Write(const std::string& format, const std::string& arg0, const std::string& arg1)
            { Write(System::String::Format(format, arg0, arg1)); }

        // --- WriteLine (format) ---
        /// Writes a formatted string with one integer argument followed by a line terminator.
        static void WriteLine(const std::string& format, intcs arg0)
            { WriteLine(System::String::Format(format, arg0)); }
        /// Writes a formatted string with one double argument followed by a line terminator.
        static void WriteLine(const std::string& format, double arg0)
            { WriteLine(System::String::Format(format, arg0)); }
        /// Writes a formatted string with one string argument followed by a line terminator.
        static void WriteLine(const std::string& format, const std::string& arg0)
            { WriteLine(System::String::Format(format, arg0)); }
        /// Writes a formatted string with two integer arguments followed by a line terminator.
        static void WriteLine(const std::string& format, intcs arg0, intcs arg1)
            { WriteLine(System::String::Format(format, arg0, arg1)); }
        /// Writes a formatted string with two string arguments followed by a line terminator.
        static void WriteLine(const std::string& format, const std::string& arg0, const std::string& arg1)
            { WriteLine(System::String::Format(format, arg0, arg1)); }

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

        /// Reads the next character from the standard input stream, or -1 if no more.
        [[nodiscard]] static intcs Read() {
            return std::cin.get();
        }

        // --- Color ---
        /// Gets the foreground color of the console.
        [[nodiscard]] static ConsoleColor getForegroundColorProperty() { return fg_; }
        /// Gets the background color of the console.
        [[nodiscard]] static ConsoleColor getBackgroundColorProperty() { return bg_; }

        /// Sets the foreground color using ANSI escape codes.
        static void setForegroundColorProperty(ConsoleColor color) {
            fg_ = color;
            std::cout << ansiColor(static_cast<int>(color), true);
        }

        /// Sets the background color using ANSI escape codes.
        static void setBackgroundColorProperty(ConsoleColor color) {
            bg_ = color;
            std::cout << ansiColor(static_cast<int>(color), false);
        }

        /// Resets foreground and background colors to defaults.
        static void ResetColor() {
            fg_ = ConsoleColor::White;
            bg_ = ConsoleColor::Black;
            std::cout << "\033[0m";
        }

        // --- Cursor / Window ---
        /// Sets the cursor position (uses ANSI escape; 0-based left/top).
        static void SetCursorPosition(intcs left, intcs top) {
            std::printf("\033[%d;%dH", top + 1, left + 1);
        }

        /// Clears the console screen using an ANSI escape sequence.
        static void Clear() { std::cout << "\033[2J\033[1;1H"; }

        /// Returns the cursor's current column (not queryable portably; returns 0).
        [[nodiscard]] static intcs getCursorLeftProperty()   { return 0; }
        /// Returns the cursor's current row (not queryable portably; returns 0).
        [[nodiscard]] static intcs getCursorTopProperty()    { return 0; }
        /// Returns the console window width in columns (default 80 if not queryable).
        [[nodiscard]] static intcs getWindowWidthProperty()  { return 80; }
        /// Returns the console window height in rows (default 24 if not queryable).
        [[nodiscard]] static intcs getWindowHeightProperty() { return 24; }

        /// Produces a simple console beep via the BEL character.
        static void Beep() { std::cout << '\a' << std::flush; }

    private:
        static inline ConsoleColor fg_ = ConsoleColor::White;
        static inline ConsoleColor bg_ = ConsoleColor::Black;

        static std::string ansiColor(int c, bool fg) {
            char buf[12];
            if (c < 8)
                std::snprintf(buf, sizeof(buf), "\033[%dm", fg ? 30 + c : 40 + c);
            else
                std::snprintf(buf, sizeof(buf), "\033[%dm", fg ? 90 + (c - 8) : 100 + (c - 8));
            return buf;
        }
    };

} // namespace System
