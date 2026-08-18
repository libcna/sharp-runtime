// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <array>
#include <charconv>
#include <cstdio>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ConsoleColor.hpp"
#include "System/ConsoleKeyInfo.hpp"
#include "System/ConsoleCancelEventArgs.hpp"
#include "System/String.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;
    using SharpRuntime::uintcs;
    using SharpRuntime::ulongcs;
    using SharpRuntime::Single;

    /**
     * @brief Represents the standard input, output, and error streams for console applications.
     *
     * C++ counterpart of .NET System.Console.
     * All members are static; instantiation is not allowed.
     * Color and cursor features use ANSI escape sequences (POSIX terminals only).
     *
     * @note **Argument-domain contract.** Two doors validate, because current .NET's rejection was
     * measured for them: the colour setters reject a `ConsoleColor` outside 0–15 with
     * `ArgumentException` (SR-AUD-243), and `SetCursorPosition` and its two property aliases reject
     * a negative coordinate with `ArgumentOutOfRangeException` (SR-AUD-244). Both reject *before*
     * storing or emitting anything, so a rejected call leaves this class's state and the terminal
     * exactly as they were.
     *
     * The rest of the argument surface **deliberately does not validate**, and that is a recorded
     * decision rather than an oversight: no upper bound is enforced on a cursor coordinate,
     * `setCursorSizeProperty` accepts values outside its documented 1–100 domain, and
     * `SetWindowSize`, `SetWindowPosition`, `SetBufferSize`, the buffer setters and `MoveBufferArea`
     * accept negative arguments. .NET is believed to reject all of these, but no managed probe
     * measured any of them and this port could only guess at the exception type, parameter name and
     * message. Ticket **#2166** owns the question; the current behaviour is pinned by test so it
     * cannot change silently in the meantime.
     *
     * @note **Thread safety.** All state here is process-global `static inline`. No member is
     * synchronised and none is documented as thread-safe.
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
        /**
         * @brief Writes the specified C-string to the standard output stream.
         *
         * A null @p value writes nothing and returns normally. Console.Write(string?)
         * delegates to Out, a TextWriter, whose Write(string? value) is a no-op for null
         * (TextWriter.cs:277-283) and never throws. Without this test `std::cout << value`
         * sets badbit on std::cout **permanently**, so every subsequent Console write in
         * the process silently produces nothing -- no crash, no exception, no message
         * (ticket #1809, build-probe/1823_prefix_defects.log case 26).
         *
         * @param value Null-terminated string to write, or null to write nothing.
         */
        static void Write(const char* value)         { if (value != nullptr) std::cout << value; }
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
        /** @brief Writes the specified unsigned 32-bit integer to the standard output stream. */
        static void Write(uintcs value)  { std::cout << value; }
        /** @brief Writes the specified unsigned 64-bit integer to the standard output stream. */
        static void Write(ulongcs value) { std::cout << value; }

        /**
         * @brief Writes all characters in a vector to the standard output stream.
         * @param buffer Vector of characters to write.
         */
        static void Write(const std::vector<char>& buffer) {
            std::cout.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        }
        /**
         * @brief Writes @p count characters from @p buffer starting at @p index.
         *
         * C++ counterpart of .NET Console.Write(char[], int, int) (via TextWriter.Write).
         * @param buffer Source vector.
         * @param index  Zero-based start index.
         * @param count  Number of characters to write.
         * @throws System::ArgumentOutOfRangeException if @p index or @p count is negative.
         * @throws System::ArgumentException if @p index and @p count do not describe a valid
         *         range within @p buffer.
         */
        static void Write(const std::vector<char>& buffer, intcs index, intcs count) {
            if (index < 0) throw System::ArgumentOutOfRangeException("index", "Non-negative number required.");
            if (count < 0) throw System::ArgumentOutOfRangeException("count", "Non-negative number required.");
            if (static_cast<intcs>(buffer.size()) - index < count)
                throw System::ArgumentException("Offset and length were out of bounds for the array or count is greater than the number of elements from index to the end of the source collection.");
            if (count > 0)
                std::cout.write(buffer.data() + index, static_cast<std::streamsize>(count));
        }

        // -----------------------------------------------------------------------
        // WriteLine
        // -----------------------------------------------------------------------

        /** @brief Writes the current line terminator to the standard output stream. */
        static void WriteLine()                      { std::cout << NewLine; }
        /** @brief Writes the specified string followed by a line terminator. */
        static void WriteLine(const std::string& v)  { std::cout << v << NewLine; }
        /**
         * @brief Writes the specified C-string followed by a line terminator.
         *
         * A null @p v writes the line terminator and nothing else, matching
         * TextWriter.cs:502-509, whose WriteLine(string? value) writes the value only when
         * it is non-null but writes the line terminator unconditionally. @see Write(const char*)
         * for why null is a no-op rather than an exception (ticket #1809, case 27).
         *
         * @param v Null-terminated string to write, or null to write only the line terminator.
         */
        static void WriteLine(const char* v)         { if (v != nullptr) { std::cout << v; } std::cout << NewLine; }
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
        static void WriteLine(bool v)    { std::cout << (v ? "True" : "False") << NewLine; }
        /** @brief Writes the specified unsigned 32-bit integer followed by a line terminator. */
        static void WriteLine(uintcs v)  { std::cout << v << NewLine; }
        /** @brief Writes the specified unsigned 64-bit integer followed by a line terminator. */
        static void WriteLine(ulongcs v) { std::cout << v << NewLine; }
        /**
         * @brief Writes all characters in a vector followed by a line terminator.
         * @param buffer Vector of characters to write.
         */
        static void WriteLine(const std::vector<char>& buffer) {
            Write(buffer);
            std::cout << NewLine;
        }

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
        [[nodiscard]] static bool getIsInputRedirectedProperty();

        /**
         * @brief Gets a value indicating whether standard output has been redirected.
         * @return true if stdout is not a terminal.
         */
        [[nodiscard]] static bool getIsOutputRedirectedProperty();

        /**
         * @brief Gets a value indicating whether standard error has been redirected.
         * @return true if stderr is not a terminal.
         */
        [[nodiscard]] static bool getIsErrorRedirectedProperty();

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
         * @throws System::ArgumentException if @p color is not one of the sixteen
         *         `ConsoleColor` enumerators. The value is rejected before anything is stored or
         *         emitted, so a rejected call leaves the getter and the terminal untouched.
         */
        static void setForegroundColorProperty(ConsoleColor color) {
            requireDefinedColor(color);
            fg_ = color;
            std::cout << ansiColor(static_cast<int>(color), true);
        }

        /**
         * @brief Sets the background color using ANSI escape codes.
         * @param color The ConsoleColor to set as background.
         * @throws System::ArgumentException if @p color is not one of the sixteen
         *         `ConsoleColor` enumerators.
         */
        static void setBackgroundColorProperty(ConsoleColor color) {
            requireDefinedColor(color);
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
         * @brief The console's coordinate ceiling, `short.MaxValue`, EXCLUSIVE.
         *
         * Ticket #2166 (2026-08-18). .NET validates a cursor coordinate in `Console.cs` itself,
         * before it reaches any platform layer — `if (left < 0 || left >= short.MaxValue)`
         * (`Console.cs:553-556`) — so this bound is platform-independent and is the one thing
         * about these doors that .NET states unconditionally. `SetCursorPosition(32767, 0)` is
         * therefore a rejection, not an acceptance: the comparison is `>=`.
         */
        static constexpr intcs kConsoleCoordinateCeiling = 32767;

        /** @brief .NET's `SR.ArgumentOutOfRange_ConsoleBufferBoundaries` (Strings.resx:200). */
        static constexpr const char* kBufferBoundariesMessage =
            "The value must be greater than or equal to zero and less than the console's buffer "
            "size in that dimension.";

        /** @brief .NET's `SR.ArgumentOutOfRange_ConsoleWindowPos` (Strings.resx:203). */
        static constexpr const char* kWindowPositionMessage =
            "The window position must be set such that the current window size fits within the "
            "console's buffer, and the numbers must not be negative.";

        /** @brief .NET's `SR.ArgumentOutOfRange_ConsoleBufferLessThanWindowSize` (Strings.resx:209). */
        static constexpr const char* kBufferExtentMessage =
            "The console buffer size must not be less than the current size and position of the "
            "console window, nor greater than or equal to short.MaxValue.";

        /**
         * @brief Rejects a buffer extent outside `[1, short.MaxValue)`.
         *
         * .NET's Windows pal checks `width < srWindow.Right + 1 || width >= short.MaxValue`
         * (`ConsolePal.Windows.cs:897-901`). The lower half is window-relative and this port has
         * no window geometry to compare against, so it enforces the part that needs none: an
         * extent must be at least one column and below `short.MaxValue`.
         */
        static void throwIfInvalidBufferExtent(intcs value, const char* paramName) {
            if (value < 1 || value >= kConsoleCoordinateCeiling) {
                throw System::ArgumentOutOfRangeException(paramName, std::to_string(value),
                                                          kBufferExtentMessage);
            }
        }

        /**
         * @brief Rejects a coordinate outside `[0, short.MaxValue)`.
         *
         * .NET passes the offending value as the exception's actual value, so the composed
         * message carries an `Actual value was N.` clause. Reproduced.
         */
        static void throwIfOutsideBufferBoundaries(intcs value, const char* paramName) {
            if (value < 0 || value >= kConsoleCoordinateCeiling) {
                throw System::ArgumentOutOfRangeException(paramName, std::to_string(value),
                                                          kBufferBoundariesMessage);
            }
        }

        /**
         * @brief Sets the cursor position using an ANSI escape sequence (0-based).
         * @param left Column index (0-based).
         * @param top  Row index (0-based).
         * @throws System::ArgumentOutOfRangeException if @p left or @p top is outside
         *         `[0, short.MaxValue)`. The coordinate is rejected before the cache is written
         *         or anything is emitted.
         *
         * @note **Ticket #2166 (2026-08-18) added the upper bound.** #2164 enforced only the
         *   negative case and recorded that the .NET limit was "believed" but unmeasured. It is
         *   measured now, and it is not a belief about a platform layer: .NET validates in
         *   `Console.cs` itself, before dispatch (`Console.cs:550-558`), so the bound applies on
         *   every platform. `SetCursorPosition(32767, 0)` is a **rejection**, because the
         *   comparison is `>=`.
         */
        static void SetCursorPosition(intcs left, intcs top) {
            // The cache is a documented local reduction, so an invalid coordinate would survive in
            // the process even where the terminal ignores the sequence it produces (SR-AUD-244).
            throwIfOutsideBufferBoundaries(left, "left");
            throwIfOutsideBufferBoundaries(top, "top");
            cursorLeft_ = left;
            cursorTop_  = top;
            // The +1 converts 0-based to the ANSI 1-based convention, and it is computed in a wider
            // type on purpose: `left + 1` at INTCS_MAX is signed overflow, which is undefined
            // behaviour rather than a wrong number. Since #2164 deliberately enforces no upper
            // bound (the .NET limit is unmeasured here — see #2166), INTCS_MAX is a reachable
            // argument, and UBSan reported it. Widening removes the UB without changing the output
            // for any value that previously produced a well-defined one.
            std::printf("\033[%lld;%lldH", static_cast<long long>(top) + 1,
                        static_cast<long long>(left) + 1);
        }

        /**
         * @brief Returns the current cursor position as (Left, Top).
         *
         * @note Status: PARTIAL. Unlike real .NET (which queries the terminal's actual
         *   cursor position via a DSR ANSI escape sequence on POSIX), this returns a local
         *   cache that is only updated by SetCursorPosition()/setCursorLeftProperty()/
         *   setCursorTopProperty()/Clear() calls made through this API. Writing text via
         *   Write()/WriteLine() (including text that wraps or contains newlines) moves the
         *   real terminal cursor without updating this cache, so the two can silently
         *   diverge -- do not rely on this for anything that needs the true on-screen
         *   position after writing output.
         * @return std::pair where first=Left, second=Top.
         */
        [[nodiscard]] static std::pair<intcs, intcs> GetCursorPosition() {
            return { cursorLeft_, cursorTop_ };
        }

        /** @brief Clears the console screen using an ANSI escape sequence. */
        static void Clear() { std::cout << "\033[2J\033[1;1H"; cursorLeft_ = 0; cursorTop_ = 0; }

        // -----------------------------------------------------------------------
        // Cursor properties
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the cursor's current column.
         * @note Status: PARTIAL -- see GetCursorPosition()'s doc-comment; this is the same
         *   locally-cached value, not a live terminal query.
         */
        [[nodiscard]] static intcs getCursorLeftProperty() { return cursorLeft_; }
        /**
         * @brief Sets the cursor's column position.
         * @throws System::ArgumentOutOfRangeException if @p v is negative — the parameter is named
         *         `left`, because .NET's `CursorLeft` setter delegates to `SetCursorPosition` too.
         */
        static void setCursorLeftProperty(intcs v) { SetCursorPosition(v, cursorTop_); }

        /**
         * @brief Gets the cursor's current row.
         * @note Status: PARTIAL -- see GetCursorPosition()'s doc-comment; this is the same
         *   locally-cached value, not a live terminal query.
         */
        [[nodiscard]] static intcs getCursorTopProperty() { return cursorTop_; }
        /**
         * @brief Sets the cursor's row position.
         * @throws System::ArgumentOutOfRangeException if @p v is negative — the parameter is named
         *         `top`, because .NET's `CursorTop` setter delegates to `SetCursorPosition` too.
         */
        static void setCursorTopProperty(intcs v) { SetCursorPosition(cursorLeft_, v); }

        /**
         * @brief Gets the height of the cursor within a character cell (1–100 percent).
         * @return The stored cursor size (default 25).
         */
        [[nodiscard]] static intcs getCursorSizeProperty() { return cursorSize_; }
        /**
         * @brief Sets the cursor size, as a percentage in `[1, 100]`.
         *
         * Ticket #2166. The value used to be stored unchecked against a domain the doc-comment
         * itself declared, so `0`, `-1` and `101` were all readable back.
         *
         * The range and the message are .NET's Windows pal
         * (`ConsolePal.Windows.cs:588-590`, `SR.ArgumentOutOfRange_CursorSize`). **On Unix .NET's
         * setter throws `PlatformNotSupportedException` outright** (`ConsolePal.Unix.cs:193-197`),
         * so it defines no range there at all — this port keeps the property working and borrows
         * the only range .NET states. Refusing outright would remove a feature this port offers
         * rather than repair one, which is the wrong direction for a validation ticket.
         *
         * @throws System::ArgumentOutOfRangeException if @p v is outside `[1, 100]`.
         */
        static void setCursorSizeProperty(intcs v) {
            if (v < 1 || v > 100) {
                throw System::ArgumentOutOfRangeException(
                    "value", std::to_string(v),
                    "The cursor size is invalid. It must be a percentage between 1 and 100.");
            }
            cursorSize_ = v;
        }

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

        // -----------------------------------------------------------------------
        // Window properties
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the width of the console window in columns.
         * @return The real terminal width when standard output is a TTY (queried via
         *         ioctl(TIOCGWINSZ) on POSIX, GetConsoleScreenBufferInfo on Windows);
         *         80 as a fallback when output is redirected or the query fails.
         */
        [[nodiscard]] static intcs getWindowWidthProperty();

        /**
         * @brief Gets the height of the console window in rows.
         * @return The real terminal height when standard output is a TTY (queried via
         *         ioctl(TIOCGWINSZ) on POSIX, GetConsoleScreenBufferInfo on Windows);
         *         24 as a fallback when output is redirected or the query fails.
         */
        [[nodiscard]] static intcs getWindowHeightProperty();

        /** @brief Gets the leftmost column of the console window area (always 0). */
        [[nodiscard]] static intcs getWindowLeftProperty() { return 0; }
        /** @brief Gets the topmost row of the console window area (always 0). */
        [[nodiscard]] static intcs getWindowTopProperty()  { return 0; }

        /**
         * @brief Gets the largest possible window width on the current display.
         * @return Terminal query result, or 80 as fallback.
         */
        [[nodiscard]] static intcs getLargestWindowWidthProperty()  { return getWindowWidthProperty(); }
        /**
         * @brief Gets the largest possible window height on the current display.
         * @return Terminal query result, or 24 as fallback.
         */
        [[nodiscard]] static intcs getLargestWindowHeightProperty() { return getWindowHeightProperty(); }

        /**
         * @brief Sets the size of the console window using an xterm OSC escape sequence.
         * @param width  New window width in columns.
         * @param height New window height in rows.
         */
        static void SetWindowSize(intcs width, intcs height) {
            // Ticket #2166. .NET's Windows pal opens with ThrowIfNegativeOrZero on both
            // (ConsolePal.Windows.cs:1020-1021); its Unix pal throws PlatformNotSupportedException
            // and states no range. A window of zero or negative columns is not a window.
            System::ArgumentOutOfRangeException::ThrowIfNegativeOrZero(width, "width");
            System::ArgumentOutOfRangeException::ThrowIfNegativeOrZero(height, "height");
            std::printf("\033[8;%d;%dt", static_cast<int>(height), static_cast<int>(width));
            std::fflush(stdout);
        }

        /**
         * @brief Sets the position of the console window using an xterm OSC escape sequence.
         * @param left New left position in pixels.
         * @param top  New top position in pixels.
         */
        static void SetWindowPosition(intcs left, intcs top) {
            // Ticket #2166. .NET's Windows pal rejects a negative coordinate with
            // SR.ArgumentOutOfRange_ConsoleWindowPos (ConsolePal.Windows.cs:995-1001). The rest of
            // that check is buffer-relative and this port has no buffer geometry to check against,
            // so only the half that needs none is reproduced -- see the migration note.
            if (left < 0) {
                throw System::ArgumentOutOfRangeException("left", std::to_string(left),
                                                          kWindowPositionMessage);
            }
            if (top < 0) {
                throw System::ArgumentOutOfRangeException("top", std::to_string(top),
                                                          kWindowPositionMessage);
            }
            std::printf("\033[3;%d;%dt", static_cast<int>(top), static_cast<int>(left));
            std::fflush(stdout);
        }

        // -----------------------------------------------------------------------
        // Buffer properties (stubs)
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the width of the console screen buffer in columns.
         * @return Same as window width.
         */
        [[nodiscard]] static intcs getBufferWidthProperty()  { return getWindowWidthProperty(); }
        /**
         * @brief Gets the height of the console screen buffer in rows.
         * @return Same as window height.
         */
        [[nodiscard]] static intcs getBufferHeightProperty() { return getWindowHeightProperty(); }

        /**
         * @brief Sets the screen buffer width (no-op in this implementation).
         * @param v New buffer width.
         */
        static void setBufferWidthProperty(intcs v)  { throwIfInvalidBufferExtent(v, "width"); }
        /**
         * @brief Sets the screen buffer height (no-op in this implementation).
         * @param v New buffer height.
         */
        static void setBufferHeightProperty(intcs v) { throwIfInvalidBufferExtent(v, "height"); }

        /**
         * @brief Sets the screen buffer size (no-op in this implementation).
         * @param width  New buffer width.
         * @param height New buffer height.
         */
        static void SetBufferSize(intcs width, intcs height) {
            throwIfInvalidBufferExtent(width, "width");
            throwIfInvalidBufferExtent(height, "height");
        }

        /**
         * @brief Copies a rectangular region of the buffer to another location (no-op stub).
         */
        static void MoveBufferArea(intcs sourceLeft, intcs sourceTop,
                                    intcs sourceWidth, intcs sourceHeight,
                                    intcs targetLeft, intcs targetTop) {
            throwIfOutsideBufferBoundaries(sourceLeft, "sourceLeft");
            throwIfOutsideBufferBoundaries(sourceTop, "sourceTop");
            throwIfOutsideBufferBoundaries(sourceWidth, "sourceWidth");
            throwIfOutsideBufferBoundaries(sourceHeight, "sourceHeight");
            throwIfOutsideBufferBoundaries(targetLeft, "targetLeft");
            throwIfOutsideBufferBoundaries(targetTop, "targetTop");
        }

        /**
         * @brief Copies a rectangular region of the buffer, filling vacated cells (no-op stub).
         */
        static void MoveBufferArea(intcs sourceLeft, intcs sourceTop,
                                    intcs sourceWidth, intcs sourceHeight,
                                    intcs targetLeft, intcs targetTop,
                                    char sourceChar,
                                    ConsoleColor sourceForeColor,
                                    ConsoleColor sourceBackColor) {
            MoveBufferArea(sourceLeft, sourceTop, sourceWidth, sourceHeight, targetLeft, targetTop);
            (void)sourceChar; (void)sourceForeColor; (void)sourceBackColor;
        }

        // -----------------------------------------------------------------------
        // Keyboard state (stubs)
        // -----------------------------------------------------------------------

        /**
         * @brief Gets a value indicating whether the CAPS LOCK key is toggled on.
         * @return false (not queryable portably).
         */
        [[nodiscard]] static bool getCapsLockProperty()   { return false; }
        /**
         * @brief Gets a value indicating whether the NUM LOCK key is toggled on.
         * @return false (not queryable portably).
         */
        [[nodiscard]] static bool getNumberLockProperty() { return false; }

        // -----------------------------------------------------------------------
        // CancelKeyPress event
        // -----------------------------------------------------------------------

        /**
         * @brief Removes the registered CancelKeyPress handler (sets it to null).
         */
        static void removeCancelKeyPressHandler() { cancelKeyPressHandler_ = nullptr; }

        // -----------------------------------------------------------------------
        // Beep
        // -----------------------------------------------------------------------

        /** @brief Produces a simple console beep via the BEL character. */
        static void Beep() { std::cout << '\a' << std::flush; }

        /**
         * @brief Produces a beep of the specified frequency and duration.
         *
         * Frequency and duration are ignored in this implementation — falls back to
         * a simple BEL character, which is the only portable option.
         * @param frequency Tone frequency in hertz (ignored).
         * @param duration  Duration in milliseconds (ignored).
         */
        static void Beep(intcs frequency, intcs duration) {
            (void)frequency; (void)duration;
            std::cout << '\a' << std::flush;
        }

    private:
        static inline ConsoleColor fg_              = ConsoleColor::White;
        static inline ConsoleColor bg_              = ConsoleColor::Black;
        static inline std::string  title_;
        static inline bool         cursorVisible_   = true;
        static inline bool         treatCtrlCAsInput_ = false;
        static inline ConsoleCancelEventHandler cancelKeyPressHandler_;
        static inline intcs cursorLeft_  = 0;
        static inline intcs cursorTop_   = 0;
        static inline intcs cursorSize_  = 25;

        /**
         * @brief Rejects a `ConsoleColor` that is not one of the sixteen enumerators.
         *
         * A scoped enumeration's *type* is not its *value set*: any `intcs` can be cast into
         * `ConsoleColor`, and before ticket #2163 every arm here treated the result as valid.
         * The audit's managed probe recorded current .NET as
         * `color=exception:System.ArgumentException` for a cast 99, because Unix `ConsolePal`
         * rejects values outside the 0–15 range.
         *
         * The message follows this repository's settled precedent for an enum-domain rejection
         * (#1954, #1992, #2148); the audit measured the exception *type*, not .NET's text, and
         * `/rv` is absent, so the text is recorded as unverified under #2166.
         */
        static void requireDefinedColor(ConsoleColor color) {
            const int value = static_cast<int>(color);
            if (value < 0 || value > 15) {
                throw System::ArgumentException("Enum value was out of legal range.", "value");
            }
        }

        /**
         * @brief Builds the ANSI SGR sequence for a colour index.
         *
         * @pre @p c is in 0–15; every caller runs `requireDefinedColor` first. That precondition
         * is what makes this buffer the right size rather than a lucky one: at
         * `static_cast<ConsoleColor>(INT_MIN)` the pre-#2163 code emitted `ESC[-21474836` —
         * truncated mid-number with **no terminating `m`** — and a terminal that receives an
         * unterminated escape consumes the output that follows it looking for the terminator.
         * GCC reports the same thing statically under `-Wall -Wextra`.
         */
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
