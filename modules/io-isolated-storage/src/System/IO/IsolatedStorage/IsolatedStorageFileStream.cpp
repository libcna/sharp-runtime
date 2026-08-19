// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/IsolatedStorage/IsolatedStorageFileStream.hpp"

#include "System/IO/IsolatedStorage/IsolatedStorageFile.hpp"

#include <filesystem>

#include "System/ArgumentException.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace System::IO::IsolatedStorage
{
    namespace {
        std::string PrepareFullPath(const std::filesystem::path& fullPath)
        {
            std::filesystem::create_directories(fullPath.parent_path());
            return fullPath.string();
        }

        // .NET rejects any mode outside the six it defines, with this exact text
        // (IsolatedStorageFileStream.cs:98-110, SR.IsolatedStorage_FileOpenMode). Every named
        // FileMode is accepted, so the only reachable failure is a value cast in from outside
        // the enumeration -- the #2378 shape.
        void ValidateMode(System::IO::FileMode mode)
        {
            switch (mode) {
                case System::IO::FileMode::CreateNew:
                case System::IO::FileMode::Create:
                case System::IO::FileMode::OpenOrCreate:
                case System::IO::FileMode::Truncate:
                case System::IO::FileMode::Append:
                case System::IO::FileMode::Open:
                    return;
            }
            throw System::ArgumentException("Invalid mode, see System.IO.FileMode.");
        }
    }

    // #2208: the path is resolved through the STORE's own fullPath(), the same resolver
    // OpenFile/CreateFile use -- so the confinement has one implementation, not two that could
    // drift apart. It throws before PrepareFullPath can create any directory.
    //
    // .NET performs its own two checks first: the mode, and an empty or backslash-only path
    // (IsolatedStorageFileStream.cs:88-110). The second needs no code here -- fullPath() strips
    // leading separators and then rejects what is left when it is empty, which covers "/" on
    // POSIX and both "/" and "\\" on Windows. Reproducing .NET's literal `path == "\\"` test on
    // POSIX would reject a legitimate file name, which this module's own isDirectorySeparator()
    // already says in its comment.
    std::string IsolatedStorageFileStream::Resolve(const std::string& path,
                                                   System::IO::FileMode mode,
                                                   const IsolatedStorageFile& store,
                                                   const char* paramName)
    {
        ValidateMode(mode);
        return PrepareFullPath(store.fullPath(path, paramName));
    }

    IsolatedStorageFileStream::IsolatedStorageFileStream(
        const std::string& path,
        System::IO::FileMode mode,
        const IsolatedStorageFile& store,
        const char* paramName)
        : System::IO::FileStream(Resolve(path, mode, store, paramName), mode)
    {
    }

    IsolatedStorageFileStream::IsolatedStorageFileStream(
        const std::string& path,
        System::IO::FileMode mode,
        const IsolatedStorageFile& store)
        : IsolatedStorageFileStream(path, mode, store, "path")
    {
    }

    // .NET's storeless form is `this(path, mode, ..., null)`, and a null store there means
    // GetUserStoreForDomain() (IsolatedStorageFileStream.cs:21-23, :82-87). The temporary store
    // lives until this constructor completes, which is all that is needed: the resolved path is
    // taken by value and nothing here retains the store.
    IsolatedStorageFileStream::IsolatedStorageFileStream(
        const std::string& path,
        System::IO::FileMode mode)
        : IsolatedStorageFileStream(path, mode, IsolatedStorageFile::GetUserStoreForDomain())
    {
    }

    void IsolatedStorageFileStream::Close()
    {
        System::IO::FileStream::Close();
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
