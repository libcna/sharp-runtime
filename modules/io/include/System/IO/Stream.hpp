// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstddef>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IO/SeekOrigin.hpp"

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
     *
     * @par Capability properties: a deliberate deviation you must read before subclassing
     * In real .NET, `Stream.CanRead`, `Stream.CanWrite` and `Stream.CanSeek` are all
     * **abstract** (`Stream.cs:29-31`), so a subclass cannot fail to answer them. Here they
     * are virtual with **three different defaults**, and a subclass that omits an override
     * is therefore **making a declaration by silence**:
     *
     * | Property | Default here | Omitting the override declares |
     * |---|---|---|
     * | getCanWriteProperty() | `false` | "this stream **cannot** be written" |
     * | getCanReadProperty()  | `true`  | "this stream **can** be read" |
     * | getCanSeekProperty()  | `false` | "this stream **cannot** seek" |
     *
     * The two mistakes those defaults invite point in **opposite** directions, which is why
     * they are worth stating together rather than one at a time:
     *
     * - **Overriding Write() but not getCanWriteProperty()** produces a stream that writes
     *   correctly and *declares that it cannot*. `BinaryWriter` already rejects such a
     *   stream at construction with `ArgumentException("Stream was not writable.")`
     *   (`BinaryWriter.cpp:17-19`), so the mistake is silent right up to the point where it
     *   is fatal. **Always override getCanWriteProperty() in a writable stream.**
     * - **Not overriding getCanReadProperty() in an unreadable stream** produces the reverse:
     *   the stream claims it can be read, so `StreamReader`'s and `BinaryReader`'s guards
     *   pass something they should have rejected. **Always override it in a stream that
     *   cannot be read.**
     *
     * A seekable stream should override getCanSeekProperty() too. Note that Seek() and
     * getPositionProperty() are **not** gated on it — Seek()'s default is expressed in terms
     * of Position/Length — so an un-overridden getCanSeekProperty() does not stop seeking
     * from working; it only makes capability-checking callers such as
     * `BinaryReader::PeekChar` refuse.
     *
     * A stream whose capability depends on its own lifetime must fold that in: see
     * `MemoryStream::getCanReadProperty()` (`isOpen_`) and the zlib wrappers, both of which
     * return `false` once closed. Reporting a capability a closed stream no longer has is a
     * defect, not a shortcut.
     *
     * The full contract, the alternatives considered, and why these properties are not simply
     * made pure virtual are recorded in `docs/StreamCapabilityContractDesign.md`.
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

        /**
         * Returns true if this stream supports writing.
         *
         * **Defaults to `false`**, unlike .NET where `Stream.CanWrite` is abstract. A subclass
         * that overrides Write() and *not* this property declares itself unwritable while
         * being writable in fact; `BinaryWriter` already rejects such a stream at construction
         * (`ArgumentException("Stream was not writable.")`). Override this in every writable
         * stream. See the class doc-comment and `docs/StreamCapabilityContractDesign.md`.
         */
        [[nodiscard]] virtual bool getCanWriteProperty() const { return false; }

        /**
         * Returns true if this stream supports reading.
         *
         * **Defaults to `true`** — the opposite direction from getCanWriteProperty(), and also
         * unlike .NET where `Stream.CanRead` is abstract. A subclass that cannot be read and
         * does not override this claims that it can, so `StreamReader`'s and `BinaryReader`'s
         * guards will accept it. Override this in every stream that cannot be read. See the
         * class doc-comment and `docs/StreamCapabilityContractDesign.md`.
         */
        [[nodiscard]] virtual bool getCanReadProperty()  const { return true; }

        /**
         * Returns the current position within the stream.
         * Default implementation throws NotSupportedException; override in seekable streams.
         */
        [[nodiscard]] virtual intcs getPositionProperty() const;

        /**
         * Sets the current position within the stream.
         * Default implementation throws NotSupportedException; override in seekable streams.
         * @param value The new position within the stream.
         */
        virtual void setPositionProperty(intcs value);

        /**
         * Returns true if this stream supports seeking.
         *
         * **Defaults to `false`**, unlike .NET where `Stream.CanSeek` is abstract. Note that
         * this property does **not** gate Seek() or getPositionProperty(): Seek()'s default is
         * expressed in terms of Position/Length, so leaving this un-overridden does not stop
         * seeking from working — it only makes capability-checking callers such as
         * `BinaryReader::PeekChar` refuse. Override this in every seekable stream. See the
         * class doc-comment and `docs/StreamCapabilityContractDesign.md`.
         */
        [[nodiscard]] virtual bool getCanSeekProperty() const { return false; }

        /**
         * Sets the position within the stream relative to @p origin.
         * Default implementation is expressed in terms of getPositionProperty()/setPositionProperty()/
         * getLengthProperty(), so it works automatically for any stream that supports Position;
         * streams that don't support seeking inherit the NotSupportedException thrown by those.
         * @param offset Byte offset relative to @p origin.
         * @param origin Reference point from which to seek.
         * @return The new position within the stream.
         */
        virtual intcs Seek(intcs offset, SeekOrigin origin);

        /**
         * Sets the length of the stream.
         * Default implementation throws NotSupportedException; override in streams that can resize.
         * @param value The desired length, in bytes.
         */
        virtual void SetLength(intcs value);

        /**
         * Reads a single byte from the stream and advances the position by one byte.
         * Default implementation is expressed in terms of Read(); override for a more efficient path.
         * @return The byte read, or -1 if at the end of the stream.
         */
        virtual intcs ReadByte();

        /** Flushes any buffered data to the underlying device. Default is a no-op. */
        virtual void Flush() {}
    };
}
