// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "System/IO/FileInfo.hpp"

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

        /** Returns true if the directory exists. */
        [[nodiscard]] bool getExistsProperty() const { return std::filesystem::is_directory(path_); }

        /** Returns the parent directory. */
        [[nodiscard]] DirectoryInfo getParentProperty() const {
            return DirectoryInfo(path_.parent_path().string());
        }

        /** Creates the directory and all intermediate directories. */
        void Create() { std::filesystem::create_directories(path_); }

        /** Deletes this directory; deletes all contents recursively when recursive is true. */
        void Delete(bool recursive = false) {
            if (recursive) std::filesystem::remove_all(path_);
            else           std::filesystem::remove(path_);
        }

        /** Moves the directory to destDirName. */
        void MoveTo(const std::string& destDirName) {
            std::filesystem::rename(path_, destDirName);
            path_ = destDirName;
        }

        /** Returns a list of FileInfo objects for the files in this directory. */
        [[nodiscard]] std::vector<FileInfo> GetFiles() const {
            std::vector<FileInfo> result;
            for (auto& e : std::filesystem::directory_iterator(path_))
                if (e.is_regular_file()) result.emplace_back(e.path().string());
            return result;
        }

        /** Returns a list of file paths for the files in this directory. */
        [[nodiscard]] std::vector<std::string> GetFileNames() const {
            std::vector<std::string> result;
            for (auto& e : std::filesystem::directory_iterator(path_))
                if (e.is_regular_file()) result.push_back(e.path().string());
            return result;
        }

        /** Returns a list of DirectoryInfo objects for the subdirectories of this directory. */
        [[nodiscard]] std::vector<DirectoryInfo> GetDirectories() const {
            std::vector<DirectoryInfo> result;
            for (auto& e : std::filesystem::directory_iterator(path_))
                if (e.is_directory()) result.emplace_back(e.path().string());
            return result;
        }
    };

} // namespace System::IO
