// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "System/IO/DirectoryNotFoundException.hpp"
#include "System/IO/FileInfo.hpp"
#include "System/IO/IOException.hpp"

namespace System::IO {

    /**
     * @brief Exposes instance methods for creating, moving, and enumerating through directories and subdirectories.
     *
     * Wraps std::filesystem. Partial C++ counterpart of .NET System.IO.DirectoryInfo.
     *
     * @note Status: Partial
     */
    class DirectoryInfo {
        std::filesystem::path path_;
    public:
        /** Constructs a DirectoryInfo for the specified path. */
        explicit DirectoryInfo(const std::string& path) : path_(path) {}

        /** Returns the last component of the directory path. */
        [[nodiscard]] std::string getNameProperty()     const { return path_.filename().string(); }
        /** Returns the full absolute path of the directory. */
        [[nodiscard]] std::string getFullNameProperty() const { return std::filesystem::absolute(path_).string(); }

        /** Returns true if the directory exists. Never throws, matching .NET. */
        [[nodiscard]] bool getExistsProperty() const {
            std::error_code ec;
            bool isDir = std::filesystem::is_directory(path_, ec);
            return !ec && isDir;
        }

        /** Returns the parent directory. */
        [[nodiscard]] DirectoryInfo getParentProperty() const {
            return DirectoryInfo(path_.parent_path().string());
        }

        /** Creates the directory and all intermediate directories. */
        void Create() {
            std::error_code ec;
            std::filesystem::create_directories(path_, ec);
            if (ec) throw IOException("Failed to create directory: " + ec.message());
        }

        /**
         * @brief Deletes this directory; deletes all contents recursively when recursive is true.
         * @throws System::IO::DirectoryNotFoundException if the directory does not exist.
         */
        void Delete(bool recursive = false) {
            if (!getExistsProperty())
                throw DirectoryNotFoundException("Could not find a part of the path '" + path_.string() + "'.");
            std::error_code ec;
            if (recursive) std::filesystem::remove_all(path_, ec);
            else           std::filesystem::remove(path_, ec);
            if (ec) throw IOException("Failed to delete directory: " + ec.message());
        }

        /**
         * @brief Moves the directory to destDirName.
         * @throws System::IO::DirectoryNotFoundException if this directory does not exist.
         */
        void MoveTo(const std::string& destDirName) {
            if (!getExistsProperty())
                throw DirectoryNotFoundException("Could not find a part of the path '" + path_.string() + "'.");
            std::error_code ec;
            std::filesystem::rename(path_, destDirName, ec);
            if (ec) throw IOException("Failed to move directory: " + ec.message());
            path_ = destDirName;
        }

        /**
         * @brief Returns a list of FileInfo objects for the files in this directory.
         * @throws System::IO::DirectoryNotFoundException if this directory does not exist.
         */
        [[nodiscard]] std::vector<FileInfo> GetFiles() const {
            if (!getExistsProperty())
                throw DirectoryNotFoundException("Could not find a part of the path '" + path_.string() + "'.");
            std::vector<FileInfo> result;
            for (auto& e : std::filesystem::directory_iterator(path_))
                if (e.is_regular_file()) result.emplace_back(e.path().string());
            return result;
        }

        /**
         * @brief Returns a list of file paths for the files in this directory.
         * @throws System::IO::DirectoryNotFoundException if this directory does not exist.
         */
        [[nodiscard]] std::vector<std::string> GetFileNames() const {
            if (!getExistsProperty())
                throw DirectoryNotFoundException("Could not find a part of the path '" + path_.string() + "'.");
            std::vector<std::string> result;
            for (auto& e : std::filesystem::directory_iterator(path_))
                if (e.is_regular_file()) result.push_back(e.path().string());
            return result;
        }

        /**
         * @brief Returns a list of DirectoryInfo objects for the subdirectories of this directory.
         * @throws System::IO::DirectoryNotFoundException if this directory does not exist.
         */
        [[nodiscard]] std::vector<DirectoryInfo> GetDirectories() const {
            if (!getExistsProperty())
                throw DirectoryNotFoundException("Could not find a part of the path '" + path_.string() + "'.");
            std::vector<DirectoryInfo> result;
            for (auto& e : std::filesystem::directory_iterator(path_))
                if (e.is_directory()) result.emplace_back(e.path().string());
            return result;
        }
    };

} // namespace System::IO
