// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <filesystem>
#include <fstream>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IO/FileMode.hpp"

namespace System::IO::IsolatedStorage
{
    /// <summary>
    /// Represents a file stream inside isolated storage.
    ///
    /// Lightweight practical port of .NET IsolatedStorageFileStream backed by
    /// a std::fstream. Obtain instances via IsolatedStorageFile::OpenFile().
    /// </summary>
    class IsolatedStorageFileStream
    {
    private:
        std::fstream stream; ///< Underlying std::fstream used for I/O.
        std::filesystem::path fullPath; ///< Absolute path to the file on disk.

    public:
        /// Opens an isolated storage file stream at @p fullPath with the specified @p mode.
        /// @param fullPath Absolute filesystem path to the file.
        /// @param mode Specifies how the file should be opened or created.
        IsolatedStorageFileStream(
            const std::filesystem::path& fullPath,
            System::IO::FileMode mode);

        /// Closes the underlying file stream.
        void Close();

        /// Reads up to @p count bytes from the stream into @p buffer starting at @p offset.
        /// @param buffer Destination byte buffer.
        /// @param offset Byte offset within @p buffer at which to begin writing.
        /// @param count Maximum number of bytes to read.
        /// @return Number of bytes actually read.
        SharpRuntime::intcs Read(SharpRuntime::bytecs buffer[], SharpRuntime::intcs offset, SharpRuntime::intcs count);

        /// Writes @p count bytes from @p buffer (starting at @p offset) to the stream.
        /// @param buffer Source byte buffer.
        /// @param offset Byte offset within @p buffer to begin reading.
        /// @param count Number of bytes to write.
        void Write(const SharpRuntime::bytecs buffer[], SharpRuntime::intcs offset, SharpRuntime::intcs count);

        /// Returns the total length of the file in bytes.
        [[nodiscard]] SharpRuntime::intcs getLengthProperty();

        /// Returns true if the underlying file stream is open.
        [[nodiscard]] bool IsOpen() const;
    };
}
