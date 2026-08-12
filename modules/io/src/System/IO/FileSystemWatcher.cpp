// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/FileSystemWatcher.hpp"
#include "System/ArgumentException.hpp"
#include "System/IO/Directory.hpp"
#include "System/IO/IOException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/PlatformNotSupportedException.hpp"

#if defined(__linux__)
#define SHARP_RUNTIME_FSW_LINUX 1
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstring>
#include <poll.h>
#include <regex>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <system_error>
#include <unistd.h>
#include <unordered_map>
#endif

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

    FileSystemWatcher::~FileSystemWatcher() {
        stopWatchingIfRunning();
    }

    namespace {
        // The watcher this thread is currently running the watch loop for, or nullptr. Set by
        // watchLoop() on entry and cleared on exit. Ticket #2347's first cut compared
        // watchThread_.get_id() instead, which ThreadSanitizer correctly reported as a data race:
        // the watcher thread read that std::thread while an external thread re-armed the watcher
        // by assigning it. A thread_local is private to the reading thread, so it cannot race.
        thread_local const FileSystemWatcher* tlsCurrentWatcher = nullptr;

        /** @brief Marks the calling thread as @p w's watch thread for the duration of the loop. */
        struct WatchThreadMarker {
            explicit WatchThreadMarker(const FileSystemWatcher* w) { tlsCurrentWatcher = w; }
            ~WatchThreadMarker() { tlsCurrentWatcher = nullptr; }
            WatchThreadMarker(const WatchThreadMarker&) = delete;
            WatchThreadMarker& operator=(const WatchThreadMarker&) = delete;
        };
    }

    bool FileSystemWatcher::onWatcherThread() const noexcept {
        // Ticket #2347. Handlers run ON watchThread_, so this is what separates "stop the
        // watcher" from "stop the watcher FROM INSIDE the watcher", which used to be a self-join.
        return tlsCurrentWatcher == this;
    }

    void FileSystemWatcher::reportHandlerFault(std::exception_ptr fault) {
        // Ticket #2347's second half: watchLoop invoked handlers with no try/catch, so ANY
        // exception escaping a handler -- not only the self-join -- reached std::terminate.
        // It is delivered here instead, on the same thread, as the asynchronous fault it is.
        if (Error.empty()) return;
        System::IO::ErrorEventArgs args(fault);
        for (auto& handler : Error) {
            // An Error handler that throws is swallowed: routing it back here would recurse.
            try { handler(this, args); } catch (...) {}
        }
    }

    void FileSystemWatcher::setPathProperty(const std::string& value) {
        if (directory_ == value) return;
        // Validation runs FIRST, before anything is torn down: a rejected path must leave a live
        // watch exactly as it was, still armed on the directory the caller already configured.
        if (value.empty()) {
            throw System::ArgumentException("Empty path.", "Path");
        }
        if (!Directory::Exists(value)) {
            throw System::ArgumentException("The directory name " + value + " does not exist.", "Path");
        }

        // Ticket #2347. Re-arming retires the current inotify watch and builds a new one, which
        // cannot be done from the thread that is inside that watch's own dispatch -- and the old
        // code tried to, by joining the calling thread with itself. Reject instead of deferring:
        // the caller's state is left exactly as it was, so a retry from another thread works.
        if (onWatcherThread()) {
            throw System::InvalidOperationException(
                "FileSystemWatcher::Path cannot be set from an event handler, because the "
                "handler runs on the watcher thread that the change has to retire. Set "
                "EnableRaisingEvents = false from the handler instead, or reconfigure from "
                "another thread.");
        }

        // watchLoop() reads directory_ on the WATCHER thread to build every FileSystemEventArgs,
        // so assigning it here while that thread runs is a data race -- ThreadSanitizer reported
        // it against the pre-repair tree, and its visible symptom was an event from the OLD
        // directory carrying a FullPath built from the NEW one: a path naming no file that
        // exists. Joining the watcher thread before the write removes the race BY CONSTRUCTION
        // rather than by adding a lock, and it is also exactly what re-arming requires -- the old
        // inotify watch has to be retired before the new directory can be armed.
        const bool wasEnabled = enabled_.load();
        stopWatchingIfRunning();
        directory_ = value;
        // Gated on enabled_ rather than on a watch having actually been running, so that the
        // "EnableRaisingEvents before Path" ordering startWatchingIfPossible() deliberately
        // tolerates (it returns quietly when no directory is configured yet) arms here, instead
        // of leaving the watcher enabled and permanently inert. If the new directory cannot be
        // armed, startWatchingIfPossible()'s existing contract applies unchanged: EnableRaisingEvents
        // goes false and Error is raised. The old watch is NOT kept as a fallback -- keeping it is
        // the defect this repair removes.
        if (wasEnabled) startWatchingIfPossible();
    }

    void FileSystemWatcher::setNotifyFilterProperty(NotifyFilters value) {
        if ((static_cast<int>(value) & ~ValidNotifyFiltersMask) != 0) {
            throw System::ArgumentException("The value of the NotifyFilter property is invalid.", "value");
        }
        if (value == notifyFilter_) return;

        // Ticket #2347. Re-arming retires the current inotify watch and builds a new one, which
        // cannot be done from the thread that is inside that watch's own dispatch -- and the old
        // code tried to, by joining the calling thread with itself. Reject instead of deferring:
        // the caller's state is left exactly as it was, so a retry from another thread works.
        if (onWatcherThread()) {
            throw System::InvalidOperationException(
                "FileSystemWatcher::NotifyFilter cannot be set from an event handler, because the "
                "handler runs on the watcher thread that the change has to retire. Set "
                "EnableRaisingEvents = false from the handler instead, or reconfigure from "
                "another thread.");
        }

        // The kernel-side mask is fixed when the watch is armed, so a filter changed on a live
        // watcher only takes effect if that watch is rebuilt. Unlike directory_, notifyFilter_ is
        // read only by the arming path on the caller thread, so this teardown is not there to
        // remove a race -- it is there because a narrower or wider mask needs a new watch.
        const bool wasEnabled = enabled_.load();
        stopWatchingIfRunning();
        notifyFilter_ = value;
        if (wasEnabled) startWatchingIfPossible();
    }

    void FileSystemWatcher::setEnableRaisingEventsProperty(bool value) {
        if (value == enabled_.load()) return;
        enabled_.store(value);
        if (value) startWatchingIfPossible();
        else stopWatchingIfRunning();
    }

#if defined(SHARP_RUNTIME_FSW_LINUX)

    namespace {
        // Mirrors Directory::GetFiles's glob-to-regex translation (including the "*.*" DOS-legacy
        // special case) so FileSystemWatcher's Filters behave the same way a caller would already
        // expect from GetFiles with the same pattern.
        bool matchesAnyFilter(const std::vector<std::string>& filters, const std::string& name) {
            if (filters.empty()) return true;
            for (const auto& rawPattern : filters) {
                std::string pattern = (rawPattern == "*.*") ? "*" : rawPattern;
                std::string regexPattern;
                for (char c : pattern) {
                    if (c == '*') regexPattern += ".*";
                    else if (c == '?') regexPattern += ".";
                    else if (std::string(".+^${}[]|()\\").find(c) != std::string::npos)
                        regexPattern += std::string("\\") + c;
                    else regexPattern += c;
                }
                std::regex rx(regexPattern, std::regex_constants::icase);
                if (std::regex_match(name, rx)) return true;
            }
            return false;
        }

        // NotifyFilters and inotify do not share a vocabulary, and this translation deliberately
        // resolves only the part of the mapping that needs no policy decision. The public values
        // fall into two classes:
        //
        //   name class     FileName, DirectoryName              -- a directory ENTRY changed
        //   content class  Attributes, Size, LastWrite,         -- the file BEHIND an entry changed
        //                  LastAccess, CreationTime, Security
        //
        // No value in one class can justify an event from the other, so a filter naming no
        // name-class value must not admit Created/Deleted/Renamed, and a filter naming no
        // content-class value must not admit Changed. That much is unambiguous with no reference
        // tree. Allocating events WITHIN a class is not, and is NOT decided here: IN_MODIFY could
        // serve Size or LastWrite or both; IN_ATTRIB could serve any of five; CreationTime has no
        // inotify event at all; LastAccess is unserved because IN_ACCESS is in no mask; and
        // FileName is not discriminated from DirectoryName even though IN_ISDIR would allow it.
        // Those five choices are ticket #2346 and are left exactly as they were measured -- this
        // function narrows the mask, it does not decide the mapping.
        constexpr int kNameClassFilters =
            static_cast<int>(NotifyFilters::FileName) | static_cast<int>(NotifyFilters::DirectoryName);
        constexpr int kContentClassFilters =
            static_cast<int>(NotifyFilters::Attributes)   | static_cast<int>(NotifyFilters::Size)     |
            static_cast<int>(NotifyFilters::LastWrite)    | static_cast<int>(NotifyFilters::LastAccess) |
            static_cast<int>(NotifyFilters::CreationTime) | static_cast<int>(NotifyFilters::Security);

        uint32_t inotifyMaskFor(NotifyFilters filter) {
            const int bits = static_cast<int>(filter);
            uint32_t mask = 0;
            // IN_MOVED_FROM and IN_MOVED_TO travel with IN_CREATE/IN_DELETE rather than forming a
            // class of their own: watchLoop pairs them by cookie to report a single Renamed, so
            // admitting one half of a pair would turn a rename into a spurious Created or Deleted.
            if ((bits & kNameClassFilters) != 0) {
                mask |= IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;
            }
            if ((bits & kContentClassFilters) != 0) {
                mask |= IN_MODIFY | IN_ATTRIB;
            }
            return mask;
        }
    } // namespace

    void FileSystemWatcher::startWatchingIfPossible() {
        // No directory configured yet: preserve the original stub behavior (track the flag,
        // watch nothing) rather than throwing -- matches ported C# code that sets
        // EnableRaisingEvents before Path in some order real .NET itself does not forbid either.
        if (directory_.empty()) return;
        // Ticket #2347. Arming from the watcher thread would assign watchThread_ while that very
        // thread runs, so a handler that re-enables a watcher it just disabled is a no-op; the
        // watch is reaped and re-armed by the next external call instead.
        if (onWatcherThread()) return;
        reapSelfStoppedThread(); // joinable-but-finished is not "already running"
        if (watchThread_.joinable()) return;

        // NotifyFilters(0) is a valid value that names no change to watch. inotify_add_watch
        // rejects a zero mask with EINVAL, which would surface as an Error event for a request
        // that is not an error, so it is treated the way an unconfigured directory already is:
        // the flag is tracked, nothing is watched, and a later NotifyFilter change re-arms.
        const uint32_t mask = inotifyMaskFor(notifyFilter_);
        if (mask == 0) return;

        inotifyFd_ = inotify_init1(IN_CLOEXEC);
        if (inotifyFd_ < 0) {
            enabled_.store(false);
            if (!Error.empty()) {
                auto ex = std::make_exception_ptr(
                    System::IO::IOException("Unable to initialize inotify: " + std::string(std::strerror(errno))));
                System::IO::ErrorEventArgs args(ex);
                for (auto& handler : Error) handler(this, args);
            }
            return;
        }

        watchDescriptor_ = inotify_add_watch(inotifyFd_, directory_.c_str(), mask);
        if (watchDescriptor_ < 0) {
            ::close(inotifyFd_);
            inotifyFd_ = -1;
            enabled_.store(false);
            if (!Error.empty()) {
                auto ex = std::make_exception_ptr(System::IO::IOException(
                    "Unable to watch directory '" + directory_ + "': " + std::string(std::strerror(errno))));
                System::IO::ErrorEventArgs args(ex);
                for (auto& handler : Error) handler(this, args);
            }
            return;
        }

        stopEventFd_ = eventfd(0, EFD_CLOEXEC);
        if (stopEventFd_ < 0) {
            inotify_rm_watch(inotifyFd_, watchDescriptor_);
            ::close(inotifyFd_);
            inotifyFd_ = -1;
            watchDescriptor_ = -1;
            enabled_.store(false);
            return;
        }

        try {
            watchThread_ = std::thread(&FileSystemWatcher::watchLoop, this);
        } catch (const std::system_error&) {
            // std::thread's constructor can throw (e.g. thread/process resource exhaustion).
            // stopWatchingIfRunning()'s cleanup is gated on watchThread_.joinable(), which never
            // becomes true on this path -- without this catch, inotifyFd_/watchDescriptor_/
            // stopEventFd_ would all leak for the lifetime of this object. Mirrors the
            // eventfd-failure cleanup just above, plus firing Error for consistency with the
            // inotify_init1/inotify_add_watch failure paths above that.
            ::close(stopEventFd_);
            inotify_rm_watch(inotifyFd_, watchDescriptor_);
            ::close(inotifyFd_);
            inotifyFd_ = -1;
            watchDescriptor_ = -1;
            stopEventFd_ = -1;
            enabled_.store(false);
            if (!Error.empty()) {
                auto ex = std::make_exception_ptr(
                    System::IO::IOException("Unable to start the file system watcher thread."));
                System::IO::ErrorEventArgs args(ex);
                for (auto& handler : Error) handler(this, args);
            }
        }
    }

    void FileSystemWatcher::reapSelfStoppedThread() {
        // Ticket #2347. A thread that stopped ITSELF is still joinable, so every path that asks
        // "is a watch running?" would otherwise mistake a finished watch for a live one and
        // silently do nothing -- leaving the watcher permanently inert after a handler disabled
        // it. Reaping is only ever done from a thread that is NOT the watcher thread.
        if (!selfStopPending_.load() || onWatcherThread()) return;
        if (watchThread_.joinable()) watchThread_.join();
        if (watchDescriptor_ >= 0 && inotifyFd_ >= 0) inotify_rm_watch(inotifyFd_, watchDescriptor_);
        if (inotifyFd_ >= 0) ::close(inotifyFd_);
        if (stopEventFd_ >= 0) ::close(stopEventFd_);
        inotifyFd_ = watchDescriptor_ = stopEventFd_ = -1;
        selfStopPending_.store(false);
    }

    void FileSystemWatcher::stopWatchingIfRunning() {
        // Ticket #2347. The identity check runs BEFORE watchThread_ is touched at all: reading
        // that std::thread from the watcher thread is itself a race with an external re-arm.
        const bool selfStop = onWatcherThread();
        if (!selfStop) {
            reapSelfStoppedThread();
            if (!watchThread_.joinable()) return;
        }

        if (stopEventFd_ >= 0) {
            uint64_t one = 1;
            // Best-effort wakeup: watchLoop's poll() is blocked waiting on this fd (or on the
            // inotify fd) with no timeout, so writing to it is the only way to unblock the
            // thread promptly without ever detaching it -- eliminating the dangling-`this`
            // hazard this project has documented (but left unfixed) for other background-thread
            // callback types elsewhere (e.g. Socket, ClientWebSocket, Timer).
            ssize_t written = ::write(stopEventFd_, &one, sizeof(one));
            (void)written;
        }

        // Ticket #2347. .NET permits a handler to stop its own watcher, and this is the call it
        // makes. The stop has been signalled, so the loop exits as soon as the handler returns;
        // joining here is what raised std::system_error("Resource deadlock avoided") and, with no
        // try/catch around handler invocation, reached std::terminate. The descriptors are NOT
        // closed either -- the loop is still polling them. Both are done when an external caller
        // or the destructor reaps the thread.
        if (selfStop) {
            selfStopPending_.store(true);
            return;
        }

        watchThread_.join();

        if (watchDescriptor_ >= 0 && inotifyFd_ >= 0) inotify_rm_watch(inotifyFd_, watchDescriptor_);
        if (inotifyFd_ >= 0) ::close(inotifyFd_);
        if (stopEventFd_ >= 0) ::close(stopEventFd_);
        inotifyFd_ = watchDescriptor_ = stopEventFd_ = -1;
    }

    void FileSystemWatcher::watchLoop() {
        const WatchThreadMarker marker(this); // ticket #2347: identifies this thread to onWatcherThread()
        constexpr size_t kEventBufSize = 16 * (sizeof(struct inotify_event) + NAME_MAX + 1);
        std::vector<char> buf(kEventBufSize);

        struct pollfd fds[2];
        fds[0].fd = inotifyFd_;   fds[0].events = POLLIN; fds[0].revents = 0;
        fds[1].fd = stopEventFd_; fds[1].events = POLLIN; fds[1].revents = 0;

        for (;;) {
            int pollResult = poll(fds, 2, -1);
            if (pollResult < 0) {
                if (errno == EINTR) continue;
                return;
            }
            if (fds[1].revents & POLLIN) return; // stop requested

            if (!(fds[0].revents & POLLIN)) continue;

            ssize_t len = read(inotifyFd_, buf.data(), buf.size());
            if (len <= 0) {
                if (len < 0 && errno == EINTR) continue;
                return;
            }

            // Tracks an in-flight IN_MOVED_FROM waiting for its paired IN_MOVED_TO (same cookie),
            // so a same-directory rename is reported as one Renamed event rather than a
            // Deleted+Created pair -- matching real .NET's rename semantics. Scoped per read()
            // batch: a paired rename's two halves are always delivered in the same batch, so
            // anything left unpaired at the end of this batch genuinely moved out of the watched
            // directory and is reported as Deleted, matching what the caller would observe.
            std::unordered_map<uint32_t, std::string> pendingMovedFrom;

            size_t i = 0;
            while (i + sizeof(struct inotify_event) <= static_cast<size_t>(len)) {
                auto* ev = reinterpret_cast<struct inotify_event*>(buf.data() + i);
                std::string name = ev->len > 0 ? std::string(ev->name) : std::string();

                if (name.empty() || matchesAnyFilter(filters_, name)) {
                    if (ev->mask & IN_MOVED_FROM) {
                        pendingMovedFrom[ev->cookie] = name;
                    } else if (ev->mask & IN_MOVED_TO) {
                        auto it = pendingMovedFrom.find(ev->cookie);
                        if (it != pendingMovedFrom.end()) {
                            RenamedEventArgs args(WatcherChangeTypes::Renamed, directory_, name, it->second);
                            for (auto& handler : Renamed)
                                try { handler(this, args); }
                                catch (...) { reportHandlerFault(std::current_exception()); }
                            pendingMovedFrom.erase(it);
                        } else {
                            FileSystemEventArgs args(WatcherChangeTypes::Created, directory_, name);
                            for (auto& handler : Created)
                                try { handler(this, args); }
                                catch (...) { reportHandlerFault(std::current_exception()); }
                        }
                    } else if (ev->mask & IN_CREATE) {
                        FileSystemEventArgs args(WatcherChangeTypes::Created, directory_, name);
                        for (auto& handler : Created)
                            try { handler(this, args); }
                            catch (...) { reportHandlerFault(std::current_exception()); }
                    } else if (ev->mask & IN_DELETE) {
                        FileSystemEventArgs args(WatcherChangeTypes::Deleted, directory_, name);
                        for (auto& handler : Deleted)
                            try { handler(this, args); }
                            catch (...) { reportHandlerFault(std::current_exception()); }
                    } else if (ev->mask & (IN_MODIFY | IN_ATTRIB)) {
                        FileSystemEventArgs args(WatcherChangeTypes::Changed, directory_, name);
                        for (auto& handler : Changed)
                            try { handler(this, args); }
                            catch (...) { reportHandlerFault(std::current_exception()); }
                    }
                }

                i += sizeof(struct inotify_event) + ev->len;
            }

            for (const auto& [cookie, name] : pendingMovedFrom) {
                (void)cookie;
                FileSystemEventArgs args(WatcherChangeTypes::Deleted, directory_, name);
                for (auto& handler : Deleted)
                    try { handler(this, args); }
                    catch (...) { reportHandlerFault(std::current_exception()); }
            }
        }
    }

#else // !SHARP_RUNTIME_FSW_LINUX

    // No OS-level watch backend implemented for this platform (see the class doc-comment).
    // Matches CLAUDE.md's platform-abstraction rule: on an unsupported platform, throw
    // System::PlatformNotSupportedException rather than silently degrading to a no-op -- a
    // silently-inert watcher that never fires a single event is a worse failure mode than a
    // loud, immediate exception at the point EnableRaisingEvents is actually turned on.
    void FileSystemWatcher::startWatchingIfPossible() {
        throw System::PlatformNotSupportedException(
            "FileSystemWatcher requires Linux inotify support; no watch backend is implemented "
            "for this platform.");
    }
    void FileSystemWatcher::stopWatchingIfRunning() {}
    void FileSystemWatcher::watchLoop() {}
    void FileSystemWatcher::reapSelfStoppedThread() {}

#endif

} // namespace System::IO
