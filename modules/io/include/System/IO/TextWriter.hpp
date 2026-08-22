// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <charconv>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;
    using SharpRuntime::Single;

    /**
     * @brief Represents a writer that can write a sequential series of characters.
     *
     * Partial C++ counterpart of .NET System.IO.TextWriter.
     *
     * @note Status: Partial
     */
    class TextWriter {
    public:
        /** Destroys the TextWriter and releases any resources. */
        virtual ~TextWriter() = default;

        /** Writes a string to the output. */
        virtual void Write(const std::string& value) = 0;
        /**
         * Writes a null-terminated C-string to the output.
         *
         * Without this overload, a string literal passed to a TextWriter& would bind to
         * Write(bool) instead of Write(const std::string&): a pointer-to-bool standard
         * conversion is preferred over the user-defined conversion to std::string during
         * overload resolution, so e.g. Write("hello") would silently write "True".
         *
         * A null @p value writes nothing and returns normally. This overload has no .NET
         * counterpart -- it exists only as this port's spelling of "a string" for the
         * literal case above -- so it takes the behaviour .NET gives a null string:
         * TextWriter.cs:277-283, `public virtual void Write(string? value) { if (value !=
         * null) Write(value.ToCharArray()); }`, and the same shape at TextWriter.cs:162-168
         * for `Write(char[]? buffer)`. .NET never throws for a null text argument, so a
         * guard that threw ArgumentNullException would be a divergence, not a repair.
         * The test is before the std::string is formed because constructing one from a
         * null pointer is undefined behaviour (ticket #1809; measured as a libstdc++
         * std::logic_error here and as an AddressSanitizer SEGV through StreamWriter's
         * override, build-probe/1823_prefix_defects.log cases 20-25).
         *
         * @param value Null-terminated string to write, or null to write nothing.
         */
        virtual void Write(const char* value)         { if (value != nullptr) Write(std::string(value)); }
        /** Writes a single character to the output. */
        virtual void Write(char value)                { Write(std::string(1, value)); }
        /** Writes a 32-bit integer to the output. */
        virtual void Write(intcs value)               { Write(std::to_string(value)); }
        /** Writes a 64-bit integer to the output. */
        virtual void Write(longcs value)              { Write(std::to_string(value)); }
        /** Writes a double-precision floating-point value to the output. */
        virtual void Write(double value) {
            std::array<char, 64> buf;
            auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
            Write(ec == std::errc{} ? std::string(buf.data(), ptr) : std::to_string(value));
        }
        /** Writes a single-precision floating-point value to the output. */
        virtual void Write(Single value) {
            std::array<char, 32> buf;
            auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
            Write(ec == std::errc{} ? std::string(buf.data(), ptr) : std::to_string(value));
        }
        /** Writes a boolean value as "True" or "False" to the output. */
        virtual void Write(bool value)                { Write(value ? std::string("True") : std::string("False")); }

        /** Writes a string followed by a line terminator. */
        virtual void WriteLine(const std::string& value) { Write(value); Write(NewLine()); }
        /**
         * Writes a null-terminated C-string followed by a line terminator.
         *
         * A null @p value writes the line terminator and nothing else -- not nothing at
         * all. Verified against TextWriter.cs:502-509, whose WriteLine(string? value)
         * writes the value only when it is non-null but writes CoreNewLineStr
         * unconditionally. See Write(const char*) for why null is a no-op rather than an
         * exception (ticket #1809).
         *
         * @param value Null-terminated string to write, or null to write only the
         *        line terminator.
         */
        virtual void WriteLine(const char* value)         { Write(value); Write(NewLine()); }
        /** Writes a line terminator. */
        virtual void WriteLine()                          { Write(NewLine()); }
        /** Writes a character followed by a line terminator. */
        virtual void WriteLine(char value)                { Write(value); Write(NewLine()); }
        /** Writes a 32-bit integer followed by a line terminator. */
        virtual void WriteLine(intcs value)               { Write(value); Write(NewLine()); }
        /** Writes a 64-bit integer followed by a line terminator. */
        virtual void WriteLine(longcs value)              { Write(value); Write(NewLine()); }
        /** Writes a double followed by a line terminator. */
        virtual void WriteLine(double value)              { Write(value); Write(NewLine()); }
        /** Writes a single-precision float followed by a line terminator. */
        virtual void WriteLine(Single value)              { Write(value); Write(NewLine()); }
        /** Writes a boolean followed by a line terminator. */
        virtual void WriteLine(bool value)                { Write(value); Write(NewLine()); }

        /** Flushes any buffered output to the underlying device. */
        virtual void Flush() {}
        /**
         * @brief Closes the writer.
         *
         * This base implementation deliberately does nothing, matching .NET's `TextWriter`
         * base. Concrete resource-owning writers implement their own lifecycle: `StringWriter`
         * and `StreamWriter` override this member, record a closed state, and reject subsequent
         * writes (ticket #2098 / SR-AUD-343).
         */
        virtual void Close() {}

    protected:
#ifdef _WIN32
        static const std::string& NewLine() { static std::string nl = "\r\n"; return nl; }
#else
        static const std::string& NewLine() { static std::string nl = "\n"; return nl; }
#endif
    };

} // namespace System::IO
