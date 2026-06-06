// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/File.hpp"
#include "System/IO/FileNotFoundException.hpp"
#include "System/IO/IOException.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>

namespace System::IO {

    bool File::Exists(const std::string& path) {
        return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
    }

    void File::Delete(const std::string& path) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        if (ec) throw IOException("Failed to delete file: " + path);
    }

    void File::Copy(const std::string& src, const std::string& dst, bool overwrite) {
        auto opts = overwrite
            ? std::filesystem::copy_options::overwrite_existing
            : std::filesystem::copy_options::none;
        std::error_code ec;
        std::filesystem::copy_file(src, dst, opts, ec);
        if (ec) throw IOException("Failed to copy file: " + ec.message());
    }

    void File::Move(const std::string& src, const std::string& dst) {
        std::error_code ec;
        std::filesystem::rename(src, dst, ec);
        if (ec) throw IOException("Failed to move file: " + ec.message());
    }

    std::string File::ReadAllText(const std::string& path) {
        if (!Exists(path)) throw FileNotFoundException("Unable to find the specified file.", path);
        std::ifstream f(path);
        if (!f) throw IOException("Failed to open file: " + path);
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    void File::WriteAllText(const std::string& path, const std::string& contents) {
        std::ofstream f(path, std::ios::trunc);
        if (!f) throw IOException("Failed to open file for writing: " + path);
        f << contents;
    }

    std::vector<std::string> File::ReadAllLines(const std::string& path) {
        if (!Exists(path)) throw FileNotFoundException("Unable to find the specified file.", path);
        std::ifstream f(path);
        if (!f) throw IOException("Failed to open file: " + path);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(f, line)) lines.push_back(line);
        return lines;
    }

    void File::WriteAllLines(const std::string& path, const std::vector<std::string>& lines) {
        std::ofstream f(path, std::ios::trunc);
        if (!f) throw IOException("Failed to open file for writing: " + path);
        for (const auto& line : lines) f << line << '\n';
    }

    std::vector<SharpRuntime::bytecs> File::ReadAllBytes(const std::string& path) {
        if (!Exists(path)) throw FileNotFoundException("Unable to find the specified file.", path);
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) throw IOException("Failed to open file: " + path);
        auto size = f.tellg();
        f.seekg(0);
        std::vector<SharpRuntime::bytecs> buf(static_cast<size_t>(size));
        f.read(reinterpret_cast<char*>(buf.data()), size);
        return buf;
    }

    void File::WriteAllBytes(const std::string& path,
                             const std::vector<SharpRuntime::bytecs>& bytes) {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) throw IOException("Failed to open file for writing: " + path);
        f.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    }

    void File::AppendAllText(const std::string& path, const std::string& contents) {
        std::ofstream f(path, std::ios::app);
        if (!f) throw IOException("Failed to open file for appending: " + path);
        f << contents;
    }

} // namespace System::IO
