// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <unistd.h>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO {

using SharpRuntime::bytecs;
using SharpRuntime::intcs;

/// <summary>Provides static methods for reading and writing files at specific positions without seeking.</summary>
class RandomAccess {
public:
    RandomAccess() = delete;

    /// <summary>Gets the length of the file in bytes.</summary>
    static int64_t GetLength(int fd) {
        off_t pos = lseek(fd, 0, SEEK_CUR);
        off_t len = lseek(fd, 0, SEEK_END);
        lseek(fd, pos, SEEK_SET);
        return static_cast<int64_t>(len);
    }

    /// <summary>Sets the length of the file.</summary>
    static void SetLength(int fd, int64_t length) {
        if (ftruncate(fd, static_cast<off_t>(length)) != 0)
            throw std::runtime_error("SetLength failed");
    }

    /// <summary>Reads bytes from the file at the specified offset into the buffer.</summary>
    static intcs Read(int fd, std::vector<bytecs>& buffer, int64_t fileOffset) {
        ssize_t n = pread(fd, buffer.data(), buffer.size(), static_cast<off_t>(fileOffset));
        if (n < 0) throw std::runtime_error("RandomAccess::Read failed");
        return static_cast<intcs>(n);
    }

    /// <summary>Reads bytes from the file at the specified offset into the buffer.</summary>
    static intcs Read(int fd, bytecs* buffer, intcs count, int64_t fileOffset) {
        ssize_t n = pread(fd, buffer, static_cast<size_t>(count), static_cast<off_t>(fileOffset));
        if (n < 0) throw std::runtime_error("RandomAccess::Read failed");
        return static_cast<intcs>(n);
    }

    /// <summary>Writes bytes from the buffer to the file at the specified offset.</summary>
    static void Write(int fd, const std::vector<bytecs>& buffer, int64_t fileOffset) {
        ssize_t n = pwrite(fd, buffer.data(), buffer.size(), static_cast<off_t>(fileOffset));
        if (n < 0) throw std::runtime_error("RandomAccess::Write failed");
    }

    /// <summary>Writes bytes from the buffer to the file at the specified offset.</summary>
    static void Write(int fd, const bytecs* buffer, intcs count, int64_t fileOffset) {
        ssize_t n = pwrite(fd, buffer, static_cast<size_t>(count), static_cast<off_t>(fileOffset));
        if (n < 0) throw std::runtime_error("RandomAccess::Write failed");
    }

    /// <summary>Flushes OS write buffers to disk for the given file descriptor.</summary>
    static void FlushToDisk(int fd) {
        if (fsync(fd) != 0) throw std::runtime_error("FlushToDisk failed");
    }
};

} // namespace System::IO
