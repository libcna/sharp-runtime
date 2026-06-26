// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstddef>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO
{
    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;

    /**
     * <summary>
     * Provides a generic view of a sequence of bytes.
     * 
     * Lightweight subset of the .NET Stream API intended for practical use
     * in source ports. Derived classes must implement Read(), Close(), and
     * getLengthProperty(); Write() and WriteByte() may optionally be overridden.
     * </summary>
     */
    class Stream
    {
    public:
        virtual ~Stream() = default;

        /**
         * Reads up to @p count bytes from the stream into @p buffer starting at @p offset.
         * @param buffer Destination byte buffer.
         * @param offset Byte offset within @p buffer at which to begin writing.
         * @param count Maximum number of bytes to read.
         * @return Number of bytes actually read; 0 at end-of-stream.
         */
        virtual intcs Read(bytecs buffer[], intcs offset, intcs count) = 0;

        /** Closes the stream and releases any associated resources. */
        virtual void Close() = 0;

        /** Returns the total length of the stream in bytes. */
        [[nodiscard]] virtual intcs getLengthProperty() const = 0;

        /**
         * Writes @p count bytes from @p buffer (starting at @p offset) to the stream.
         * Default implementation throws NotSupportedException; override in writable streams.
         * @param buffer Source byte buffer.
         * @param offset Byte offset within @p buffer to begin reading.
         * @param count Number of bytes to write.
         */
        virtual void Write(const bytecs buffer[], intcs offset, intcs count);

        /**
         * Writes a single byte @p value to the stream.
         * Default implementation throws NotSupportedException; override in writable streams.
         * @param value Byte to write.
         */
        virtual void WriteByte(bytecs value);

        /** Returns true if this stream supports writing. */
        [[nodiscard]] virtual bool getCanWriteProperty() const { return false; }

        /** Returns true if this stream supports reading. */
        [[nodiscard]] virtual bool getCanReadProperty()  const { return true; }

        /** Flushes any buffered data to the underlying device. Default is a no-op. */
        virtual void Flush() {}
    };
}
