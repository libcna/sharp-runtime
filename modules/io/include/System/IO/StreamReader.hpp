// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/IO/Stream.hpp"
#include "System/IO/TextReader.hpp"

namespace System::IO
{
    /**
     * @brief Reads characters from a Stream.
     *
     * This is a lightweight subset of the .NET StreamReader class: encoding
     * detection/BOM handling is not implemented, and bytes are read as raw
     * Latin-1/ASCII characters (no multi-byte decoding).
     *
     * @note Status: PARTIAL
     */
    class StreamReader : public TextReader
    {
    private:
        Stream* stream_;
        bool    leaveOpen_;
        bool    ownsStream_;
        bool    hasPeeked_ = false;
        bytecs  peeked_ = 0;

    public:
        /**
         * @brief Initializes a StreamReader for the specified stream.
         *
         * A null @p stream is rejected here rather than carried, so no read
         * can report an empty document when there was in fact no stream at
         * all. This matches .NET's own constructors and the sibling
         * BinaryReader in this module.
         *
         * A @p stream that exists but declares itself unreadable is rejected
         * for the same reason: it can only ever answer -1 and "", which are
         * indistinguishable from an empty document. Verified against
         * StreamReader.cs:145-148, which follows its null check with
         * `if (!stream.CanRead) throw new ArgumentException(
         * SR.Argument_StreamNotReadable);` -- message only, no parameter name.
         *
         * @param stream Stream to read from. Must not be null and must report
         *        getCanReadProperty() == true.
         * @param leaveOpen If false (the default), the stream is closed when this StreamReader is destroyed.
         * @throws System::ArgumentNullException if @p stream is null.
         * @throws System::ArgumentException if @p stream reports
         *         getCanReadProperty() == false.
         */
        explicit StreamReader(Stream* stream, bool leaveOpen = false);

        /** @brief Opens the file at @p path for reading and wraps it in a StreamReader that owns the underlying FileStream. */
        explicit StreamReader(const std::string& path);

        /** Destroys the StreamReader, closing the underlying stream unless leaveOpen was set. */
        ~StreamReader() override;

        /** Returns the underlying stream. */
        [[nodiscard]] Stream* getBaseStreamProperty() const { return stream_; }

        /** Returns the next character without advancing the position, or -1 at end. */
        intcs Peek() override;

        /** Reads and returns the next character, or -1 at end. */
        intcs Read() override;

        /** Reads the next line, stripping the line terminator. */
        [[nodiscard]] std::string ReadLine() override;

        /** Reads all remaining characters from the current position to the end. */
        [[nodiscard]] std::string ReadToEnd() override;

        /** Closes the underlying stream. */
        void Close() override;
    };
}
