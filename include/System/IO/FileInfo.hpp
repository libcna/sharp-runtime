// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <filesystem>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO {

    using SharpRuntime::longcs;

    /**
     * @brief Provides instance methods for the creation, copying, deletion, moving, and opening of files.
     *
     * Wraps std::filesystem. Partial C++ counterpart of .NET System.IO.FileInfo.
     *
     * @note Status: Partial
     */
    class FileInfo {
        std::filesystem::path path_;
    public:
        /** Constructs a FileInfo for the specified path. */
        explicit FileInfo(const std::string& path) : path_(path) {}

        /** Returns the file name and extension. */
        [[nodiscard]] std::string getNameProperty()      const { return path_.filename().string(); }
        /** Returns the full absolute path of the file. */
        [[nodiscard]] std::string getFullNameProperty()  const { return std::filesystem::absolute(path_).string(); }
        /** Returns the extension of the file, including the leading dot. */
        [[nodiscard]] std::string getExtensionProperty() const { return path_.extension().string(); }
        /** Returns the directory portion of the full path. */
        [[nodiscard]] std::string getDirectoryNameProperty() const { return path_.parent_path().string(); }

        /** Returns true if the file exists. */
        [[nodiscard]] bool getExistsProperty() const { return std::filesystem::is_regular_file(path_); }

        /** Returns the file size in bytes, or -1 if an error occurs. */
        [[nodiscard]] longcs getLengthProperty() const {
            std::error_code ec;
            auto sz = std::filesystem::file_size(path_, ec);
            return ec ? -1LL : static_cast<longcs>(sz);
        }

        /** Returns true if the file does not have write permission for its owner. */
        [[nodiscard]] bool getIsReadOnlyProperty() const {
            auto perms = std::filesystem::status(path_).permissions();
            return (perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none;
        }

        /** Permanently deletes the file. */
        void Delete() { std::filesystem::remove(path_); }

        /** Copies the file to destFileName; overwrites if overwrite is true. */
        void CopyTo(const std::string& destFileName, bool overwrite = false) {
            auto opts = overwrite
                ? std::filesystem::copy_options::overwrite_existing
                : std::filesystem::copy_options::none;
            std::filesystem::copy_file(path_, destFileName, opts);
        }

        /** Moves the file to destFileName, updating the internal path. */
        void MoveTo(const std::string& destFileName) {
            std::filesystem::rename(path_, destFileName);
            path_ = destFileName;
        }

        /** Copies the file to destFileName and returns a FileInfo for the destination. */
        FileInfo CopyToInfo(const std::string& destFileName, bool overwrite = false) {
            CopyTo(destFileName, overwrite);
            return FileInfo(destFileName);
        }
    };

} // namespace System::IO
