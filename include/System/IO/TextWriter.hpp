// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
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
        /// Destroys the TextWriter and releases any resources.
        virtual ~TextWriter() = default;

        /// Writes a string to the output.
        virtual void Write(const std::string& value) = 0;
        /// Writes a single character to the output.
        virtual void Write(char value)                { Write(std::string(1, value)); }
        /// Writes a 32-bit integer to the output.
        virtual void Write(intcs value)               { Write(std::to_string(value)); }
        /// Writes a 64-bit integer to the output.
        virtual void Write(longcs value)              { Write(std::to_string(value)); }
        /// Writes a double-precision floating-point value to the output.
        virtual void Write(double value)              { Write(std::to_string(value)); }
        /// Writes a single-precision floating-point value to the output.
        virtual void Write(Single value)              { Write(std::to_string(value)); }
        /// Writes a boolean value as "True" or "False" to the output.
        virtual void Write(bool value)                { Write(value ? std::string("True") : std::string("False")); }

        /// Writes a string followed by a line terminator.
        virtual void WriteLine(const std::string& value) { Write(value); Write(NewLine()); }
        /// Writes a line terminator.
        virtual void WriteLine()                          { Write(NewLine()); }
        /// Writes a character followed by a line terminator.
        virtual void WriteLine(char value)                { Write(value); Write(NewLine()); }
        /// Writes a 32-bit integer followed by a line terminator.
        virtual void WriteLine(intcs value)               { Write(value); Write(NewLine()); }
        /// Writes a 64-bit integer followed by a line terminator.
        virtual void WriteLine(longcs value)              { Write(value); Write(NewLine()); }
        /// Writes a double followed by a line terminator.
        virtual void WriteLine(double value)              { Write(value); Write(NewLine()); }
        /// Writes a single-precision float followed by a line terminator.
        virtual void WriteLine(Single value)              { Write(value); Write(NewLine()); }
        /// Writes a boolean followed by a line terminator.
        virtual void WriteLine(bool value)                { Write(value); Write(NewLine()); }

        /// Flushes any buffered output to the underlying device.
        virtual void Flush() {}
        /// Closes the writer.
        virtual void Close() {}

    protected:
#ifdef _WIN32
        static const std::string& NewLine() { static std::string nl = "\r\n"; return nl; }
#else
        static const std::string& NewLine() { static std::string nl = "\n"; return nl; }
#endif
    };

} // namespace System::IO
