// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <filesystem>
#include <string>

#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

namespace System::IO::IsolatedStorage
{
    class IsolatedStorageFile;

    /**
     * @brief Represents a file stream inside isolated storage.
     *
     * C++ counterpart of .NET System.IO.IsolatedStorage.IsolatedStorageFileStream, which
     * derives from FileStream; this port does the same, inheriting Read/Write/Position/
     * Seek/CanSeek/SetLength.
     *
     * @note **Confined since ticket #2208 (2026-08-19).** The constructor used to take a
     * `std::filesystem::path` and check nothing: it opened whatever it was handed, anywhere on
     * the filesystem, and created that path's missing parent directories. That was a **wider
     * hole than #2207's declared TOCTOU** -- no race and no privilege were needed, just the call.
     *
     * **The reference corrected this ticket's proposed shape.** #2208 was written as "remove the
     * path constructor and take the owning store instead", but .NET publishes eight constructors
     * and every one of them begins `(string path, FileMode mode, ...)` with the store as an
     * **optional trailing** parameter (`IsolatedStorageFile? isf`,
     * `IsolatedStorageFileStream.cs:21-56`). Its storeless form is **not** unconfined: when `isf`
     * is null it takes `IsolatedStorageFile.GetUserStoreForDomain()` and resolves through
     * `isf.GetFullPath(path)` exactly as the store-taking form does (`:82-118`). So the
     * confinement this ticket exists to add is obtained **without** removing an overload .NET
     * publishes and without inventing a leading-store parameter order.
     *
     * Both forms therefore resolve `path` against a store's root through the same `fullPath()`
     * every other door on `IsolatedStorageFile` uses; a path that escapes is refused before any
     * filesystem access happens.
     *
     * The parameter type changed with the meaning: `std::filesystem::path` -> `std::string`,
     * which is .NET's. On POSIX `std::filesystem::path` converts to `std::string` implicitly, so
     * an existing call still compiles and its **meaning** changes -- from "open this filesystem
     * path" to "open this path inside the store". That is the repair, not a side effect.
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
        /**
         * @brief Opens a stream on @p path inside the default user store.
         * @param path Path relative to the store. Leading separators are stripped, as at every
         *             other door, so an absolute path is contained rather than refused.
         * @param mode How the file should be opened or created.
         * @throws System::ArgumentException if @p path would resolve outside the store, is empty,
         *         or @p mode is not one of the six defined FileMode values.
         *
         * The .NET counterpart defaults to `IsolatedStorageFile.GetUserStoreForDomain()` and this
         * port does the same.
         */
        IsolatedStorageFileStream(const std::string& path, System::IO::FileMode mode);

        /**
         * @brief Opens a stream on @p path inside @p store.
         * @param path  Path relative to @p store.
         * @param mode  How the file should be opened or created.
         * @param store The owning isolated store; the path is resolved against its root.
         * @throws System::ArgumentException as above.
         *
         * The store is trailing because .NET's is (`IsolatedStorageFileStream.cs:26`). It is a
         * reference rather than .NET's nullable `IsolatedStorageFile?` because passing null there
         * is defined to mean "use the default store", which is what the two-argument overload
         * above already spells.
         */
        IsolatedStorageFileStream(const std::string& path,
                                  System::IO::FileMode mode,
                                  const IsolatedStorageFile& store);

        /** Closes the underlying file stream, syncing IDBFS on Emscripten builds. */
        void Close() override;

    private:
        friend class IsolatedStorageFile;

        /**
         * The resolving constructor every public one funnels through. @p paramName is the name
         * the CALLING door gives its own path parameter, so a refusal names the parameter the
         * caller actually wrote: `path` from the two public constructors, `relativePath` from
         * IsolatedStorageFile::OpenFile()/CreateFile(). Reporting one door's parameter name from
         * another door is the shape #2323 rules out.
         */
        IsolatedStorageFileStream(const std::string& path,
                                  System::IO::FileMode mode,
                                  const IsolatedStorageFile& store,
                                  const char* paramName);

        /**
         * Validates @p mode, resolves @p path through @p store's own confinement check, and
         * creates the file's missing parents. A member rather than a free function because
         * IsolatedStorageFile::fullPath() is private and this class is its only friend.
         */
        [[nodiscard]] static std::string Resolve(const std::string& path,
                                                 System::IO::FileMode mode,
                                                 const IsolatedStorageFile& store,
                                                 const char* paramName);
    };
}
