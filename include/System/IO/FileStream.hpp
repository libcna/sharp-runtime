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
        FileMode     mode_;
        intcs        length_;
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
         */
        FileStream(const std::string& path, FileMode mode, FileAccess access);

        /** Destroys the FileStream and closes the file. */
        ~FileStream() override;

        /** Reads up to count bytes into buffer starting at offset; returns bytes actually read. */
        intcs Read(bytecs buffer[], intcs offset, intcs count) override;
        /** Writes count bytes from buffer starting at offset into the file. */
        void  Write(const bytecs buffer[], intcs offset, intcs count) override;
        /** Writes a single byte to the file. */
        void  WriteByte(bytecs value) override;
        /** Closes the file stream. */
        void  Close() override;
        /** Flushes any buffered data to the file. */
        void  Flush() override;

        /** Returns the length of the file in bytes. */
        [[nodiscard]] intcs getLengthProperty() const override;
        /** Returns true if the stream supports writing. */
        [[nodiscard]] bool  getCanWriteProperty() const override { return canWrite_; }
        /** Returns true if the stream supports reading. */
        [[nodiscard]] bool  getCanReadProperty()  const override { return !canWrite_ || mode_ != FileMode::Append; }
        /** Returns true if the underlying file is open. */
        [[nodiscard]] bool  IsOpen() const;
    };
}