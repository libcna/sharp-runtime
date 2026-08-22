// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/Directory.hpp"
#include "System/IO/DirectoryNotFoundException.hpp"
#include "System/IO/IOException.hpp"
#include "System/ArgumentException.hpp"
#include "System/UnauthorizedAccessException.hpp"

#include <filesystem>
#include <regex>

namespace System::IO {

    namespace {
        void ThrowIfNullOrEmpty(const std::string& path, const std::string& paramName) {
            if (path.empty()) throw System::ArgumentException("Path cannot be the empty string.", paramName);
        }

        [[noreturn]] void ThrowEnumerationError(const std::string& path,
                                                const std::error_code& error) {
            if (error == std::errc::permission_denied) {
                throw System::UnauthorizedAccessException(
                    "Access to the path '" + path + "' is denied.");
            }
            if (error == std::errc::no_such_file_or_directory ||
                error == std::errc::not_a_directory) {
                throw DirectoryNotFoundException(
                    "Could not find a part of the path '" + path + "'.");
            }
            throw IOException("Failed to enumerate directory '" + path + "': " +
                              error.message());
        }

        template <typename Visitor>
        void ForEachDirectoryEntry(const std::string& path, Visitor&& visitor) {
            std::error_code error;
            std::filesystem::directory_iterator iterator(path, error);
            if (error) ThrowEnumerationError(path, error);

            const std::filesystem::directory_iterator end;
            while (iterator != end) {
                visitor(*iterator);
                iterator.increment(error);
                if (error) ThrowEnumerationError(path, error);
            }
        }

        bool IsRegularFile(const std::filesystem::directory_entry& entry,
                           const std::string& directoryPath) {
            std::error_code error;
            const bool result = entry.is_regular_file(error);
            if (error) ThrowEnumerationError(directoryPath, error);
            return result;
        }

        bool IsDirectory(const std::filesystem::directory_entry& entry,
                         const std::string& directoryPath) {
            std::error_code error;
            const bool result = entry.is_directory(error);
            if (error) ThrowEnumerationError(directoryPath, error);
            return result;
        }
    }

    bool Directory::Exists(const std::string& path) {
        if (path.empty()) return false;
        std::error_code ec;
        bool isDir = std::filesystem::is_directory(path, ec);
        return !ec && isDir;
    }

    void Directory::CreateDirectory(const std::string& path) {
        ThrowIfNullOrEmpty(path, "path");
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec) throw IOException("Failed to create directory: " + ec.message());
    }

    void Directory::Delete(const std::string& path, bool recursive) {
        ThrowIfNullOrEmpty(path, "path");
        if (!Exists(path)) throw DirectoryNotFoundException("Could not find a part of the path '" + path + "'.");
        std::error_code ec;
        if (recursive) std::filesystem::remove_all(path, ec);
        else           std::filesystem::remove(path, ec);
        if (ec) throw IOException("Failed to delete directory: " + ec.message());
    }

    void Directory::Move(const std::string& src, const std::string& dst) {
        ThrowIfNullOrEmpty(src, "sourceDirName");
        ThrowIfNullOrEmpty(dst, "destDirName");
        if (!Exists(src)) throw DirectoryNotFoundException("Could not find a part of the path '" + src + "'.");
        // See File::Move's comment: real .NET's Move does not replace an existing destination.
        // std::filesystem::rename() would otherwise silently replace an empty destination
        // directory (or fail less clearly for a non-empty one) on POSIX.
        if (Exists(dst))
            throw IOException("Cannot create '" + dst + "' because a file or directory with the same name already exists.");
        std::error_code ec;
        std::filesystem::rename(src, dst, ec);
        if (ec) throw IOException("Failed to move directory: " + ec.message());
    }

    std::vector<std::string> Directory::GetFiles(const std::string& path) {
        std::vector<std::string> result;
        ForEachDirectoryEntry(path, [&](const std::filesystem::directory_entry& entry) {
            if (IsRegularFile(entry, path))
                result.push_back(entry.path().string());
        });
        return result;
    }

    std::vector<std::string> Directory::GetFiles(const std::string& path,
                                                  const std::string& searchPattern) {
        // Convert glob pattern (*.txt) to regex
        // Verified against FileSystemName.cs's TranslateWin32Expression: real .NET
        // special-cases "*.*" (legacy DOS 8.3 compatibility) to match every file, with or
        // without an extension -- a literal glob-to-regex translation of "*.*" instead demands
        // a literal '.' character in the filename, silently excluding every extensionless file
        // from the result.
        std::string pat = (searchPattern == "*.*") ? "*" : searchPattern;
        // Escape regex special chars except * and ?
        std::string regexPat;
        for (char c : pat) {
            if (c == '*') regexPat += ".*";
            else if (c == '?') regexPat += ".";
            else if (std::string(".+^${}[]|()\\").find(c) != std::string::npos)
                regexPat += std::string("\\") + c;
            else regexPat += c;
        }
        std::regex rx(regexPat, std::regex_constants::icase);
        std::vector<std::string> result;
        ForEachDirectoryEntry(path, [&](const std::filesystem::directory_entry& entry) {
            if (IsRegularFile(entry, path)) {
                std::string fname = entry.path().filename().string();
                if (std::regex_match(fname, rx))
                    result.push_back(entry.path().string());
            }
        });
        return result;
    }

    std::vector<std::string> Directory::GetDirectories(const std::string& path) {
        std::vector<std::string> result;
        ForEachDirectoryEntry(path, [&](const std::filesystem::directory_entry& entry) {
            if (IsDirectory(entry, path))
                result.push_back(entry.path().string());
        });
        return result;
    }

    std::string Directory::GetCurrentDirectory() {
        std::error_code error;
        const auto current = std::filesystem::current_path(error);
        if (error) {
            throw IOException("Failed to get current directory: " + error.message());
        }
        return current.string();
    }

    void Directory::SetCurrentDirectory(const std::string& path) {
        std::error_code ec;
        std::filesystem::current_path(path, ec);
        if (ec) throw IOException("Failed to set current directory: " + ec.message());
    }

} // namespace System::IO
