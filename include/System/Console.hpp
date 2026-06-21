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
#include "System/ConsoleKeyInfo.hpp"
#include "System/ConsoleCancelEventArgs.hpp"
#include "System/String.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;
    using SharpRuntime::Single;

    /**
     * @brief Represents the standard input, output, and error streams for console applications.
     *
     * C++ counterpart of .NET System.Console.
     * All members are static; instantiation is not allowed.
     * Color and cursor features use ANSI escape sequences (POSIX terminals only).
     */
    class Console {
    public:
        /** @brief Deleted constructor — all members are static. */
        Console() = delete;

        /**
         * @brief The newline string used by the current environment.
         * "\r\n" on Windows, "\n" on POSIX.
         */
        static inline const std::string NewLine =
#ifdef _WIN32
            "\r\n";
#else
            "\n";
#endif

        // -----------------------------------------------------------------------
        // Write
        // -----------------------------------------------------------------------

        /** @brief Writes the specified string to the standard output stream. */
        static void Write(const std::string& value) { std::cout << value; }
        /** @brief Writes the specified C-string to the standard output stream. */
        static void Write(const char* value)         { std::cout << value; }
        /** @brief Writes the specified character to the standard output stream. */
        static void Write(char value)                { std::cout << value; }
        /** @brief Writes the specified integer to the standard output stream. */
        static void Write(intcs value)               { std::cout << value; }
        /** @brief Writes the specified 64-bit integer to the standard output stream. */
        static void Write(longcs value)              { std::cout << value; }
        /** @brief Writes the specified double to the standard output stream. */
        static void Write(double value) {
            std::array<char, 64> buf;
            auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
            if (ec == std::errc{}) std::cout.write(buf.data(), ptr - buf.data());
            else std::cout << value;
        }
        /** @brief Writes the specified float to the standard output stream. */
        static void Write(float value) {
            std::array<char, 32> buf;
            auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
            if (ec == std::errc{}) std::cout.write(buf.data(), ptr - buf.data());
            else std::cout << value;
        }
        /** @brief Writes the specified Boolean to the standard output stream. */
        static void Write(bool value) { std::cout << (value ? "True" : "False"); }

        // -----------------------------------------------------------------------
        // WriteLine
        // -----------------------------------------------------------------------

        /** @brief Writes the current line terminator to the standard output stream. */
        static void WriteLine()                      { std::cout << NewLine; }
        /** @brief Writes the specified string followed by a line terminator. */
        static void WriteLine(const std::string& v)  { std::cout << v << NewLine; }
        /** @brief Writes the specified C-string followed by a line terminator. */
        static void WriteLine(const char* v)         { std::cout << v << NewLine; }
        /** @brief Writes the specified character followed by a line terminator. */
        static void WriteLine(char v)                { std::cout << v << NewLine; }
        /** @brief Writes the specified integer followed by a line terminator. */
        static void WriteLine(intcs v)               { std::cout << v << NewLine; }
        /** @brief Writes the specified 64-bit integer followed by a line terminator. */
        static void WriteLine(longcs v)              { std::cout << v << NewLine; }
        /** @brief Writes the specified double followed by a line terminator. */
        static void WriteLine(double v) { Write(v); std::cout << NewLine; }
        /** @brief Writes the specified float followed by a line terminator. */
        static void WriteLine(float v)  { Write(v); std::cout << NewLine; }
        /** @brief Writes the specified Boolean followed by a line terminator. */
        static void WriteLine(bool v)   { std::cout << (v ? "True" : "False") << NewLine; }

        // -----------------------------------------------------------------------
        // Write (format)
        // -----------------------------------------------------------------------

        /** @brief Writes a composite format string with one integer argument. */
        static void Write(const std::string& format, intcs arg0)
            { Write(System::String::Format(format, arg0)); }
        /** @brief Writes a composite format string with one double argument. */
        static void Write(const std::string& format, double arg0)
            { Write(System::String::Format(format, arg0)); }
        /** @brief Writes a composite format string with one string argument. */
        static void Write(const std::string& format, const std::string& arg0)
            { Write(System::String::Format(format, arg0)); }
        /** @brief Writes a composite format string with two integer arguments. */
        static void Write(const std::string& format, intcs arg0, intcs arg1)
            { Write(System::String::Format(format, arg0, arg1)); }
        /** @brief Writes a composite format string with two string arguments. */
        static void Write(const std::string& format, const std::string& arg0, const std::string& arg1)
            { Write(System::String::Format(format, arg0, arg1)); }

        // -----------------------------------------------------------------------
        // WriteLine (format)
        // -----------------------------------------------------------------------

        /** @brief Writes a composite format string with one integer argument, then a line terminator. */
        static void WriteLine(const std::string& format, intcs arg0)
            { WriteLine(System::String::Format(format, arg0)); }
        /** @brief Writes a composite format string with one double argument, then a line terminator. */
        static void WriteLine(const std::string& format, double arg0)
            { WriteLine(System::String::Format(format, arg0)); }
        /** @brief Writes a composite format string with one string argument, then a line terminator. */
        static void WriteLine(const std::string& format, const std::string& arg0)
            { WriteLine(System::String::Format(format, arg0)); }
        /** @brief Writes a composite format string with two integer arguments, then a line terminator. */
        static void WriteLine(const std::string& format, intcs arg0, intcs arg1)
            { WriteLine(System::String::Format(format, arg0, arg1)); }
        /** @brief Writes a composite format string with two string arguments, then a line terminator. */
        static void WriteLine(const std::string& format, const std::string& arg0, const std::string& arg1)
            { WriteLine(System::String::Format(format, arg0, arg1)); }

        // -----------------------------------------------------------------------
        // Error stream
        // -----------------------------------------------------------------------

        /** @brief Writes the specified string to the standard error stream. */
        static void Error_Write(const std::string& v)     { std::cerr << v; }
        /** @brief Writes the specified string followed by a line terminator to the standard error stream. */
        static void Error_WriteLine(const std::string& v) { std::cerr << v << NewLine; }

        // -----------------------------------------------------------------------
        // Input
        // -----------------------------------------------------------------------

        /**
         * @brief Reads the next line of characters from the standard input stream.
         * @return The next line, or an empty string at end-of-file.
         */
        [[nodiscard]] static std::string ReadLine() {
            std::string line;
            std::getline(std::cin, line);
            return line;
        }

        /**
         * @brief Reads the next character from the standard input stream.
         * @return The next character as an int, or -1 at end-of-stream.
         */
        [[nodiscard]] static intcs Read() { return std::cin.get(); }

        /**
         * @brief Reads the next key pressed from the console.
         *
         * Stub — reads a character from stdin and wraps it in a ConsoleKeyInfo.
         * @param intercept If true the key is not echoed (not enforced in this impl).
         * @return A ConsoleKeyInfo describing the pressed key.
         */
        [[nodiscard]] static ConsoleKeyInfo ReadKey(bool intercept = false) {
            (void)intercept;
            int ch = std::cin.get();
            if (ch == EOF) return ConsoleKeyInfo{};
            return ConsoleKeyInfo{
                static_cast<char>(ch),
                ConsoleKey::None,
                false, false, false
            };
        }

        /**
         * @brief Gets a value indicating whether a key press is available in the input stream.
         *
         * Always returns false in this implementation (stdin peeking is platform-specific).
         * @return false (stub).
         */
        [[nodiscard]] static bool getKeyAvailableProperty() { return false; }

        // -----------------------------------------------------------------------
        // Title
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the title of the console window.
         * @return The stored title string (set via setTitleProperty).
         */
        [[nodiscard]] static const std::string& getTitleProperty() { return title_; }

        /**
         * @brief Sets the title of the console window.
         * On POSIX terminals this emits an ANSI/xterm OSC escape sequence.
         * @param title The new title string.
         */
        static void setTitleProperty(const std::string& title) {
            title_ = title;
#ifndef _WIN32
            std::cout << "\033]0;" << title << "\007" << std::flush;
#endif
        }

        // -----------------------------------------------------------------------
        // Redirection detection
        // -----------------------------------------------------------------------

        /**
         * @brief Gets a value indicating whether standard input has been redirected.
         * @return true if stdin is not a terminal.
         */
        [[nodiscard]] static bool getIsInputRedirectedProperty() {
#if defined(__linux__) || defined(__APPLE__)
            return !isatty(fileno(stdin));
#else
            return false;
#endif
        }

        /**
         * @brief Gets a value indicating whether standard output has been redirected.
         * @return true if stdout is not a terminal.
         */
        [[nodiscard]] static bool getIsOutputRedirectedProperty() {
#if defined(__linux__) || defined(__APPLE__)
            return !isatty(fileno(stdout));
#else
            return false;
#endif
        }

        /**
         * @brief Gets a value indicating whether standard error has been redirected.
         * @return true if stderr is not a terminal.
         */
        [[nodiscard]] static bool getIsErrorRedirectedProperty() {
#if defined(__linux__) || defined(__APPLE__)
            return !isatty(fileno(stderr));
#else
            return false;
#endif
        }

        // -----------------------------------------------------------------------
        // Input control
        // -----------------------------------------------------------------------

        /**
         * @brief Gets a value indicating whether CTRL+C is treated as ordinary input.
         * @return The stored value (not enforced in this implementation).
         */
        [[nodiscard]] static bool getTreatControlCAsInputProperty() { return treatCtrlCAsInput_; }
        /** @brief Sets whether CTRL+C should be treated as ordinary input (stored but not enforced). */
        static void setTreatControlCAsInputProperty(bool v) { treatCtrlCAsInput_ = v; }

        /**
         * @brief Stub: registers a handler for the CancelKeyPress event.
         *
         * The handler is stored but not automatically invoked in this implementation.
         * @param handler The ConsoleCancelEventHandler to register.
         */
        static void addCancelKeyPressHandler(ConsoleCancelEventHandler handler) {
            cancelKeyPressHandler_ = std::move(handler);
        }

        // -----------------------------------------------------------------------
        // Color
        // -----------------------------------------------------------------------

        /** @brief Gets the foreground color of the console. */
        [[nodiscard]] static ConsoleColor getForegroundColorProperty() { return fg_; }
        /** @brief Gets the background color of the console. */
        [[nodiscard]] static ConsoleColor getBackgroundColorProperty() { return bg_; }

        /**
         * @brief Sets the foreground color using ANSI escape codes.
         * @param color The ConsoleColor to set as foreground.
         */
        static void setForegroundColorProperty(ConsoleColor color) {
            fg_ = color;
            std::cout << ansiColor(static_cast<int>(color), true);
        }

        /**
         * @brief Sets the background color using ANSI escape codes.
         * @param color The ConsoleColor to set as background.
         */
        static void setBackgroundColorProperty(ConsoleColor color) {
            bg_ = color;
            std::cout << ansiColor(static_cast<int>(color), false);
        }

        /** @brief Resets foreground and background colors to their defaults. */
        static void ResetColor() {
            fg_ = ConsoleColor::White;
            bg_ = ConsoleColor::Black;
            std::cout << "\033[0m";
        }

        // -----------------------------------------------------------------------
        // Cursor / Window
        // -----------------------------------------------------------------------

        /**
         * @brief Sets the cursor position using an ANSI escape sequence (0-based).
         * @param left Column index (0-based).
         * @param top  Row index (0-based).
         */
        static void SetCursorPosition(intcs left, intcs top) {
            std::printf("\033[%d;%dH", top + 1, left + 1);
        }

        /** @brief Clears the console screen using an ANSI escape sequence. */
        static void Clear() { std::cout << "\033[2J\033[1;1H"; }

        /** @brief Returns the cursor's current column (0 — not queryable portably). */
        [[nodiscard]] static intcs getCursorLeftProperty()   { return 0; }
        /** @brief Returns the cursor's current row (0 — not queryable portably). */
        [[nodiscard]] static intcs getCursorTopProperty()    { return 0; }
        /** @brief Returns the console window width in columns (default 80). */
        [[nodiscard]] static intcs getWindowWidthProperty()  { return 80; }
        /** @brief Returns the console window height in rows (default 24). */
        [[nodiscard]] static intcs getWindowHeightProperty() { return 24; }

        /**
         * @brief Gets a value indicating whether the cursor is visible.
         * @return The stored visibility flag.
         */
        [[nodiscard]] static bool getCursorVisibleProperty() { return cursorVisible_; }
        /**
         * @brief Sets whether the cursor is visible using an ANSI escape sequence.
         * @param v true to show the cursor; false to hide it.
         */
        static void setCursorVisibleProperty(bool v) {
            cursorVisible_ = v;
            std::cout << (v ? "\033[?25h" : "\033[?25l") << std::flush;
        }

        /** @brief Produces a simple console beep via the BEL character. */
        static void Beep() { std::cout << '\a' << std::flush; }

    private:
        static inline ConsoleColor fg_              = ConsoleColor::White;
        static inline ConsoleColor bg_              = ConsoleColor::Black;
        static inline std::string  title_;
        static inline bool         cursorVisible_   = true;
        static inline bool         treatCtrlCAsInput_ = false;
        static inline ConsoleCancelEventHandler cancelKeyPressHandler_;

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
