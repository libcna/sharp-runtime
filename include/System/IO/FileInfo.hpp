// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <filesystem>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/IO/FileNotFoundException.hpp"
#include "System/IO/FileSystemInfo.hpp"
#include "System/IO/IOException.hpp"

namespace System::IO {

    using SharpRuntime::longcs;

    /**
     * @brief Provides instance methods for the creation, copying, deletion, moving, and opening of files.
     *
     * Wraps std::filesystem. Partial C++ counterpart of .NET System.IO.FileInfo.
     *
     * @note Status: Partial
     */
    class FileInfo : public FileSystemInfo {
    public:
        /** Constructs a FileInfo for the specified path. */
        explicit FileInfo(const std::string& path) : FileSystemInfo(path) {}

        /** Returns the file name and extension. */
        [[nodiscard]] std::string getNameProperty()      const override { return fullPath_.filename().string(); }
        /** Returns the directory portion of the full path. */
        [[nodiscard]] std::string getDirectoryNameProperty() const { return fullPath_.parent_path().string(); }

        /** Returns true if the file exists. Never throws, matching .NET. */
        [[nodiscard]] bool getExistsProperty() const override {
            std::error_code ec;
            bool isFile = std::filesystem::is_regular_file(fullPath_, ec);
            return !ec && isFile;
        }

        /** Returns the file size in bytes, or -1 if an error occurs. */
        [[nodiscard]] longcs getLengthProperty() const {
            std::error_code ec;
            auto sz = std::filesystem::file_size(fullPath_, ec);
            return ec ? -1LL : static_cast<longcs>(sz);
        }

        /**
         * @brief Returns true if the file does not have write permission for its owner.
         * Returns false (rather than throwing) if the file does not exist or its status
         * cannot be determined.
         */
        [[nodiscard]] bool getIsReadOnlyProperty() const {
            std::error_code ec;
            auto status = std::filesystem::status(fullPath_, ec);
            if (ec) return false;
            auto perms = status.permissions();
            return (perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none;
        }

        /**
         * @brief Permanently deletes the file.
         *
         * Matches .NET: deleting a file that does not exist is not an error.
         * @throws System::IO::IOException on a genuine deletion failure.
         */
        void Delete() override {
            std::error_code ec;
            std::filesystem::remove(fullPath_, ec);
            if (ec) throw IOException("Failed to delete file '" + fullPath_.string() + "': " + ec.message());
        }

        /**
         * @brief Copies the file to destFileName; overwrites if overwrite is true.
         * @throws System::ArgumentException if @p destFileName is empty.
         * @throws System::IO::FileNotFoundException if this file does not exist.
         * @throws System::IO::IOException on a genuine copy failure.
         */
        void CopyTo(const std::string& destFileName, bool overwrite = false) {
            if (destFileName.empty())
                throw System::ArgumentException("Path cannot be the empty string.", "destFileName");
            if (!getExistsProperty())
                throw FileNotFoundException("Could not find file '" + fullPath_.string() + "'.", fullPath_.string());
            auto opts = overwrite
                ? std::filesystem::copy_options::overwrite_existing
                : std::filesystem::copy_options::none;
            std::error_code ec;
            std::filesystem::copy_file(fullPath_, destFileName, opts, ec);
            if (ec) throw IOException("Failed to copy file '" + fullPath_.string() + "' to '" + destFileName + "': " + ec.message());
        }

        /**
         * @brief Moves the file to destFileName, updating the internal path.
         * @throws System::ArgumentException if @p destFileName is empty.
         * @throws System::IO::FileNotFoundException if this file does not exist.
         * @throws System::IO::IOException on a genuine move failure.
         */
        void MoveTo(const std::string& destFileName) {
            if (destFileName.empty())
                throw System::ArgumentException("Path cannot be the empty string.", "destFileName");
            if (!getExistsProperty())
                throw FileNotFoundException("Could not find file '" + fullPath_.string() + "'.", fullPath_.string());
            std::error_code ec;
            std::filesystem::rename(fullPath_, destFileName, ec);
            if (ec) throw IOException("Failed to move file '" + fullPath_.string() + "' to '" + destFileName + "': " + ec.message());
            fullPath_ = std::filesystem::absolute(destFileName);
            originalPath_ = destFileName;
        }

        /** Copies the file to destFileName and returns a FileInfo for the destination. */
        FileInfo CopyToInfo(const std::string& destFileName, bool overwrite = false) {
            CopyTo(destFileName, overwrite);
            return FileInfo(destFileName);
        }
    };

} // namespace System::IO
