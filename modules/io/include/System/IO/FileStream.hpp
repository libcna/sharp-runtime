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
        /** Flushes any buffered data to the file. */
        void  Flush() override;

        /** Returns the length of the file in bytes. */
        [[nodiscard]] intcs getLengthProperty() const override;
        /** Returns true if the stream was opened with write access. */
        [[nodiscard]] bool  getCanWriteProperty() const override { return canWrite_; }
        /** Returns true if the stream was opened with read access. */
        [[nodiscard]] bool  getCanReadProperty()  const override { return canRead_; }
        /** Returns true if the underlying file is open. */
        [[nodiscard]] bool  IsOpen() const;

        /** Returns true if the underlying file is open (FileStream always supports seeking while open). */
        [[nodiscard]] bool  getCanSeekProperty() const override { return file_.is_open(); }
        /** Returns the current read/write position within the file. */
        [[nodiscard]] intcs getPositionProperty() const override;
        /** Sets the current read/write position within the file. Throws ArgumentOutOfRangeException if negative. */
        void setPositionProperty(intcs value) override;
        /** Truncates or extends the file to the given length. */
        void SetLength(intcs value) override;
    };
}
