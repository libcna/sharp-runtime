// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/FileSystemWatcher.hpp"
#include "System/ArgumentException.hpp"
#include "System/IO/Directory.hpp"

namespace System::IO {

    void FileSystemWatcher::CheckPathValidity(const std::string& path) {
        if (path.empty()) {
            throw System::ArgumentException("Empty path.", "path");
        }
        if (!Directory::Exists(path)) {
            throw System::ArgumentException("The directory name " + path + " does not exist.", "path");
        }
    }

    FileSystemWatcher::FileSystemWatcher(const std::string& path) {
        CheckPathValidity(path);
        directory_ = path;
    }

    FileSystemWatcher::FileSystemWatcher(const std::string& path, const std::string& filter) {
        CheckPathValidity(path);
        directory_ = path;
        setFilterProperty(filter);
    }

    void FileSystemWatcher::setPathProperty(const std::string& value) {
        if (directory_ == value) return;
        if (value.empty()) {
            throw System::ArgumentException("Empty path.", "Path");
        }
        if (!Directory::Exists(value)) {
            throw System::ArgumentException("The directory name " + value + " does not exist.", "Path");
        }
        directory_ = value;
    }

    void FileSystemWatcher::setNotifyFilterProperty(NotifyFilters value) {
        if ((static_cast<int>(value) & ~ValidNotifyFiltersMask) != 0) {
            throw System::ArgumentException("The value of the NotifyFilter property is invalid.", "value");
        }
        notifyFilter_ = value;
    }

} // namespace System::IO
