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

        ~FileStream() override;

        intcs Read(bytecs buffer[], intcs offset, intcs count) override;
        void  Write(const bytecs buffer[], intcs offset, intcs count) override;
        void  WriteByte(bytecs value) override;
        void  Close() override;
        void  Flush() override;

        [[nodiscard]] intcs getLengthProperty() const override;
        [[nodiscard]] bool  getCanWriteProperty() const override { return canWrite_; }
        [[nodiscard]] bool  getCanReadProperty()  const override { return !canWrite_ || mode_ != FileMode::Append; }
        [[nodiscard]] bool  IsOpen() const;
    };
}