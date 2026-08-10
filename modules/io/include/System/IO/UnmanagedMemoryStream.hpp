// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/FileAccess.hpp"
#include "System/IO/Stream.hpp"

namespace System::IO {

    /**
     * @brief Provides access to unmanaged blocks of memory from managed code.
     *
     * Lightweight C++ counterpart of .NET System.IO.UnmanagedMemoryStream: wraps a raw,
     * caller-owned byte buffer (in place of .NET's SafeBuffer/unsafe byte* + Initialize
     * overloads) as a Stream. The stream never owns or frees the buffer.
     *
     * @note Status: PARTIAL
     */
    class UnmanagedMemoryStream : public Stream {
    private:
        bytecs*    buffer_   = nullptr;
        intcs      length_   = 0;
        intcs      capacity_ = 0;
        intcs      position_ = 0;
        FileAccess access_   = FileAccess::Read;
        bool       isOpen_   = false;

    protected:
        /** Initializes a stream with no backing buffer; derived types must call Initialize(). */
        UnmanagedMemoryStream() = default;

        /** Initializes the stream over @p pointer with the given length, capacity, and access. */
        void Initialize(bytecs* pointer, intcs length, intcs capacity, FileAccess access);

    private:
        /**
         * @brief Throws if the stream has been closed.
         *
         * Ticket #2108 (SR-AUD-344, cause I-A). `isOpen_` and `Close()` already existed and
         * `Read`/`Write`/`SetLength` already consulted them; six other public members did not,
         * so a closed stream still reported its length, still accepted a position mutation, and
         * still handed out a raw pointer into its buffer. This is the single door they now all
         * go through, so they cannot drift apart again.
         *
         * It is checked BEFORE any argument-domain check, which is the order `Read` and `Write`
         * already use and the order this file's transcribed note records for .NET's
         * `EnsureNotClosed()`.
         */
        void EnsureNotClosed() const;

    public:
        /** Constructs a read-only stream over @p pointer spanning @p length bytes. */
        UnmanagedMemoryStream(bytecs* pointer, intcs length);
        /** Constructs a stream over @p pointer with the given length, capacity, and access. */
        UnmanagedMemoryStream(bytecs* pointer, intcs length, intcs capacity, FileAccess access);

        ~UnmanagedMemoryStream() override = default;

        /**
         * @brief Reads up to count bytes into buffer at offset; returns bytes actually read.
         * @throws System::ObjectDisposedException if the stream has been closed.
         * @throws System::ArgumentNullException if @p buffer is null.
         * @throws System::ArgumentOutOfRangeException if @p offset or @p count is negative.
         */
        intcs Read(bytecs buffer[], intcs offset, intcs count) override;
        /**
         * @brief Writes count bytes from buffer at offset into the unmanaged buffer.
         * @throws System::ObjectDisposedException if the stream has been closed.
         * @throws System::ArgumentNullException if @p buffer is null.
         * @throws System::ArgumentOutOfRangeException if @p offset or @p count is negative.
         * @throws System::NotSupportedException if the write would exceed the stream's fixed capacity.
         */
        void  Write(const bytecs buffer[], intcs offset, intcs count) override;
        /** Writes a single byte to the unmanaged buffer. */
        void  WriteByte(bytecs value) override;
        /** Marks the stream as closed. Calling it twice is safe. */
        void  Close() override { isOpen_ = false; }
        /**
         * @brief No-op; there is no underlying device to flush to.
         * @throws System::ObjectDisposedException if the stream has been closed.
         */
        void  Flush() override { EnsureNotClosed(); }

        /**
         * @brief Returns the number of valid bytes in the stream.
         * @throws System::ObjectDisposedException if the stream has been closed.
         */
        [[nodiscard]] intcs getLengthProperty()   const override { EnsureNotClosed(); return length_; }
        /**
         * @brief Returns the total size of the backing buffer, in bytes.
         * @throws System::ObjectDisposedException if the stream has been closed.
         */
        [[nodiscard]] intcs getCapacityProperty() const { EnsureNotClosed(); return capacity_; }
        /** Returns true if the stream supports reading. */
        [[nodiscard]] bool  getCanReadProperty()  const override { return isOpen_ && (access_ & FileAccess::Read)  == FileAccess::Read; }
        /** Returns true if the stream supports writing. */
        [[nodiscard]] bool  getCanWriteProperty() const override { return isOpen_ && (access_ & FileAccess::Write) == FileAccess::Write; }
        /** UnmanagedMemoryStream always supports seeking while open. */
        [[nodiscard]] bool  getCanSeekProperty()  const override { return isOpen_; }

        /**
         * @brief Returns the current read/write position within the buffer.
         * @throws System::ObjectDisposedException if the stream has been closed.
         */
        [[nodiscard]] intcs getPositionProperty() const override { EnsureNotClosed(); return position_; }
        /**
         * @brief Sets the current read/write position within the buffer.
         * @throws System::ObjectDisposedException if the stream has been closed.
         * @throws System::ArgumentOutOfRangeException if @p value is negative.
         */
        void setPositionProperty(intcs value) override;

        /**
         * @brief Resizes the valid length of the stream; must not exceed Capacity.
         * @throws System::ObjectDisposedException if the stream has been closed.
         */
        void SetLength(intcs value) override;

        /**
         * @brief Returns a raw pointer to the current position within the buffer.
         * @throws System::ObjectDisposedException if the stream has been closed. Until #2108
         * this handed out a live pointer into the buffer of a stream that had been closed --
         * the sharpest member of SR-AUD-344, because the result outlives the only signal the
         * caller had that the stream was finished with.
         */
        [[nodiscard]] bytecs* getPositionPointerProperty() const { EnsureNotClosed(); return buffer_ + position_; }
    };

} // namespace System::IO
