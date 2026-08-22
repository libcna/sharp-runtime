// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "System/IO/DirectoryNotFoundException.hpp"
#include "System/IO/FileInfo.hpp"
#include "System/IO/FileSystemInfo.hpp"
#include "System/IO/IOException.hpp"
#include "System/UnauthorizedAccessException.hpp"

namespace System::IO {

    /**
     * @brief Exposes instance methods for creating, moving, and enumerating through directories and subdirectories.
     *
     * Wraps std::filesystem. Partial C++ counterpart of .NET System.IO.DirectoryInfo.
     *
     * @note Status: Partial — covers Create/Delete/MoveTo/GetFiles/GetDirectories and the common
     * FileSystemInfo properties. Not ported: CreateSubdirectory, the Enumerate*
     * (EnumerateFiles/EnumerateDirectories/EnumerateFileSystemInfos) streaming variants,
     * GetFileSystemInfos, the Root property, and searchPattern/SearchOption-based overloads of
     * GetFiles/GetDirectories. GetFileNames() is a port-added convenience method with no real
     * .NET equivalent (real .NET's GetFiles() already returns FileInfo[], not paths).
     */
    class DirectoryInfo : public FileSystemInfo {
        [[noreturn]] static void throwEnumerationError(const std::filesystem::path& path,
                                                       const std::error_code& error) {
            if (error == std::errc::permission_denied) {
                throw System::UnauthorizedAccessException(
                    "Access to the path '" + path.string() + "' is denied.");
            }
            if (error == std::errc::no_such_file_or_directory ||
                error == std::errc::not_a_directory) {
                throw DirectoryNotFoundException(
                    "Could not find a part of the path '" + path.string() + "'.");
            }
            throw IOException("Failed to enumerate directory '" + path.string() + "': " +
                              error.message());
        }

        template <typename Visitor>
        void forEachEntry(Visitor&& visitor) const {
            std::error_code error;
            std::filesystem::directory_iterator iterator(fullPath_, error);
            if (error) throwEnumerationError(fullPath_, error);

            const std::filesystem::directory_iterator end;
            while (iterator != end) {
                visitor(*iterator);
                iterator.increment(error);
                if (error) throwEnumerationError(fullPath_, error);
            }
        }

        [[nodiscard]] bool isRegularFile(const std::filesystem::directory_entry& entry) const {
            std::error_code error;
            const bool result = entry.is_regular_file(error);
            if (error) throwEnumerationError(fullPath_, error);
            return result;
        }

        [[nodiscard]] bool isDirectory(const std::filesystem::directory_entry& entry) const {
            std::error_code error;
            const bool result = entry.is_directory(error);
            if (error) throwEnumerationError(fullPath_, error);
            return result;
        }

    public:
        /** Constructs a DirectoryInfo for the specified path. */
        explicit DirectoryInfo(const std::string& path) : FileSystemInfo(path) {}

        /** Returns the last component of the directory path. */
        [[nodiscard]] std::string getNameProperty() const override { return fullPath_.filename().string(); }

        /** Returns true if the directory exists. Never throws, matching .NET. */
        [[nodiscard]] bool getExistsProperty() const override {
            std::error_code ec;
            bool isDir = std::filesystem::is_directory(fullPath_, ec);
            return !ec && isDir;
        }

        /** Returns the parent directory. */
        [[nodiscard]] DirectoryInfo getParentProperty() const {
            return DirectoryInfo(fullPath_.parent_path().string());
        }

        /** Creates the directory and all intermediate directories. */
        void Create() {
            std::error_code ec;
            std::filesystem::create_directories(fullPath_, ec);
            if (ec) throw IOException("Failed to create directory: " + ec.message());
        }

        /**
         * @brief Deletes this directory; deletes all contents recursively when recursive is true.
         * @throws System::IO::DirectoryNotFoundException if the directory does not exist.
         */
        void Delete(bool recursive) {
            if (!getExistsProperty())
                throw DirectoryNotFoundException("Could not find a part of the path '" + fullPath_.string() + "'.");
            std::error_code ec;
            if (recursive) std::filesystem::remove_all(fullPath_, ec);
            else           std::filesystem::remove(fullPath_, ec);
            if (ec) throw IOException("Failed to delete directory: " + ec.message());
        }

        /**
         * @brief Deletes this directory (non-recursive).
         * @throws System::IO::DirectoryNotFoundException if the directory does not exist.
         */
        void Delete() override { Delete(false); }

        /**
         * @brief Moves the directory to destDirName.
         * @throws System::IO::DirectoryNotFoundException if this directory does not exist.
         */
        void MoveTo(const std::string& destDirName) {
            if (!getExistsProperty())
                throw DirectoryNotFoundException("Could not find a part of the path '" + fullPath_.string() + "'.");
            std::error_code ec;
            const auto destinationPath = std::filesystem::absolute(destDirName, ec);
            if (ec)
                throw IOException("Failed to resolve destination directory '" + destDirName +
                                  "': " + ec.message());
            // Real .NET's MoveTo does not replace an existing destination directory.
            const bool destinationExists = std::filesystem::exists(destinationPath, ec);
            if (ec) {
                if (ec == std::errc::permission_denied)
                    throw System::UnauthorizedAccessException(
                        "Access to the path '" + destDirName + "' is denied.");
                throw IOException("Failed to inspect destination directory '" + destDirName +
                                  "': " + ec.message());
            }
            if (destinationExists)
                throw IOException("Cannot create '" + destDirName + "' because a file or directory with the same name already exists.");
            std::filesystem::rename(fullPath_, destinationPath, ec);
            if (ec) throw IOException("Failed to move directory: " + ec.message());
            fullPath_ = destinationPath;
            originalPath_ = destDirName;
        }

        /**
         * @brief Returns a list of FileInfo objects for the files in this directory.
         * @throws System::IO::DirectoryNotFoundException if this directory does not exist.
         * @throws System::UnauthorizedAccessException if enumeration is denied.
         * @throws System::IO::IOException for another filesystem enumeration failure.
         */
        [[nodiscard]] std::vector<FileInfo> GetFiles() const {
            std::vector<FileInfo> result;
            forEachEntry([&](const std::filesystem::directory_entry& entry) {
                if (isRegularFile(entry)) result.emplace_back(entry.path().string());
            });
            return result;
        }

        /**
         * @brief Returns a list of file paths for the files in this directory.
         * @throws System::IO::DirectoryNotFoundException if this directory does not exist.
         * @throws System::UnauthorizedAccessException if enumeration is denied.
         * @throws System::IO::IOException for another filesystem enumeration failure.
         */
        [[nodiscard]] std::vector<std::string> GetFileNames() const {
            std::vector<std::string> result;
            forEachEntry([&](const std::filesystem::directory_entry& entry) {
                if (isRegularFile(entry)) result.push_back(entry.path().string());
            });
            return result;
        }

        /**
         * @brief Returns a list of DirectoryInfo objects for the subdirectories of this directory.
         * @throws System::IO::DirectoryNotFoundException if this directory does not exist.
         * @throws System::UnauthorizedAccessException if enumeration is denied.
         * @throws System::IO::IOException for another filesystem enumeration failure.
         */
        [[nodiscard]] std::vector<DirectoryInfo> GetDirectories() const {
            std::vector<DirectoryInfo> result;
            forEachEntry([&](const std::filesystem::directory_entry& entry) {
                if (isDirectory(entry)) result.emplace_back(entry.path().string());
            });
            return result;
        }
    };

} // namespace System::IO
