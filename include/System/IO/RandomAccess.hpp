// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO {

using SharpRuntime::bytecs;
using SharpRuntime::intcs;

/// <summary>Provides static methods for reading and writing files at specific positions without seeking.</summary>
class RandomAccess {
public:
    RandomAccess() = delete;

    /// <summary>Gets the length of the file in bytes (platform-specific fd).</summary>
    static int64_t GetLength(int fd);

    /// <summary>Sets the length of the file.</summary>
    static void SetLength(int fd, int64_t length);

    /// <summary>Reads bytes from the file at the specified offset into the buffer.</summary>
    static intcs Read(int fd, std::vector<bytecs>& buffer, int64_t fileOffset);

    /// <summary>Reads bytes from the file at the specified offset into the buffer.</summary>
    static intcs Read(int fd, bytecs* buffer, intcs count, int64_t fileOffset);

    /// <summary>Writes bytes from the buffer to the file at the specified offset.</summary>
    static void Write(int fd, const std::vector<bytecs>& buffer, int64_t fileOffset);

    /// <summary>Writes bytes from the buffer to the file at the specified offset.</summary>
    static void Write(int fd, const bytecs* buffer, intcs count, int64_t fileOffset);

    /// <summary>Flushes OS write buffers to disk for the given file descriptor.</summary>
    static void FlushToDisk(int fd);
};

} // namespace System::IO
