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
        virtual ~TextWriter() = default;

        virtual void Write(const std::string& value) = 0;
        virtual void Write(char value)                { Write(std::string(1, value)); }
        virtual void Write(intcs value)               { Write(std::to_string(value)); }
        virtual void Write(longcs value)              { Write(std::to_string(value)); }
        virtual void Write(double value)              { Write(std::to_string(value)); }
        virtual void Write(Single value)              { Write(std::to_string(value)); }
        virtual void Write(bool value)                { Write(value ? std::string("True") : std::string("False")); }

        virtual void WriteLine(const std::string& value) { Write(value); Write(NewLine()); }
        virtual void WriteLine()                          { Write(NewLine()); }
        virtual void WriteLine(char value)                { Write(value); Write(NewLine()); }
        virtual void WriteLine(intcs value)               { Write(value); Write(NewLine()); }
        virtual void WriteLine(longcs value)              { Write(value); Write(NewLine()); }
        virtual void WriteLine(double value)              { Write(value); Write(NewLine()); }
        virtual void WriteLine(Single value)              { Write(value); Write(NewLine()); }
        virtual void WriteLine(bool value)                { Write(value); Write(NewLine()); }

        virtual void Flush() {}
        virtual void Close() {}

    protected:
#ifdef _WIN32
        static const std::string& NewLine() { static std::string nl = "\r\n"; return nl; }
#else
        static const std::string& NewLine() { static std::string nl = "\n"; return nl; }
#endif
    };

} // namespace System::IO
