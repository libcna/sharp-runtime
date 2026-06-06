// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/IOException.hpp"

namespace System::IO {

    class FileLoadException : public IOException {
        std::string fileName_;
    public:
        FileLoadException() : IOException("Could not load the specified file.") {}
        explicit FileLoadException(const std::string& message) : IOException(message) {}
        FileLoadException(const std::string& message, const std::string& fileName)
            : IOException(message), fileName_(fileName) {}
        FileLoadException(const std::string& message, const std::exception& inner)
            : IOException(message + " | inner: " + inner.what()) {}

        [[nodiscard]] const std::string& getFileNameProperty() const { return fileName_; }
    };

} // namespace System::IO
