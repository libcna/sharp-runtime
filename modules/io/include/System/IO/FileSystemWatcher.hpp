// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <atomic>
#include <exception>
#include <thread>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IO/ErrorEventHandler.hpp"
#include "System/IO/FileSystemEventHandler.hpp"
#include "System/IO/NotifyFilters.hpp"
#include "System/IO/RenamedEventHandler.hpp"

namespace System::IO {

    using SharpRuntime::intcs;

    /**
     * @brief Listens to the system directory change notifications and raises events when a
     * directory or file within a directory changes.
     *
     * C++ counterpart of .NET System.IO.FileSystemWatcher.
     *
     * @note Status: PARTIAL. On Linux, enabling the watcher (EnableRaisingEvents = true) starts
     * a real background thread backed by inotify that observes Created/Deleted/Changed/Renamed
     * events in the watched directory and invokes the registered handler vectors -- matching
     * real .NET's semantics for the common single-directory, non-recursive case. Deliberately
     * NOT implemented, matching this project's "document, don't rush" precedent for large
     * feature gaps: recursive subdirectory watching (IncludeSubdirectories is tracked but has
     * no effect -- would require walking the whole tree, adding a watch per subdirectory, and
     * handling directories created/removed while already watching), and any non-Linux backend
     * (FSEvents on macOS, ReadDirectoryChangesW on Windows) -- on those platforms, setting
     * EnableRaisingEvents = true throws System::PlatformNotSupportedException rather than
     * silently tracking the flag with no real monitoring, matching CLAUDE.md's
     * platform-abstraction policy (never silently fail on an unsupported platform). This is a
     * documented gap, not a silent one.
     */
    class FileSystemWatcher {
        std::string directory_;
        std::vector<std::string> filters_;
        NotifyFilters notifyFilter_ = NotifyFilters::LastWrite | NotifyFilters::FileName | NotifyFilters::DirectoryName;
        bool includeSubdirectories_ = false;
        // std::atomic because ticket #2347 lets the WATCHER thread write it (a handler calling
        // EnableRaisingEvents = false) while another thread reads it. Same size and alignment as
        // the bool it replaces, so the object layout is unchanged.
        std::atomic<bool> enabled_{false};
        unsigned int internalBufferSize_ = 8192;

        // Backing state for the real inotify-based watch thread (Linux only; unused elsewhere).
        // A FileSystemWatcher with a live watch thread can never be safely copied or moved (the
        // thread captures `this` for the lifetime of the watch), so copy/move are deleted below
        // rather than leaving a dangling-`this` hazard for a caller to discover the hard way.
        // Guarded to match FileSystemWatcher.cpp's own SHARP_RUNTIME_FSW_LINUX condition exactly --
        // on non-Linux targets (e.g. Emscripten, where startWatchingIfPossible() always throws)
        // these fields are never touched, and declaring them unconditionally left them dead code
        // there, tripping -Werror=unused-private-field.
#if defined(__linux__)
        int inotifyFd_ = -1;
        int watchDescriptor_ = -1;
        int stopEventFd_ = -1;
        // Set when the watcher thread stopped ITSELF -- a handler called EnableRaisingEvents =
        // false, so the stop was signalled but the thread could not be joined from inside itself
        // (ticket #2347). The thread object stays joinable until an external caller reaps it, so
        // this flag is what tells the arming path "joinable does not mean running here".
        std::atomic<bool> selfStopPending_{false};
#endif
        std::thread watchThread_;

        void startWatchingIfPossible();
        void stopWatchingIfRunning();
        void watchLoop();

        /**
         * @return true when the CALLING thread is this watcher's own watch thread.
         * @note Answered from a thread_local marker the watch loop sets, NOT by reading
         * watchThread_: reading that std::thread from the watcher thread races with the external
         * thread that re-arms the watcher by assigning it (ThreadSanitizer-confirmed against the
         * first cut of ticket #2347). A thread_local is per-thread by construction, so this adds
         * no shared state and no member.
         */
        [[nodiscard]] bool onWatcherThread() const noexcept;
        /** @brief Joins and cleans up a thread that stopped itself; a no-op otherwise. */
        void reapSelfStoppedThread();
        /** @brief Delivers an exception that escaped an event handler to the Error handlers. */
        void reportHandlerFault(std::exception_ptr fault);

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

        FileSystemWatcher(const FileSystemWatcher&) = delete;
        FileSystemWatcher& operator=(const FileSystemWatcher&) = delete;
        FileSystemWatcher(FileSystemWatcher&&) = delete;
        FileSystemWatcher& operator=(FileSystemWatcher&&) = delete;

        /** Stops any active watch thread (joining it) before the watcher is destroyed. */
        ~FileSystemWatcher();

        /**
         * @brief Gets or sets the path of the directory to watch.
         *
         * Setting a different path while EnableRaisingEvents is true retires the existing watch
         * and arms the new directory: the watcher thread is joined before the new path is stored,
         * so events from the old directory stop and no event can be reported against a directory
         * the caller no longer asked for. Events already queued for the old directory but not yet
         * delivered are discarded with the watch, exactly as they are when EnableRaisingEvents is
         * set to false. If the new directory cannot be armed, EnableRaisingEvents becomes false
         * and Error is raised, the same contract EnableRaisingEvents = true already carries; the
         * old watch is not retained as a fallback. A rejected value leaves the watcher completely
         * unchanged, still armed on the current directory.
         *
         * @throws System::ArgumentException if @p value is empty or is not an existing directory.
         * @throws System::PlatformNotSupportedException on a platform with no watch backend, if
         *         the watcher is enabled -- the same exception EnableRaisingEvents = true raises
         *         there, from whichever setter triggers the arm.
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
         *
         * The value is applied to the kernel-side watch, and changing it on a live watcher
         * re-arms through the same join -> write -> arm sequence Path uses, because the inotify
         * mask is fixed when the watch is armed. A reconfiguration discards events already queued
         * but not yet delivered, exactly as disabling does.
         *
         * <b>What this filters, exactly.</b> The defined values fall into two classes, and no
         * value in one class admits an event from the other:
         *
         * - <b>name class</b> -- FileName, DirectoryName -> Created, Deleted, Renamed;
         * - <b>content class</b> -- Attributes, Size, LastWrite, LastAccess, CreationTime,
         *   Security -> Changed.
         *
         * NotifyFilters(0) is a valid value naming no change: nothing is watched, and a later
         * filter change re-arms.
         *
         * <b>UNRESOLVED, and deliberately so (ticket #2346, SR-AUD-346 remains `confirmed`).</b>
         * Allocation of events WITHIN a class is a mapping policy this port has not chosen,
         * because Linux does not give the information the .NET vocabulary assumes: IN_MODIFY
         * cannot say whether the size changed, so Size and LastWrite are indistinguishable;
         * IN_ATTRIB covers chmod/chown/link-count/utimes without saying which, so all six content
         * values share it; inotify cannot report a creation-time change at all; LastAccess is
         * served by no mask this watcher registers; and FileName is not separated from
         * DirectoryName even though IN_ISDIR would allow it. Until #2346 is decided the six
         * content values are mutually indistinguishable and FileName behaves as DirectoryName
         * does. That is pinned by tests rather than assumed, so whichever mapping is eventually
         * chosen shows up as a test change and not as a silent drift.
         *
         * @throws System::ArgumentException if @p value contains bits outside the defined NotifyFilters values.
         */
        [[nodiscard]] NotifyFilters getNotifyFilterProperty() const { return notifyFilter_; }
        void setNotifyFilterProperty(NotifyFilters value);

        /** @brief Gets or sets whether subdirectories within the specified path should be monitored. */
        [[nodiscard]] bool getIncludeSubdirectoriesProperty() const { return includeSubdirectories_; }
        void setIncludeSubdirectoriesProperty(bool value) { includeSubdirectories_ = value; }

        /** @brief Gets or sets the size of the internal buffer, in bytes. Values below 4096 are clamped to 4096, matching .NET. */
        [[nodiscard]] intcs getInternalBufferSizeProperty() const { return static_cast<intcs>(internalBufferSize_); }
        void setInternalBufferSizeProperty(intcs value) {
            internalBufferSize_ = value < 4096 ? 4096u : static_cast<unsigned int>(value);
        }

        /**
         * @brief Gets or sets whether the component is enabled.
         *
         * On Linux, setting this to true starts a real inotify-backed watch thread for the
         * configured directory (see the class doc-comment for what is and isn't covered).
         * On other platforms this remains a documented stub: the flag is tracked faithfully so
         * ported C# code reading EnableRaisingEvents back gets the value it set, but no real
         * monitoring occurs.
         *
         * Setting this to false joins the watcher thread before returning, so no handler is
         * invoked for activity that happens afterwards, and events queued but not yet delivered
         * are discarded with the watch. Whether a handler that is ALREADY EXECUTING can still be
         * running when this setter returns is a concurrency question this port has NOT measured;
         * it is ticket #2105 (deferred -- it needs ThreadSanitizer and a blocking-handler
         * harness), and it is recorded as an open question rather than asserted either way.
         *
         * <b>Handlers run on the watcher thread, and this setter MAY be called from one</b>
         * (ticket #2347). Until #2347 all three reconfiguring members joined that thread
         * unconditionally, so calling any of them from a handler was a self-join: it raised
         * std::system_error ("Resource deadlock avoided") and, because handler invocation was not
         * wrapped in a try/catch, the exception reached std::terminate rather than the caller
         * (measured, `build-probe/2104_probe1_modeA.log`, SIGABRT). .NET permits the pattern, so
         * that was both a real divergence and a real crash. Now:
         *
         * - <b>`EnableRaisingEvents = false` from a handler is permitted</b>, matching .NET. The
         *   stop is signalled and the thread is NOT joined; the watch loop exits as soon as the
         *   current handler returns, and the thread is reaped by the next reconfiguration or by
         *   the destructor. The setter returns immediately, so a handler cannot wait on itself.
         * - <b>`Path` and `NotifyFilter` from a handler throw</b>
         *   `System::InvalidOperationException` while a watch is live, and are unchanged when no
         *   watch thread is running. Rejection rather than deferral is deliberate: both re-arm by
         *   retiring the current inotify watch and building a new one, which cannot be done from
         *   the thread that is inside that watch's own dispatch, and .NET's exact semantics for
         *   the case are not measurable here (`/rv` absent). The watcher's state is left exactly
         *   as it was, so a caller may retry from another thread.
         *
         * An exception that escapes any event handler no longer reaches std::terminate either: it
         * is delivered to the Error handlers, and an Error handler that itself throws is
         * swallowed rather than allowed to recurse.
         */
        [[nodiscard]] bool getEnableRaisingEventsProperty() const { return enabled_.load(); }
        void setEnableRaisingEventsProperty(bool value);
    };

} // namespace System::IO
