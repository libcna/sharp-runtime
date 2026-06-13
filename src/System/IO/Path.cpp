// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/Path.hpp"
#include "System/IO/IOException.hpp"

#include <filesystem>
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <unistd.h>
#endif

namespace System::IO {

    std::string Path::Combine(const std::string& path1, const std::string& path2) {
        return (std::filesystem::path(path1) / path2).string();
    }

    std::string Path::Combine(const std::string& path1, const std::string& path2,
                               const std::string& path3) {
        return (std::filesystem::path(path1) / path2 / path3).string();
    }

    std::string Path::GetFileName(const std::string& path) {
        return std::filesystem::path(path).filename().string();
    }

    std::string Path::GetFileNameWithoutExtension(const std::string& path) {
        return std::filesystem::path(path).stem().string();
    }

    std::string Path::GetExtension(const std::string& path) {
        return std::filesystem::path(path).extension().string();
    }

    std::string Path::GetDirectoryName(const std::string& path) {
        auto p = std::filesystem::path(path).parent_path();
        return p.empty() ? "" : p.string();
    }

    std::string Path::GetFullPath(const std::string& path) {
        std::error_code ec;
        auto abs = std::filesystem::absolute(path, ec);
        if (ec) throw IOException("Failed to get full path: " + ec.message());
        return abs.string();
    }

    std::string Path::GetTempPath() {
        return std::filesystem::temp_directory_path().string() +
               std::string(1, DirectorySeparatorChar);
    }

    std::string Path::GetTempFileName() {
        auto tmp = std::filesystem::temp_directory_path() / "tmp_XXXXXX";
        std::string tmpl = tmp.string();
#ifdef _WIN32
        char buf[MAX_PATH];
        if (GetTempFileNameA(std::filesystem::temp_directory_path().string().c_str(),
                             "tmp", 0, buf) == 0)
            throw IOException("Failed to create temp file.");
        return std::string(buf);
#else
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
        int fd = mkstemp(buf.data());
        if (fd == -1) throw IOException("Failed to create temp file.");
        close(fd);
        return std::string(buf.data());
#endif
    }

    std::string Path::ChangeExtension(const std::string& path, const std::string& extension) {
        auto p = std::filesystem::path(path);
        p.replace_extension(extension.empty() || extension[0] == '.'
                            ? extension : "." + extension);
        return p.string();
    }

    bool Path::HasExtension(const std::string& path) {
        return !std::filesystem::path(path).extension().empty();
    }

    bool Path::IsPathRooted(const std::string& path) {
        return std::filesystem::path(path).is_absolute();
    }

} // namespace System::IO
