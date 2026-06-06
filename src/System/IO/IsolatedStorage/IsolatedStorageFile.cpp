// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/IsolatedStorage/IsolatedStorageFile.hpp"

#include <filesystem>

#include "SharpRuntime/Storage/StoragePaths.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageException.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageFileStream.hpp"

namespace System::IO::IsolatedStorage
{
    IsolatedStorageFile::IsolatedStorageFile(const std::filesystem::path& rootDirectory)
        : rootDirectory(rootDirectory)
    {
        std::filesystem::create_directories(this->rootDirectory);
    }

    IsolatedStorageFile IsolatedStorageFile::GetUserStoreForApplication()
    {
        return IsolatedStorageFile(SharpRuntime::Storage::StoragePaths::GetIsolatedStorageRoot());
    }

    IsolatedStorageFile IsolatedStorageFile::GetUserStoreForAssembly()
    {
        return IsolatedStorageFile(SharpRuntime::Storage::StoragePaths::GetIsolatedStorageRoot());
    }

    bool IsolatedStorageFile::FileExists(const std::string& relativePath) const
    {
        const std::filesystem::path fullPath = rootDirectory / relativePath;
        return std::filesystem::exists(fullPath) && std::filesystem::is_regular_file(fullPath);
    }

    IsolatedStorageFileStream IsolatedStorageFile::OpenFile(
        const std::string& relativePath,
        System::IO::FileMode mode) const
    {
        return IsolatedStorageFileStream(rootDirectory / relativePath, mode);
    }

    void IsolatedStorageFile::DeleteFile(const std::string& relativePath) const
    {
        const std::filesystem::path fullPath = rootDirectory / relativePath;

        std::error_code ec;
        std::filesystem::remove(fullPath, ec);

        if (ec)
        {
            throw IsolatedStorageException(
                "Failed to delete isolated storage file: " + fullPath.string());
        }
    }

    const std::filesystem::path& IsolatedStorageFile::getRootDirectoryProperty() const
    {
        return rootDirectory;
    }
}