// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/IO/IOException.hpp"
namespace System::IO {
    /** @brief The exception thrown when part of a file or directory cannot be found. @note Status: Implemented */
    class DirectoryNotFoundException : public IOException {
        std::string directoryPath_;
    public:
        /** Initializes a new DirectoryNotFoundException with a default message. */
        DirectoryNotFoundException();
        /** Initializes a new DirectoryNotFoundException with the specified message. */
        explicit DirectoryNotFoundException(const char* message);
        /** Initializes a new DirectoryNotFoundException with the specified message. */
        explicit DirectoryNotFoundException(const std::string& message);
        /** Initializes a DirectoryNotFoundException with a message and an inner exception. */
        DirectoryNotFoundException(const std::string& message, std::exception_ptr inner);
        /** Initializes a DirectoryNotFoundException with a message and the directory path that could not be found. */
        DirectoryNotFoundException(const std::string& message, const std::string& directoryPath);
        /**
         * @brief Initializes a DirectoryNotFoundException with a message, the directory path
         * that could not be found, and the exception that caused the failure.
         *
         * Completes this port's directory-path family so that a caller no longer has to
         * discard either the failed path or its cause, matching the
         * `(message, fileName, inner)` shape `System::IO::FileNotFoundException` and
         * `System::IO::FileLoadException` already provide.
         */
        DirectoryNotFoundException(const std::string& message, const std::string& directoryPath,
                                   std::exception_ptr inner);

        /** @return The directory path that could not be found, or an empty string if not set. */
        [[nodiscard]] const std::string& getDirectoryPathProperty() const { return directoryPath_; }
    };
} // namespace System::IO
