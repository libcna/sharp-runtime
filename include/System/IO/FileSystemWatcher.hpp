// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/IO/ErrorEventHandler.hpp"
#include "System/IO/FileSystemEventHandler.hpp"
#include "System/IO/NotifyFilters.hpp"
#include "System/IO/RenamedEventHandler.hpp"

namespace System::IO {

    /**
     * @brief Listens to the system directory change notifications and raises events when a
     * directory or file within a directory changes.
     *
     * C++ counterpart of .NET System.IO.FileSystemWatcher.
     *
     * @note Status: STUB. Property/event API surface matches .NET (path/filter validation,
     * NotifyFilter masking, EnableRaisingEvents state), so ported C# code that constructs a
     * watcher and registers handlers compiles and runs without crashing. Real OS-level file
     * system monitoring (inotify/FSEvents/ReadDirectoryChangesW) is NOT implemented — enabling
     * the watcher does not actually observe file system changes, and registered handlers are
     * never invoked. This is a documented gap, not a silent one.
     */
    class FileSystemWatcher {
        std::string directory_;
        std::vector<std::string> filters_;
        NotifyFilters notifyFilter_ = NotifyFilters::LastWrite | NotifyFilters::FileName | NotifyFilters::DirectoryName;
        bool includeSubdirectories_ = false;
        bool enabled_ = false;
        unsigned int internalBufferSize_ = 8192;

        static constexpr int ValidNotifyFiltersMask =
            static_cast<int>(NotifyFilters::Attributes)    | static_cast<int>(NotifyFilters::CreationTime) |
            static_cast<int>(NotifyFilters::DirectoryName) | static_cast<int>(NotifyFilters::FileName)     |
            static_cast<int>(NotifyFilters::LastAccess)    | static_cast<int>(NotifyFilters::LastWrite)    |
            static_cast<int>(NotifyFilters::Security)      | static_cast<int>(NotifyFilters::Size);

        static void CheckPathValidity(const std::string& path);

    public:
        /** Handler lists for each event; subscribers append via push_back, matching .NET's multicast delegate semantics. */
        std::vector<FileSystemEventHandler> Changed;
        std::vector<FileSystemEventHandler> Created;
        std::vector<FileSystemEventHandler> Deleted;
        std::vector<RenamedEventHandler>    Renamed;
        std::vector<ErrorEventHandler>      Error;

        /** Initializes a new instance of the FileSystemWatcher class with no directory set. */
        FileSystemWatcher() = default;

        /**
         * @brief Initializes a new instance of the FileSystemWatcher class, given the specified
         * directory to monitor.
         * @throws System::ArgumentException if @p path is empty or is not an existing directory.
         */
        explicit FileSystemWatcher(const std::string& path);

        /**
         * @brief Initializes a new instance of the FileSystemWatcher class, given the specified
         * directory and filter.
         * @throws System::ArgumentException if @p path is empty or is not an existing directory.
         */
        FileSystemWatcher(const std::string& path, const std::string& filter);

        /**
         * @brief Gets or sets the path of the directory to watch.
         * @throws System::ArgumentException if @p value is empty or is not an existing directory.
         */
        [[nodiscard]] const std::string& getPathProperty() const { return directory_; }
        void setPathProperty(const std::string& value);

        /** @brief Gets or sets the filter string used to determine which files are monitored (first entry of Filters, or "*" if empty). */
        [[nodiscard]] std::string getFilterProperty() const { return filters_.empty() ? "*" : filters_.front(); }
        void setFilterProperty(const std::string& value) {
            filters_.clear();
            filters_.push_back(value);
        }

        /** @brief Gets the collection of all filter strings used to determine which files are monitored. */
        [[nodiscard]] std::vector<std::string>& getFiltersProperty() { return filters_; }
        [[nodiscard]] const std::vector<std::string>& getFiltersProperty() const { return filters_; }

        /**
         * @brief Gets or sets the type of changes to watch for.
         * @throws System::ArgumentException if @p value contains bits outside the defined NotifyFilters values.
         */
        [[nodiscard]] NotifyFilters getNotifyFilterProperty() const { return notifyFilter_; }
        void setNotifyFilterProperty(NotifyFilters value);

        /** @brief Gets or sets whether subdirectories within the specified path should be monitored. */
        [[nodiscard]] bool getIncludeSubdirectoriesProperty() const { return includeSubdirectories_; }
        void setIncludeSubdirectoriesProperty(bool value) { includeSubdirectories_ = value; }

        /** @brief Gets or sets the size of the internal buffer, in bytes. Values below 4096 are clamped to 4096, matching .NET. */
        [[nodiscard]] int getInternalBufferSizeProperty() const { return static_cast<int>(internalBufferSize_); }
        void setInternalBufferSizeProperty(int value) {
            internalBufferSize_ = value < 4096 ? 4096u : static_cast<unsigned int>(value);
        }

        /**
         * @brief Gets or sets whether the component is enabled.
         *
         * @note This is a documented STUB: setting this to true does not start any real OS-level
         * file system monitoring, since none is implemented. The flag is tracked faithfully so
         * ported C# code reading EnableRaisingEvents back gets the value it set.
         */
        [[nodiscard]] bool getEnableRaisingEventsProperty() const { return enabled_; }
        void setEnableRaisingEventsProperty(bool value) { enabled_ = value; }
    };

} // namespace System::IO
