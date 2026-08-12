// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <fstream>
#include <string>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/Stream.hpp"

namespace System::IO
{
    /**
     * @brief Represents a file-backed stream supporting read and write.
     *
     * @note Status: Implemented
     */
    class FileStream : public Stream
    {
    private:
        std::fstream file_;
        std::string  path_;
        intcs        length_;
        bool         canRead_;
        bool         canWrite_;

        /**
         * @brief Throws if the file has been closed.
         *
         * Ticket #2099 (SR-AUD-342, cause I-A). `FileStream` needs no `closed_` flag of its
         * own: `file_.is_open()` already *is* the closed state, `Close()` already clears it,
         * and the three capability properties already consult it. That is why this repair is
         * layout-neutral and did not have to wait for #2098's storage decision (Approval
         * IO-1) -- the same reason #2108 split `UnmanagedMemoryStream` out of #2098.
         *
         * Every member that depends on the closed file routes through here, including the
         * four that were already correct, so they cannot drift apart again. It is checked
         * BEFORE any argument-domain check, which is the order `Read` and `Write` already use
         * and the order this file's transcribed .NET note records.
         */
        void EnsureNotClosed() const;

    public:
        /**
         * @brief Opens a file for reading (FileMode::Open).
         */
        explicit FileStream(const std::string& path);

        /**
         * @brief Opens or creates a file with the specified FileMode.
         */
        FileStream(const std::string& path, FileMode mode);

        /**
         * @brief Opens or creates a file with the specified FileMode and FileAccess.
         * @throws System::ArgumentException if @p access includes Read with FileMode::Append, or
         *         if @p access excludes Write with a mode that requires write access
         *         (Truncate, CreateNew, Create, Append).
         * @throws System::IO::FileNotFoundException if @p mode is Open or Truncate and the file
         *         does not exist.
         * @throws System::IO::IOException if @p mode is CreateNew and the file already exists,
         *         or on another genuine I/O failure.
         */
        FileStream(const std::string& path, FileMode mode, FileAccess access);

        /** Destroys the FileStream and closes the file. */
        ~FileStream() override;

        /**
         * @brief Reads up to count bytes into buffer starting at offset; returns bytes actually read.
         *
         * Validated in .NET's order (Strategies/OSFileStreamStrategy.cs:208-217): the
         * closed state first, the access flags second, so a stream that is both closed
         * and unreadable reports ObjectDisposedException.
         *
         * @throws System::ObjectDisposedException if the file has been closed.
         * @throws System::NotSupportedException if this stream was not opened with read access.
         * @throws System::ArgumentNullException if @p buffer is null.
         * @throws System::ArgumentOutOfRangeException if @p offset or @p count is negative.
         */
        intcs Read(bytecs buffer[], intcs offset, intcs count) override;
        /**
         * @brief Writes count bytes from buffer starting at offset into the file.
         *
         * Validated in .NET's order (Strategies/OSFileStreamStrategy.cs:232-241): the
         * closed state first, the access flags second. Without the access check the
         * underlying std::fstream accepted the call, set badbit and dropped the bytes,
         * so a write to a read-only handle was lost in silence.
         *
         * @throws System::ObjectDisposedException if the file has been closed.
         * @throws System::NotSupportedException if this stream was not opened with write access.
         * @throws System::ArgumentNullException if @p buffer is null.
         * @throws System::ArgumentOutOfRangeException if @p offset or @p count is negative.
         */
        void  Write(const bytecs buffer[], intcs offset, intcs count) override;
        /**
         * @brief Writes a single byte to the file.
         *
         * @throws System::ObjectDisposedException if the file has been closed.
         * @throws System::NotSupportedException if this stream was not opened with write access.
         */
        void  WriteByte(bytecs value) override;
        /** Closes the file stream. */
        void  Close() override;
        /**
         * @brief Flushes any buffered data to the file.
         * @throws System::ObjectDisposedException if the file has been closed.
         */
        void  Flush() override;

        /**
         * @brief Returns the length of the file in bytes.
         * @throws System::ObjectDisposedException if the file has been closed.
         */
        [[nodiscard]] intcs getLengthProperty() const override;
        /**
         * @brief Returns true if the stream is open with write access -- false once closed.
         *
         * Folds in file_.is_open() as well as the requested access, matching real .NET's
         * `OSFileStreamStrategy.CanWrite => !_fileHandle.IsClosed && (_access & FileAccess.Write)
         * != 0` (Strategies/OSFileStreamStrategy.cs:75), reached through FileStream.CanWrite
         * (FileStream.cs:389). Ticket #1842: before this, the override returned the bare
         * canWrite_, which reflects the access mode requested at construction and is never reset
         * by Close(), so a closed writable FileStream still claimed CanWrite == true -- the same
         * stale-capability shape #1826 fixed in MemoryStream. getCanSeekProperty() already folded
         * the open state in; getCanReadProperty()/getCanWriteProperty() did not. Unlike
         * MemoryStream (whose .NET CanWrite is `=> _writable` and stays true after close, #1826),
         * .NET FileStream folds the closed state into BOTH directions, so both do here.
         */
        [[nodiscard]] bool  getCanWriteProperty() const override { return canWrite_ && file_.is_open(); }
        /**
         * @brief Returns true if the stream is open with read access -- false once closed.
         *
         * Folds in file_.is_open(), matching `OSFileStreamStrategy.CanRead => !_fileHandle.IsClosed
         * && (_access & FileAccess.Read) != 0` (Strategies/OSFileStreamStrategy.cs:73). See
         * getCanWriteProperty() above for the ticket #1842 rationale; the two are symmetric for
         * FileStream, matching .NET.
         */
        [[nodiscard]] bool  getCanReadProperty()  const override { return canRead_ && file_.is_open(); }
        /** Returns true if the underlying file is open. */
        [[nodiscard]] bool  IsOpen() const;

        /** Returns true if the underlying file is open (FileStream always supports seeking while open). */
        [[nodiscard]] bool  getCanSeekProperty() const override { return file_.is_open(); }
        /**
         * @brief Returns the current read/write position within the file.
         *
         * Until #2099 a closed stream returned the sentinel -1 here, which `Stream::Seek`
         * then turned into an `IOException` for `SeekOrigin::Current` -- a wrong diagnostic
         * for a closed stream, and the reason `Seek` is not listed as "succeeds outright"
         * for all three origins.
         *
         * @throws System::ObjectDisposedException if the file has been closed.
         */
        [[nodiscard]] intcs getPositionProperty() const override;
        /**
         * @brief Sets the current read/write position within the file.
         *
         * `FileStream` does not override `Seek`; the inherited `Stream::Seek` is expressed in
         * terms of this setter and of `getLengthProperty()`, so all three origins report a
         * closed stream through them. See `Stream::Seek` for the one residual ordering case.
         *
         * @throws System::ObjectDisposedException if the file has been closed, checked before
         *         the argument-domain test.
         * @throws System::ArgumentOutOfRangeException if @p value is negative.
         */
        void setPositionProperty(intcs value) override;
        /**
         * @brief Truncates or extends the file to the given length.
         * @throws System::ObjectDisposedException if the file has been closed, checked before
         *         the argument-domain test.
         * @throws System::ArgumentOutOfRangeException if @p value is negative.
         * @throws System::NotSupportedException if this stream was not opened with write access.
         */
        void SetLength(intcs value) override;
    };
}
