// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/Directory.hpp"
#include "System/IO/IOException.hpp"

#include <filesystem>
#include <regex>

namespace System::IO {

    bool Directory::Exists(const std::string& path) {
        return std::filesystem::exists(path) && std::filesystem::is_directory(path);
    }

    void Directory::CreateDirectory(const std::string& path) {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec) throw IOException("Failed to create directory: " + ec.message());
    }

    void Directory::Delete(const std::string& path, bool recursive) {
        std::error_code ec;
        if (recursive) std::filesystem::remove_all(path, ec);
        else           std::filesystem::remove(path, ec);
        if (ec) throw IOException("Failed to delete directory: " + ec.message());
    }

    void Directory::Move(const std::string& src, const std::string& dst) {
        std::error_code ec;
        std::filesystem::rename(src, dst, ec);
        if (ec) throw IOException("Failed to move directory: " + ec.message());
    }

    std::vector<std::string> Directory::GetFiles(const std::string& path) {
        std::vector<std::string> result;
        for (const auto& entry : std::filesystem::directory_iterator(path))
            if (entry.is_regular_file())
                result.push_back(entry.path().string());
        return result;
    }

    std::vector<std::string> Directory::GetFiles(const std::string& path,
                                                  const std::string& searchPattern) {
        // Convert glob pattern (*.txt) to regex
        std::string pat = searchPattern;
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
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string fname = entry.path().filename().string();
                if (std::regex_match(fname, rx))
                    result.push_back(entry.path().string());
            }
        }
        return result;
    }

    std::vector<std::string> Directory::GetDirectories(const std::string& path) {
        std::vector<std::string> result;
        for (const auto& entry : std::filesystem::directory_iterator(path))
            if (entry.is_directory())
                result.push_back(entry.path().string());
        return result;
    }

    std::string Directory::GetCurrentDirectory() {
        return std::filesystem::current_path().string();
    }

    void Directory::SetCurrentDirectory(const std::string& path) {
        std::error_code ec;
        std::filesystem::current_path(path, ec);
        if (ec) throw IOException("Failed to set current directory: " + ec.message());
    }

} // namespace System::IO
