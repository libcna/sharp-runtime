// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/IO/Stream.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO {

    /**
     * @brief Implements a TextWriter for writing characters to a stream in a
     * particular encoding (default: UTF-8).
     *
     * Partial C++ counterpart of .NET System.IO.StreamWriter.
     *
     * @note Status: Implemented
     */
    class StreamWriter {
    private:
        Stream* stream_;
        bool    leaveOpen_;

        void WriteRaw(const char* data, size_t len);

    public:
        /** Constructs a StreamWriter wrapping the given stream. */
        explicit StreamWriter(Stream* stream, bool leaveOpen = false);
        /** Constructs a StreamWriter that writes to a new or truncated file at path. */
        explicit StreamWriter(const std::string& path);
        /** Destroys the StreamWriter and closes the underlying stream if not leaveOpen. */
        virtual ~StreamWriter();

        /** Returns the underlying stream. */
        [[nodiscard]] Stream* getBaseStreamProperty() const { return stream_; }

        /** Writes a string to the stream. */
        virtual void Write(const std::string& value);
        /** Writes a null-terminated character array to the stream. */
        virtual void Write(const char* value);
        /** Writes a single character to the stream. */
        virtual void Write(char value);
        /** Writes a 32-bit integer to the stream. */
        virtual void Write(SharpRuntime::intcs value);
        /** Writes a double to the stream. */
        virtual void Write(double value);
        /** Writes a boolean as "True" or "False" to the stream. */
        virtual void Write(bool value);

        /** Writes a string followed by a line terminator. */
        virtual void WriteLine(const std::string& value);
        /** Writes a null-terminated character array followed by a line terminator. */
        virtual void WriteLine(const char* value);
        /** Writes a line terminator. */
        virtual void WriteLine();

        /** Flushes any buffered data to the underlying stream. */
        virtual void Flush();
        /** Closes the writer and the underlying stream. */
        virtual void Close();

    private:
        bool ownsStream_ = false;
    };

} // namespace System::IO
