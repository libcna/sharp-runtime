// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/FileStream.hpp"
#include "System/IO/FileNotFoundException.hpp"

#include <stdexcept>
#include <filesystem>

namespace System::IO
{
    static std::ios::openmode toIosMode(FileMode mode) {
        switch (mode) {
            case FileMode::CreateNew:
                return std::ios::binary | std::ios::in | std::ios::out;
            case FileMode::Create:
                return std::ios::binary | std::ios::out | std::ios::trunc;
            case FileMode::Open:
                return std::ios::binary | std::ios::in;
            case FileMode::OpenOrCreate:
                return std::ios::binary | std::ios::in | std::ios::out;
            case FileMode::Truncate:
                return std::ios::binary | std::ios::out | std::ios::trunc;
            case FileMode::Append:
                return std::ios::binary | std::ios::out | std::ios::app;
            default:
                return std::ios::binary | std::ios::in;
        }
    }

    static bool isWriteMode(FileMode mode) {
        return mode != FileMode::Open;
    }

    static std::ios::openmode toIosModeWithAccess(FileMode mode, FileAccess access)
    {
        std::ios::openmode m = std::ios::binary;
        // Apply FileMode base flags
        switch (mode) {
            case FileMode::CreateNew:   m |= std::ios::out; break;
            case FileMode::Create:      m |= std::ios::out | std::ios::trunc; break;
            case FileMode::Open:        break;
            case FileMode::OpenOrCreate:break;
            case FileMode::Truncate:    m |= std::ios::out | std::ios::trunc; break;
            case FileMode::Append:      m |= std::ios::out | std::ios::app; break;
            default: break;
        }
        // Layer FileAccess on top
        if (access == FileAccess::Read || access == FileAccess::ReadWrite)
            m |= std::ios::in;
        if (access == FileAccess::Write || access == FileAccess::ReadWrite)
            m |= std::ios::out;
        return m;
    }

    FileStream::FileStream(const std::string& path)
        : FileStream(path, FileMode::Open) {}

    FileStream::FileStream(const std::string& path, FileMode mode)
        : mode_(mode), length_(0), canWrite_(isWriteMode(mode))
    {
        auto iosMode = toIosMode(mode);
        file_.open(path, iosMode);
        if (!file_.is_open()) {
            if (mode == FileMode::Open)
                throw FileNotFoundException("Unable to find the specified file.", path);
            throw std::runtime_error("Failed to open file: " + path);
        }
        if (!canWrite_) {
            file_.seekg(0, std::ios::end);
            length_ = static_cast<intcs>(file_.tellg());
            file_.seekg(0, std::ios::beg);
        }
    }

    FileStream::FileStream(const std::string& path, FileMode mode, FileAccess access)
        : mode_(mode), length_(0),
          canWrite_(access == FileAccess::Write || access == FileAccess::ReadWrite)
    {
        auto iosMode = toIosModeWithAccess(mode, access);
        file_.open(path, iosMode);
        if (!file_.is_open()) {
            if (mode == FileMode::Open)
                throw FileNotFoundException("Unable to find the specified file.", path);
            throw std::runtime_error("Failed to open file: " + path);
        }
        if (access == FileAccess::Read) {
            file_.seekg(0, std::ios::end);
            length_ = static_cast<intcs>(file_.tellg());
            file_.seekg(0, std::ios::beg);
        }
    }

    FileStream::~FileStream() { Close(); }

    intcs FileStream::Read(bytecs buffer[], intcs offset, intcs count)
    {
        if (!file_.is_open() || buffer == nullptr || offset < 0 || count < 0) return 0;
        file_.read(reinterpret_cast<char*>(buffer + offset), count);
        return static_cast<intcs>(file_.gcount());
    }

    void FileStream::Write(const bytecs buffer[], intcs offset, intcs count)
    {
        if (!file_.is_open() || buffer == nullptr || count <= 0) return;
        file_.write(reinterpret_cast<const char*>(buffer + offset),
                    static_cast<std::streamsize>(count));
    }

    void FileStream::WriteByte(bytecs value)
    {
        file_.put(static_cast<char>(value));
    }

    void FileStream::Flush()
    {
        if (file_.is_open()) file_.flush();
    }

    void FileStream::Close()
    {
        if (file_.is_open()) file_.close();
    }

    intcs FileStream::getLengthProperty() const { return length_; }

    bool FileStream::IsOpen() const { return file_.is_open(); }
}
