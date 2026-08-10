// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "System/IO/FileMode.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorage.hpp"
#include "System/IO/IsolatedStorage/IsolatedStorageScope.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO::IsolatedStorage
{
    class IsolatedStorageFileStream;

    /**
     * @brief Represents a scope of isolated storage.
     *
     * Practical port of .NET System.IO.IsolatedStorage.IsolatedStorageFile.
     * All paths passed to methods are interpreted relative to the storage root.
     *
     * @note **Root confinement.** Every path-taking member enforces that invariant rather than
     *       merely documenting it: a path that would resolve outside the storage root — because
     *       it is absolute, because `..` climbs past the root, or because a symbolic link in it
     *       points out — throws System::ArgumentException before any filesystem access or
     *       mutation happens.  A leading directory separator is stripped rather than rejected,
     *       matching .NET's `GetFullPath`, so `"/data.bin"` names `data.bin` inside the store.
     *       The check is check-then-use and therefore does not defeat a process concurrently
     *       replacing a path component with a symbolic link (ticket #2207), and
     *       IsolatedStorageFileStream's own public constructor is not confined at all
     *       (ticket #2208).
     *
     * @note Status: DONE
     */
    class IsolatedStorageFile : public IsolatedStorage
    {
    private:
        std::filesystem::path rootDirectory_; ///< Root directory of this isolated storage scope.
        bool disposed_ = false;               ///< True after Close()/Dispose().

        /**
         * @brief Resolves @p relativePath against the storage root and proves the result is
         *        confined to it.
         *
         * Enforces the store's defining invariant: the resolved target is the storage root's
         * descendant, or nothing happens at all.  Leading directory separators are stripped
         * first, matching .NET's `IsolatedStorageFile.GetFullPath`, so a rooted caller path is
         * reinterpreted as store-relative rather than honoured.  What remains is then checked
         * lexically (`..` may not climb past the root) and again after resolving every symbolic
         * link in the existing prefix.
         *
         * @param relativePath Caller-supplied path, interpreted relative to the storage root.
         * @param paramName    Name of the public parameter @p relativePath arrived in, used for
         *                     the thrown exception.
         * @return The normalized absolute path to operate on.
         * @throws System::ArgumentException if @p relativePath contains an embedded NUL, is
         *         empty once leading separators are removed, is rooted in a way this port does
         *         not reinterpret, or resolves outside the storage root.
         *
         * @note This is a check-then-use test.  It rejects every path the caller can name, but
         *       it cannot defeat a process that swaps a path component for a symbolic link
         *       between the check and the operation; see ticket #2207.
         */
        [[nodiscard]] std::filesystem::path fullPath(const std::string& relativePath,
                                                     const char* paramName) const;

        /** @throws System::ObjectDisposedException if this store has been Close()d/Remove()d/Dispose()d. */
        void throwIfDisposed() const;

    public:
        /** Constructs an IsolatedStorageFile rooted at @p rootDirectory with the given scope. */
        explicit IsolatedStorageFile(const std::filesystem::path& rootDirectory,
                                      IsolatedStorageScope scope = IsolatedStorageScope::None);

        /** Returns an isolated storage scoped to the current application. */
        [[nodiscard]] static IsolatedStorageFile GetUserStoreForApplication();

        /** Returns an isolated storage scoped to the current assembly (same root as application). */
        [[nodiscard]] static IsolatedStorageFile GetUserStoreForAssembly();

        // --- File operations ---

        /** Returns true if the specified relative path exists as a file in isolated storage. */
        [[nodiscard]] bool FileExists(const std::string& relativePath) const;

        /** Opens a file inside isolated storage with the specified mode. */
        [[nodiscard]] IsolatedStorageFileStream OpenFile(
            const std::string& relativePath,
            System::IO::FileMode mode) const;

        /** Creates a new file (or truncates an existing one) and returns its stream. */
        [[nodiscard]] IsolatedStorageFileStream CreateFile(const std::string& relativePath) const;

        /** Deletes the specified file from isolated storage. */
        void DeleteFile(const std::string& relativePath) const;

        /** Copies @p sourceFileName to @p destinationFileName; does not overwrite by default. */
        void CopyFile(const std::string& sourceFileName, const std::string& destinationFileName) const;

        /** Copies @p sourceFileName to @p destinationFileName; overwrites if @p overwrite is true. */
        void CopyFile(const std::string& sourceFileName, const std::string& destinationFileName, bool overwrite) const;

        /** Moves (renames) a file within isolated storage. */
        void MoveFile(const std::string& sourceFileName, const std::string& destinationFileName) const;

        /** Returns names of all files matching @p searchPattern ("*" matches everything). */
        [[nodiscard]] std::vector<std::string> GetFileNames(const std::string& searchPattern = "*") const;

        // --- Directory operations ---

        /** Returns true if the specified relative path exists as a directory in isolated storage. */
        [[nodiscard]] bool DirectoryExists(const std::string& relativePath) const;

        /** Creates the specified directory (and any intermediate directories) inside isolated storage. */
        void CreateDirectory(const std::string& relativePath) const;

        /** Deletes the specified directory and all its contents from isolated storage. */
        void DeleteDirectory(const std::string& relativePath) const;

        /** Moves (renames) a directory within isolated storage. */
        void MoveDirectory(const std::string& sourceDirectoryName, const std::string& destinationDirectoryName) const;

        /** Returns names of all directories matching @p searchPattern ("*" matches everything). */
        [[nodiscard]] std::vector<std::string> GetDirectoryNames(const std::string& searchPattern = "*") const;

        // --- Store lifecycle ---

        /** Removes the entire isolated storage store and all its contents. */
        void Remove() override;

        /** Closes the isolated storage file (marks as disposed). */
        void Close() override;

        /** Releases all resources used by the isolated storage. */
        void Dispose();

        // --- Space properties ---

        /** Returns the available free space in the store's volume in bytes. */
        [[nodiscard]] SharpRuntime::longcs getAvailableFreeSpaceProperty() const override;

        /** Returns the approximate used size of the store in bytes (sum of all file sizes). */
        [[nodiscard]] SharpRuntime::longcs getUsedSizeProperty() const override;

        /** Returns the store's quota in bytes. This runtime does not enforce quotas, so this is always longcs's maximum value. */
        [[nodiscard]] SharpRuntime::longcs getQuotaProperty() const override;

        /** Returns the root directory path for this isolated storage scope. */
        [[nodiscard]] const std::filesystem::path& getRootDirectoryProperty() const;
    };
}
