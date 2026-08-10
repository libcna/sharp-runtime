// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <filesystem>
#include <string>
#include <system_error>
#include "System/ArgumentException.hpp"
#include "System/DateTime.hpp"
#include "System/IO/IOException.hpp"

namespace System::IO {

    /**
     * @brief Provides the base class for both FileInfo and DirectoryInfo objects.
     *
     * Partial C++ counterpart of .NET System.IO.FileSystemInfo.
     *
     * @note Status: Partial — `UnixFileMode`, `LinkTarget`, `CreateAsSymbolicLink`, and
     *   `ResolveLinkTarget` are not implemented (documented gap, not silently wrong).
     *   `CreationTime`/`CreationTimeUtc` setters are not provided: reliably setting a
     *   file's birth time has no portable C++ standard library support and is not
     *   supported by most POSIX filesystems at all. `CreationTime` getters on POSIX use
     *   `st_ctime` (last inode-metadata-change time) as an approximation of birth time,
     *   since `<sys/stat.h>` on Linux has no portable birth-time field without a
     *   `statx()` feature-test — the same approximation real .NET itself documents for
     *   `File.GetCreationTimeUtc` on Linux prior to kernel/glibc `statx` support.
     */
    class FileSystemInfo {
    protected:
        std::filesystem::path fullPath_;
        std::string           originalPath_;

        /**
         * @brief Resolves the constructor's path without letting a `std::` exception escape.
         *
         * Ticket #2101 / SR-AUD-347, cause I-C. `std::filesystem::absolute(path)` throws
         * `std::filesystem::filesystem_error` for an empty path, and that exception was
         * propagating straight out of `FileInfo("")` and `DirectoryInfo("")` — a `std::`
         * exception crossing a `System`-shaped public API, invisible to a caller writing
         * `catch (const System::Exception&)`. Measured in
         * `build-probe/2101_probe2_before.log`:
         * *"filesystem error: cannot make absolute path: Invalid argument"*.
         *
         * The empty case is rejected explicitly, and every **other** failure is routed through
         * the `std::error_code` overload rather than the throwing one, so `filesystem_error`
         * can no longer escape this door **by construction** rather than by being caught.
         *
         * @param path The caller's path.
         * @return The absolute path.
         *
         * @throws System::ArgumentException if @p path is empty. The message matches the
         *         wording `FileInfo::CopyTo` already uses for the same defect in a different
         *         parameter, so the module states one rule once; the type and `paramName` are
         *         **this port's choice**, recorded as such because the reference tree is absent.
         * @throws System::IO::IOException if the path cannot be resolved for any other reason
         *         (for example when the current working directory has been removed).
         *
         * @note A **whitespace-only** path is deliberately still accepted: it resolves cleanly
         *       on POSIX and no repository evidence says .NET rejects it here. Narrowing it
         *       would be a guess, so it is recorded in
         *       `docs/SystemIONamespaceReviewPlan.md` rather than implemented.
         */
        static std::filesystem::path toAbsolutePath(const std::string& path) {
            if (path.empty()) {
                throw System::ArgumentException("Path cannot be the empty string.", "path");
            }
            std::error_code ec;
            std::filesystem::path absolute = std::filesystem::absolute(path, ec);
            if (ec) {
                throw System::IO::IOException("Could not resolve the full path of '" + path +
                                              "': " + ec.message());
            }
            return absolute;
        }

        /** @brief Constructs the base state from the path passed to a derived FileInfo/DirectoryInfo. */
        explicit FileSystemInfo(const std::string& path)
            : fullPath_(toAbsolutePath(path)), originalPath_(path) {}

    public:
        virtual ~FileSystemInfo() = default;

        /** @return The full absolute path of the file or directory. */
        [[nodiscard]] virtual std::string getFullNameProperty() const { return fullPath_.string(); }

        /** @return The extension of the file/directory name, including the leading dot, or empty if none. */
        [[nodiscard]] std::string getExtensionProperty() const { return fullPath_.extension().string(); }

        /** @return The name of the file or the last directory component (abstract in .NET; overridden by FileInfo/DirectoryInfo). */
        [[nodiscard]] virtual std::string getNameProperty() const = 0;

        /** @return True if the file or directory exists (abstract in .NET; overridden by FileInfo/DirectoryInfo). */
        [[nodiscard]] virtual bool getExistsProperty() const = 0;

        /** @brief Deletes the file or directory (abstract in .NET; overridden by FileInfo/DirectoryInfo). */
        virtual void Delete() = 0;

        /** @return The creation time of the file/directory, in local time (approximated via st_ctime on POSIX — see class doc comment). */
        [[nodiscard]] System::DateTime getCreationTimeProperty() const;
        /** @return The creation time of the file/directory, in UTC (approximated via st_ctime on POSIX — see class doc comment). */
        [[nodiscard]] System::DateTime getCreationTimeUtcProperty() const;

        /** @return The last access time of the file/directory, in local time. */
        [[nodiscard]] System::DateTime getLastAccessTimeProperty() const;
        /** @return The last access time of the file/directory, in UTC. */
        [[nodiscard]] System::DateTime getLastAccessTimeUtcProperty() const;

        /** @return The last write time of the file/directory, in local time. */
        [[nodiscard]] System::DateTime getLastWriteTimeProperty() const;
        /** @return The last write time of the file/directory, in UTC. */
        [[nodiscard]] System::DateTime getLastWriteTimeUtcProperty() const;

        /** @brief Sets the last write time of the file/directory, given in local time. */
        void setLastWriteTimeProperty(const System::DateTime& value);
        /** @brief Sets the last write time of the file/directory, given in UTC. */
        void setLastWriteTimeUtcProperty(const System::DateTime& value);

        /** @return The original path passed to the constructor (matches .NET's ToString(), which is NOT FullName). */
        [[nodiscard]] virtual std::string ToString() const { return originalPath_; }
    };

} // namespace System::IO
