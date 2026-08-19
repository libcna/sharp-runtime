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
     *       IsolatedStorageFileStream's own public constructor is not confined at all
     *       (ticket #2208).
     *
     * @note **DECLARED LIMITATION — the confinement is check-then-use (SR-AUD-241 residual,
     *       ticket #2207, decided 2026-08-19).** The resolver verifies containment with
     *       `weakly_canonical` and the operation then runs on the path *name*, so a process able
     *       to write inside the store root can swap a component for a symbolic link between the
     *       two and win the race.
     *
     *       **The threat boundary is what makes this acceptable rather than merely tolerated, so
     *       it is stated rather than left to be inferred:**
     *       - it **does** protect against *accidental* escape — a path that climbs out through
     *         `..`, an absolute path, or an existing symlink pointing outside — which is what the
     *         confinement was added for and what a caller passing untrusted *path text* needs;
     *       - it does **not** protect against an attacker who can already **write inside the
     *         store root**. Such an attacker can already read and write every file in the store
     *         directly, so winning this race widens their reach *beyond* the store rather than
     *         granting them access *to* it.
     *
     *       **What closing it would cost, measured and declined**: per-component
     *       `openat(O_NOFOLLOW)` resolution held open for the operation's whole duration,
     *       fd-relative `*at` operations, an **fd-accepting `FileStream` primitive this port does
     *       not have**, an `NtCreateFile`/`FILE_FLAG_OPEN_REPARSE_POINT` equivalent for Windows,
     *       and an Emscripten story. That is a platform-policy change for the whole module, not a
     *       repair to this function.
     *
     *       Pinned by `IsolatedStorageConfinementTests.Decl2207_*`.
     *
     * @note Status: DONE
     */
    class IsolatedStorageFile : public IsolatedStorage
    {
    private:
        /// #2208: the stream resolves its relative path through this class's own fullPath(), so
        /// the confinement has exactly one implementation rather than two that could drift.
        friend class IsolatedStorageFileStream;

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
         * @note This is a check-then-use test, **declared and accepted** rather than pending
         *       (ticket #2207, decided 2026-08-19). It rejects every path the caller can name,
         *       but it cannot defeat a process that swaps a path component for a symbolic link
         *       between the check and the operation. See the class-level note for the threat
         *       boundary that makes that acceptable, and for what closing it would cost.
         */
        [[nodiscard]] std::filesystem::path fullPath(const std::string& relativePath,
                                                     const char* paramName) const;

        /**
         * @brief Splits a search pattern into the directory to enumerate and the glob to match.
         *
         * Ticket #2209. `GetFileNames("sub/" "*")` must list `sub`'s files, matching .NET, whose two
         * enumeration doors delegate to `Directory.EnumerateFiles(RootDirectory, searchPattern)`
         * and whose `FileSystemEnumerableFactory.NormalizeInputs` splits the pattern at its last
         * separator (`FileSystemEnumerableFactory.cs:45-56`).
         *
         * The directory half goes through fullPath(), so it stays inside the store. .NET's does
         * **not** — see the implementation comment for why this port is deliberately the more
         * restrictive of the two.
         *
         * @param searchPattern The caller's pattern.
         * @param glob          Receives the final segment, or `"*"` when the pattern ends in a
         *                      separator or is empty.
         * @return The directory to enumerate.
         * @throws System::ArgumentException if the directory half leaves the store.
         */
        [[nodiscard]] std::filesystem::path resolveSearchScope(const std::string& searchPattern,
                                                               std::string&       glob) const;

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

        /**
         * @brief Obtains the user store scoped to the calling assembly and domain.
         *
         * .NET's `GetStore(Assembly | Domain | User)` (IsolatedStorageFile.cs:466-469). It is the
         * store `IsolatedStorageFileStream`'s storeless constructor defaults to, which is why
         * #2208 needed it; like this port's other two factories it resolves to the same storage
         * root, so the scope is recorded rather than reflected in the directory.
         */
        [[nodiscard]] static IsolatedStorageFile GetUserStoreForDomain();

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
