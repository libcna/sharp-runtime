// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/IO/Stream.hpp"
#include "System/IO/TextWriter.hpp"
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
    class StreamWriter : public TextWriter {
    private:
        Stream* stream_;
        bool    leaveOpen_;
        bool    ownsStream_ = false;

        void WriteRaw(const char* data, size_t len);

    public:
        /**
         * @brief Constructs a StreamWriter wrapping the given stream.
         *
         * A null @p stream is rejected here rather than carried. With the
         * default `leaveOpen = false` it was previously fatal merely to
         * construct such a writer and let it leave scope, because the
         * destructor closed the stream it did not have. This matches .NET's
         * own constructors and the sibling BinaryWriter in this module.
         *
         * @param stream Stream to write to. Must not be null.
         * @param leaveOpen If false (the default), the stream is closed when this StreamWriter is destroyed.
         * @throws System::ArgumentNullException if @p stream is null.
         */
        explicit StreamWriter(Stream* stream, bool leaveOpen = false);
        /** Constructs a StreamWriter that writes to a new or truncated file at path. */
        explicit StreamWriter(const std::string& path);
        /** Destroys the StreamWriter and closes the underlying stream if not leaveOpen. */
        ~StreamWriter() override;

        /** Returns the underlying stream. */
        [[nodiscard]] Stream* getBaseStreamProperty() const { return stream_; }

        using TextWriter::Write;
        using TextWriter::WriteLine;

        /** Writes a string to the stream. */
        void Write(const std::string& value) override;
        /**
         * @brief Writes a null-terminated character array to the stream.
         *
         * A null @p value writes nothing and returns normally, matching
         * TextWriter::Write(const char*) and .NET's own treatment of a null string
         * (TextWriter.cs:277-283). @see TextWriter::Write(const char*)
         *
         * @param value Null-terminated string to write, or null to write nothing.
         */
        void Write(const char* value) override;

        /** Flushes any buffered data to the underlying stream. */
        void Flush() override;
        /**
         * @brief Closes the underlying stream, unless this writer was constructed with leaveOpen.
         *
         * <b>It does not close the WRITER</b>, and with leaveOpen true it closes nothing at all:
         * a Write after Close() succeeds and the underlying stream grows. That is SR-AUD-337 on
         * the writer side, still open, repaired by ticket #2098, BLOCKED on Approval IO-1.
         */
        void Close() override;
    };

} // namespace System::IO
