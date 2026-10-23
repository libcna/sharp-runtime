// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/File.hpp"
#include "Utf8Path.hpp"

#include <optional>
#include "System/IO/FileMode.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileNotFoundException.hpp"
#include "System/IO/IOException.hpp"
#include "System/IO/DirectoryNotFoundException.hpp"
#include "System/IO/PathTooLongException.hpp"
#include "System/ArgumentException.hpp"
#include "System/UnauthorizedAccessException.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cerrno>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace System::IO {

    namespace {
        void ThrowIfNullOrEmpty(const std::string& path, const std::string& paramName) {
            if (path.empty()) throw System::ArgumentException("Path cannot be the empty string.", paramName);
        }
    }

    bool File::Exists(const std::string& path) {
        // .NET File.cs:91-119 calls Path.GetFullPath and catches its ArgumentException.
        // A native C-string filesystem call must not truncate an embedded NUL to a real file.
        if (path.empty() || path.find('\0') != std::string::npos) return false;
        std::error_code ec;
        const std::optional<std::filesystem::path> native = Detail::TryNativePath(path);
        if (!native) return false;
        bool isFile = std::filesystem::is_regular_file(*native, ec);
        return !ec && isFile;
    }

    void File::Delete(const std::string& path) {
        ThrowIfNullOrEmpty(path, "path");
        if (path.find('\0') != std::string::npos)
            throw System::ArgumentException("Null character in path.", "path");
        const auto native = Detail::NativePath(path);
        // Use a file-only operation: a preceding directory check plus remove() can
        // still remove a directory after a rename, and follows directory symlinks.
#if defined(_WIN32)
        if (::DeleteFileW(native.c_str())) return;
        const auto error = ::GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) return;
        if (error == ERROR_PATH_NOT_FOUND)
            throw DirectoryNotFoundException("Could not find a part of the path '" + path + "'.");
        if (error == ERROR_ACCESS_DENIED)
            throw System::UnauthorizedAccessException("Access to the path '" + path + "' is denied.");
        if (error == ERROR_FILENAME_EXCED_RANGE)
            throw PathTooLongException("The file name or path is too long.");
        throw IOException("Failed to delete file '" + path + "': " +
                          std::system_category().message(static_cast<int>(error)));
#else
        if (::unlink(native.c_str()) == 0) return;
        const int error = errno;
        if (error == ENOENT) {
            auto parent = native.parent_path();
            if (parent.empty()) parent = ".";
            std::error_code statusError;
            if (std::filesystem::is_directory(parent, statusError) && !statusError) return;
            throw DirectoryNotFoundException("Could not find a part of the path '" + path + "'.");
        }
        if (error == ENOTDIR)
            throw DirectoryNotFoundException("Could not find a part of the path '" + path + "'.");
        if (error == EACCES || error == EPERM || error == EISDIR)
            throw System::UnauthorizedAccessException("Access to the path '" + path + "' is denied.");
        if (error == ENAMETOOLONG)
            throw PathTooLongException("The file name or path is too long.");
        // A read-only filesystem can reject even a nonexistent leaf before lookup.
        if (error == EROFS) {
            std::error_code statusError;
            (void)std::filesystem::status(native, statusError);
            if (statusError == std::errc::no_such_file_or_directory) return;
        }
        throw IOException("Failed to delete file '" + path + "': " +
                          std::generic_category().message(error));
#endif
    }

    void File::Copy(const std::string& src, const std::string& dst, bool overwrite) {
        ThrowIfNullOrEmpty(src, "sourceFileName");
        ThrowIfNullOrEmpty(dst, "destFileName");
        if (!Exists(src)) throw FileNotFoundException("Could not find file '" + src + "'.", src);
        auto opts = overwrite
            ? std::filesystem::copy_options::overwrite_existing
            : std::filesystem::copy_options::none;
        std::error_code ec;
        std::filesystem::copy_file(src, dst, opts, ec);
        if (ec) throw IOException("Failed to copy file '" + src + "' to '" + dst + "': " + ec.message());
    }

    void File::Move(const std::string& src, const std::string& dst) {
        ThrowIfNullOrEmpty(src, "sourceFileName");
        ThrowIfNullOrEmpty(dst, "destFileName");
        if (!Exists(src)) throw FileNotFoundException("Could not find file '" + src + "'.", src);
        // Verified against FileSystem.Unix.cs's MoveFile(src, dst, overwrite: false): real
        // .NET's non-overwrite Move does not replace an existing destination file. This port
        // previously used std::filesystem::rename() unconditionally, which silently replaces
        // an existing destination on POSIX.
        if (Exists(dst))
            throw IOException("Cannot create '" + dst + "' because a file or directory with the same name already exists.");
        std::error_code ec;
        std::filesystem::rename(src, dst, ec);
        if (ec) throw IOException("Failed to move file '" + src + "' to '" + dst + "': " + ec.message());
    }

    std::string File::ReadAllText(const std::string& path) {
        if (!Exists(path)) throw FileNotFoundException("Unable to find the specified file.", path);
        std::ifstream f(Detail::NativePath(path));
        if (!f) throw IOException("Failed to open file: " + path);
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    void File::WriteAllText(const std::string& path, const std::string& contents) {
        std::ofstream f(Detail::NativePath(path), std::ios::trunc);
        if (!f) throw IOException("Failed to open file for writing: " + path);
        f << contents;
    }

    std::vector<std::string> File::ReadAllLines(const std::string& path) {
        if (!Exists(path)) throw FileNotFoundException("Unable to find the specified file.", path);
        std::ifstream f(Detail::NativePath(path));
        if (!f) throw IOException("Failed to open file: " + path);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(f, line)) lines.push_back(line);
        return lines;
    }

    void File::WriteAllLines(const std::string& path, const std::vector<std::string>& lines) {
        std::ofstream f(Detail::NativePath(path), std::ios::trunc);
        if (!f) throw IOException("Failed to open file for writing: " + path);
        for (const auto& line : lines) f << line << '\n';
    }

    std::vector<SharpRuntime::bytecs> File::ReadAllBytes(const std::string& path) {
        if (!Exists(path)) throw FileNotFoundException("Unable to find the specified file.", path);
        std::ifstream f(Detail::NativePath(path), std::ios::binary | std::ios::ate);
        if (!f) throw IOException("Failed to open file: " + path);
        auto size = f.tellg();
        f.seekg(0);
        std::vector<SharpRuntime::bytecs> buf(static_cast<size_t>(size));
        f.read(reinterpret_cast<char*>(buf.data()), size);
        return buf;
    }

    void File::WriteAllBytes(const std::string& path,
                             const std::vector<SharpRuntime::bytecs>& bytes) {
        std::ofstream f(Detail::NativePath(path), std::ios::binary | std::ios::trunc);
        if (!f) throw IOException("Failed to open file for writing: " + path);
        f.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    }

    void File::AppendAllText(const std::string& path, const std::string& contents) {
        std::ofstream f(Detail::NativePath(path), std::ios::app);
        if (!f) throw IOException("Failed to open file for appending: " + path);
        f << contents;
    }

    FileStream File::Create(const std::string& path) {
        return FileStream(path, FileMode::Create, FileAccess::ReadWrite, FileShare::None);
    }

    FileStream File::OpenRead(const std::string& path) {
        return FileStream(path, FileMode::Open, FileAccess::Read);
    }

} // namespace System::IO
