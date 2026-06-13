// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <filesystem>
#include <string>

#include "System/IO/FileMode.hpp"

namespace System::IO::IsolatedStorage
{
    class IsolatedStorageFileStream;

    /// <summary>
    /// Represents a scope of isolated storage.
    ///
    /// Lightweight practical port of .NET IsolatedStorageFile. Isolated storage
    /// is rooted at a configurable directory; all paths passed to methods are
    /// interpreted relative to that root.
    /// </summary>
    class IsolatedStorageFile
    {
    private:
        std::filesystem::path rootDirectory; ///< Root directory of this isolated storage scope.

    public:
        /// Constructs an IsolatedStorageFile rooted at @p rootDirectory.
        /// @param rootDirectory Absolute or relative path to the storage root.
        explicit IsolatedStorageFile(const std::filesystem::path& rootDirectory);

        /// Returns an isolated storage scoped to the current application.
        /// @return IsolatedStorageFile instance.
        [[nodiscard]] static IsolatedStorageFile GetUserStoreForApplication();

        /// Returns an isolated storage scoped to the current assembly.
        /// In this practical port, behaves identically to GetUserStoreForApplication().
        /// @return IsolatedStorageFile instance.
        [[nodiscard]] static IsolatedStorageFile GetUserStoreForAssembly();

        /// Returns true if the specified relative path exists as a file in isolated storage.
        /// @param relativePath File path relative to the storage root.
        [[nodiscard]] bool FileExists(const std::string& relativePath) const;

        /// Opens a file inside isolated storage with the specified mode.
        /// @param relativePath File path relative to the storage root.
        /// @param mode Specifies how the file should be opened or created.
        /// @return Opened IsolatedStorageFileStream.
        [[nodiscard]] IsolatedStorageFileStream OpenFile(
            const std::string& relativePath,
            System::IO::FileMode mode) const;

        /// Deletes the specified file from isolated storage.
        /// @param relativePath File path relative to the storage root.
        void DeleteFile(const std::string& relativePath) const;

        /// Returns the root directory path for this isolated storage scope.
        [[nodiscard]] const std::filesystem::path& getRootDirectoryProperty() const;
    };
}
