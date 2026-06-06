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
        explicit DirectoryInfo(const std::string& path) : path_(path) {}

        [[nodiscard]] std::string getNameProperty()     const { return path_.filename().string(); }
        [[nodiscard]] std::string getFullNameProperty() const { return std::filesystem::absolute(path_).string(); }

        [[nodiscard]] bool getExistsProperty() const { return std::filesystem::is_directory(path_); }

        [[nodiscard]] DirectoryInfo getParentProperty() const {
            return DirectoryInfo(path_.parent_path().string());
        }

        void Create() { std::filesystem::create_directories(path_); }

        void Delete(bool recursive = false) {
            if (recursive) std::filesystem::remove_all(path_);
            else           std::filesystem::remove(path_);
        }

        void MoveTo(const std::string& destDirName) {
            std::filesystem::rename(path_, destDirName);
            path_ = destDirName;
        }

        [[nodiscard]] std::vector<FileInfo> GetFiles() const {
            std::vector<FileInfo> result;
            for (auto& e : std::filesystem::directory_iterator(path_))
                if (e.is_regular_file()) result.emplace_back(e.path().string());
            return result;
        }

        [[nodiscard]] std::vector<std::string> GetFileNames() const {
            std::vector<std::string> result;
            for (auto& e : std::filesystem::directory_iterator(path_))
                if (e.is_regular_file()) result.push_back(e.path().string());
            return result;
        }

        [[nodiscard]] std::vector<DirectoryInfo> GetDirectories() const {
            std::vector<DirectoryInfo> result;
            for (auto& e : std::filesystem::directory_iterator(path_))
                if (e.is_directory()) result.emplace_back(e.path().string());
            return result;
        }
    };

} // namespace System::IO
