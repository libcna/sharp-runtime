// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <vector>
#include "System/IO/Stream.hpp"

namespace System::IO
{
    /**
     * @brief A stream backed by an in-memory byte buffer.
     *
     * Used to wrap data loaded from sources such as Android APK assets.
     *
     * @note Status: IMPLEMENTED
     */
    class MemoryStream : public Stream
    {
    private:
        std::vector<bytecs> data_;
        intcs position_;
        bool  writable_;
        bool  isOpen_ = true;

        /**
         * @brief Throws System::ObjectDisposedException if this stream has been closed.
         *
         * Matches real .NET's MemoryStream.EnsureNotClosed(), called from Read/Write/WriteByte/
         * the Length/Position getters and the Position setter/SetLength -- but deliberately NOT
         * from GetBuffer()/ToArray(), which real .NET keeps usable after Dispose() specifically
         * so callers can still retrieve data from an already-closed stream.
         */
        void ensureNotClosed() const;

    public:
        /** @brief Creates an empty, writable MemoryStream. */
        MemoryStream();

        /**
         * @brief Creates a MemoryStream over a copy of a byte buffer.
         *
         * The default is writable, matching .NET's MemoryStream(byte[])
         * constructor. Callers that explicitly need a read-only stream can
         * pass false for @p writable.
         *
         * Arguments are validated before the source is read, so an invalid
         * call reports the .NET argument diagnostic instead of dereferencing
         * the caller's pointer. A null @p buffer is accepted only when
         * @p size is zero, which is this port's spelling of an empty source
         * range; .NET's `byte[]` parameter has no equivalent spelling and so
         * rejects null outright.
         *
         * @param buffer Pointer to the source bytes.
         * @param size Number of bytes to copy.
         * @param writable Whether writing to the copied buffer is allowed.
         * @throws System::ArgumentNullException if @p buffer is null and
         *         @p size is not zero.
         * @throws System::ArgumentOutOfRangeException if @p size is negative.
         */
        MemoryStream(const bytecs* buffer, intcs size, bool writable = true);

        ~MemoryStream() override = default;

        /** Reads up to count bytes into buffer at offset; returns bytes actually read. */
        intcs Read(bytecs buffer[], intcs offset, intcs count) override;
        /**
         * Writes count bytes from buffer at offset into the memory buffer.
         * @throws System::ArgumentNullException if buffer is null.
         * @throws System::ArgumentOutOfRangeException if offset or count is negative.
         */
        void  Write(const bytecs buffer[], intcs offset, intcs count) override;
        /** Writes a single byte to the memory buffer. */
        void  WriteByte(bytecs value) override;
        /**
         * @brief Marks this stream closed. Matches MemoryStream.Dispose(), which deliberately
         * leaves the buffer and position untouched so GetBuffer()/ToArray() keep working
         * afterward -- but Read/Write/Seek/Length/Position now correctly throw
         * System::ObjectDisposedException after this call, matching real .NET's _isOpen guard
         * (this port previously had no disposed-state tracking at all).
         */
        void  Close() override;
        /** No-op for MemoryStream; the buffer is always up to date. */
        void  Flush() override {}

        /** Returns the length of the in-memory buffer in bytes. */
        [[nodiscard]] intcs getLengthProperty()   const override;

        /**
         * @brief Returns true if the stream was created as writable.
         *
         * @note Deliberately does **not** fold in the disposed state, so a closed writable
         *       MemoryStream still reports true. That looks inconsistent beside
         *       getCanReadProperty() and getCanSeekProperty() below, and it is — but the
         *       inconsistency is .NET's own, not this port's: `MemoryStream.cs:103` is
         *       `public override bool CanWrite => _writable;` while `:99` and `:101` are both
         *       `=> _isOpen`. Ticket #1826 documented the asymmetry rather than silently
         *       normalising it, because matching .NET matters more than internal symmetry.
         */
        [[nodiscard]] bool  getCanWriteProperty() const override { return writable_; }

        /**
         * @brief Returns true if this stream can be read -- false once closed.
         *
         * Matches `MemoryStream.cs:99`, `public override bool CanRead => _isOpen;`.
         *
         * Before ticket #1826 this override did not exist, so MemoryStream inherited
         * Stream::getCanReadProperty()'s base default of `true` and kept claiming to be
         * readable after Close(). Two things followed, both measured in
         * build-probe/1826_prefix_defects.log: a caller that tested CanRead to decide whether
         * to read a disposed stream was told yes and then got ObjectDisposedException from
         * Read() (cases 2, 4); and a StreamReader over a closed MemoryStream was **accepted at
         * construction** and failed only on the first read (case 5), where .NET rejects it up
         * front with ArgumentException("Stream was not readable.") — which is exactly the
         * guard ticket #1808 added, defeated by this property lying to it.
         */
        [[nodiscard]] bool  getCanReadProperty()  const override { return isOpen_; }

        /**
         * @brief Returns the current read/write position within the buffer.
         * @throws System::ObjectDisposedException if this stream has been closed.
         */
        [[nodiscard]] intcs getPositionProperty() const override;
        /**
         * @brief Sets the current read/write position within the buffer.
         * @throws System::ArgumentOutOfRangeException if @p value is negative.
         * @throws System::ObjectDisposedException if this stream has been closed.
         */
        void setPositionProperty(intcs value) override;

        /**
         * @brief Returns true if this stream can seek -- false once closed.
         *
         * Matches real .NET's MemoryStream.CanSeek (`MemoryStream.cs:101`, `=> _isOpen`), which
         * returns false rather than throwing once the stream is disposed. This one was already
         * correct before ticket #1826; getCanReadProperty() above was the one that was not.
         */
        [[nodiscard]] bool getCanSeekProperty() const override { return isOpen_; }

        /** Resizes the underlying buffer to the given length, truncating or zero-extending it. */
        void SetLength(intcs value) override;

        /**
         * @brief Returns an independent copy of the buffer contents.
         *
         * C++ counterpart of .NET MemoryStream.ToArray(). Unlike GetBuffer(), the
         * returned vector is safe to hold onto across further writes to this stream.
         */
        [[nodiscard]] std::vector<bytecs> ToArray() const { return data_; }

        /**
         * @brief Returns a reference to the live internal buffer.
         *
         * C++ counterpart of .NET MemoryStream.GetBuffer(). The returned reference is
         * invalidated by any subsequent write that reallocates the buffer (matching
         * .NET's own caveat that the buffer may not reflect the stream's current length
         * without also checking getLengthProperty()).
         */
        [[nodiscard]] const std::vector<bytecs>& GetBuffer() const { return data_; }
    };
}
