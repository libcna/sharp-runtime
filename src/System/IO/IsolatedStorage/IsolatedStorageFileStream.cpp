// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/IsolatedStorage/IsolatedStorageFileStream.hpp"

#include <filesystem>
#include <fstream>

#include "System/IO/IsolatedStorage/IsolatedStorageException.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace System::IO::IsolatedStorage
{
    IsolatedStorageFileStream::IsolatedStorageFileStream(
        const std::filesystem::path& fullPath,
        System::IO::FileMode mode)
        : fullPath(fullPath)
    {
        std::filesystem::create_directories(fullPath.parent_path());

        std::ios::openmode openMode = std::ios::binary;
        switch (mode)
        {
            case System::IO::FileMode::Open:
                openMode |= std::ios::in;
                break;
            case System::IO::FileMode::Create:
                openMode |= std::ios::in | std::ios::out | std::ios::trunc;
                break;
            case System::IO::FileMode::CreateNew:
                openMode |= std::ios::out | std::ios::trunc;
                break;
            case System::IO::FileMode::OpenOrCreate:
                openMode |= std::ios::in | std::ios::out;
                break;
            case System::IO::FileMode::Truncate:
                openMode |= std::ios::out | std::ios::trunc;
                break;
            case System::IO::FileMode::Append:
                openMode |= std::ios::out | std::ios::app;
                break;
        }

        stream.open(fullPath, openMode);

        if (!stream.is_open())
        {
            throw IsolatedStorageException(
                "Failed to open isolated storage file: " + fullPath.string());
        }
    }

    void IsolatedStorageFileStream::Close()
    {
        if (stream.is_open())
        {
            stream.close();
#if defined(__EMSCRIPTEN__)
            // Flush in-memory IDBFS writes to IndexedDB so save data survives
            // page reloads.  The callback is fire-and-forget; errors are logged
            // to the browser console.
            EM_ASM(
                FS.syncfs(false, function(err) {
                    if (err) { console.warn('IDBFS post-close sync failed:', err); }
                });
            );
#endif
        }
    }

    SharpRuntime::intcs IsolatedStorageFileStream::Read(
        SharpRuntime::bytecs buffer[],
        SharpRuntime::intcs offset,
        SharpRuntime::intcs count)
    {
        if (!stream.is_open() || buffer == nullptr || offset < 0 || count < 0)
        {
            return 0;
        }

        stream.read(reinterpret_cast<char*>(buffer + offset), count);
        return static_cast<SharpRuntime::intcs>(stream.gcount());
    }

    void IsolatedStorageFileStream::Write(
        const SharpRuntime::bytecs buffer[],
        SharpRuntime::intcs offset,
        SharpRuntime::intcs count)
    {
        if (!stream.is_open() || buffer == nullptr || offset < 0 || count < 0)
        {
            throw IsolatedStorageException("Invalid write operation on isolated storage stream.");
        }

        stream.write(reinterpret_cast<const char*>(buffer + offset), count);
        if (!stream.good())
        {
            throw IsolatedStorageException(
                "Failed to write isolated storage file: " + fullPath.string());
        }
    }

    SharpRuntime::intcs IsolatedStorageFileStream::getLengthProperty()
    {
        if (!std::filesystem::exists(fullPath))
        {
            return 0;
        }

        return static_cast<SharpRuntime::intcs>(std::filesystem::file_size(fullPath));
    }

    bool IsolatedStorageFileStream::IsOpen() const
    {
        return stream.is_open();
    }
}