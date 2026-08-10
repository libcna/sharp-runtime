// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <filesystem>

#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

namespace System::IO::IsolatedStorage
{
    /**
     * @brief Represents a file stream inside isolated storage.
     *
     * C++ counterpart of .NET System.IO.IsolatedStorage.IsolatedStorageFileStream, which
     * derives from FileStream; this port does the same, inheriting Read/Write/Position/
     * Seek/CanSeek/SetLength. Obtain instances via IsolatedStorageFile::OpenFile()/CreateFile().
     */
    class IsolatedStorageFileStream : public System::IO::FileStream
    {
    public:
        /**
         * @brief Opens an isolated storage file stream at @p fullPath with the specified @p mode.
         * @param fullPath Absolute filesystem path to the file. Parent directories are created if missing.
         * @param mode Specifies how the file should be opened or created.
         *
         * @warning **This constructor is not a confinement boundary.** It opens whatever path it
         *          is handed, anywhere on the filesystem, and creates that path's missing parent
         *          directories.  Confinement to an isolated store is enforced by
         *          IsolatedStorageFile::OpenFile()/CreateFile(), which validate the caller's
         *          relative path before constructing a stream; calling this constructor directly
         *          bypasses that check entirely.  Confining it would mean taking the owning store
         *          as a parameter, a public signature change tracked as ticket #2208.  Prefer
         *          IsolatedStorageFile::OpenFile()/CreateFile().
         */
        IsolatedStorageFileStream(const std::filesystem::path& fullPath, System::IO::FileMode mode);

        /** Closes the underlying file stream, syncing IDBFS on Emscripten builds. */
        void Close() override;
    };
}
