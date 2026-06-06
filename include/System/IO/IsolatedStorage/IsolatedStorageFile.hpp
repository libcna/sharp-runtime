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

    /**
     * @brief Represents a scope of isolated storage.
     *
     * This is a lightweight practical port of .NET IsolatedStorageFile.
     *
     * @note Status: PARTIAL
     */
    class IsolatedStorageFile
    {
    private:
        /**
         * @brief Root directory of this isolated storage.
         *
         * @note Status: IMPLEMENTED
         */
        std::filesystem::path rootDirectory;

    public:
        /**
         * @brief Initializes a new IsolatedStorageFile with the specified root directory.
         *
         * @param rootDirectory Root directory path.
         *
         * @note Status: IMPLEMENTED
         */
        explicit IsolatedStorageFile(const std::filesystem::path& rootDirectory);

        /**
         * @brief Gets an isolated storage scoped to the current application.
         *
         * @return Isolated storage instance.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static IsolatedStorageFile GetUserStoreForApplication();

        /**
         * @brief Gets an isolated storage scoped to the current assembly.
         *
         * In this practical port, this behaves the same as GetUserStoreForApplication().
         *
         * @return Isolated storage instance.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static IsolatedStorageFile GetUserStoreForAssembly();

        /**
         * @brief Returns true if the specified file exists in isolated storage.
         *
         * @param relativePath Relative file path.
         * @return True if the file exists.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] bool FileExists(const std::string& relativePath) const;

        /**
         * @brief Opens a file in isolated storage.
         *
         * @param relativePath Relative file path.
         * @param mode File mode.
         * @return Opened isolated storage file stream.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] IsolatedStorageFileStream OpenFile(
            const std::string& relativePath,
            System::IO::FileMode mode) const;

        /**
         * @brief Deletes a file from isolated storage.
         *
         * @param relativePath Relative file path.
         *
         * @note Status: IMPLEMENTED
         */
        void DeleteFile(const std::string& relativePath) const;

        /**
         * @brief Gets the root directory of this isolated storage.
         *
         * @return Root path.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] const std::filesystem::path& getRootDirectoryProperty() const;
    };
}