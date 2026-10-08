// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/FileSystemWatcher.hpp"
#include <algorithm>
#include "System/ArgumentException.hpp"
#include "System/IO/Directory.hpp"
#include "System/IO/IOException.hpp"
#include "System/IO/InternalBufferOverflowException.hpp"
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
#elif defined(__APPLE__)
// AM4-055: Darwin has no inotify. kqueue reports a directory's own vnode changing when an entry is
// added, removed or renamed, and each entry's vnode for its own content; the directory is
// re-listed and diffed against the snapshot taken when watching started.
#define SHARP_RUNTIME_FSW_DARWIN 1
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <optional>
#include <regex>
#include <sys/event.h>
#include <sys/resource.h>
#include <sys/stat.h>
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

#if defined(SHARP_RUNTIME_FSW_LINUX) || defined(SHARP_RUNTIME_FSW_DARWIN)

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
        // content-class value must not admit Changed. That much was unambiguous with no reference
        // tree and landed as #2345.
        //
        // Allocating events WITHIN a class is ticket #2346, and it is NOT derivable from the
        // reference and never will be: `NotifyFilters` names the notifications Win32's
        // ReadDirectoryChangesW produces, and inotify's event set is not a relabelling of it. It
        // is therefore a user decision, taken on 2026-08-17 and recorded as
        // `docs/StandingApprovals.md` SA-7. The shape of the answer is *permissive where Linux
        // genuinely cannot discriminate, discriminating where it can*: over-notification is
        // recoverable by a caller, silence is not, but where the information does exist the two
        // filters are meant to differ.
        //
        //   1 (a)  IN_MODIFY serves Size AND LastWrite. Linux gives one bit for "content was
        //          written" and no way to know whether the length changed, so serving only one of
        //          the two would silently remove behaviour from the other.
        //   2 (a)  IN_ATTRIB serves ALL SIX content values. It is one bit for
        //          chmod/chown/link-count/utimes and does not say which of them happened.
        //   3 (a)  CreationTime is approximated through the content class. inotify cannot report a
        //          btime change at all; rejecting the value at the setter (option c) would throw
        //          for a value .NET accepts, and admitting nothing (option b) would make a
        //          configured filter silently inert.
        //   4 (c)  IN_ACCESS is admitted ONLY when LastAccess is named. Adding it to the whole
        //          content class would make every read wake every content watcher; leaving it out
        //          entirely left a named filter unable to fire for its own operation.
        //   5 (b)  FileName and DirectoryName are separated, in DISPATCH rather than in the mask,
        //          because IN_ISDIR travels on the event and not on the subscription. See
        //          nameClassAdmits() below.
        constexpr int kNameClassFilters =
            static_cast<int>(NotifyFilters::FileName) | static_cast<int>(NotifyFilters::DirectoryName);
        constexpr int kContentClassFilters =
            static_cast<int>(NotifyFilters::Attributes)   | static_cast<int>(NotifyFilters::Size)     |
            static_cast<int>(NotifyFilters::LastWrite)    | static_cast<int>(NotifyFilters::LastAccess) |
            static_cast<int>(NotifyFilters::CreationTime) | static_cast<int>(NotifyFilters::Security);
        // Decision 1(a): the two values a "content was written" notification can honestly serve.
        constexpr int kWriteServedFilters =
            static_cast<int>(NotifyFilters::Size) | static_cast<int>(NotifyFilters::LastWrite);

#if defined(SHARP_RUNTIME_FSW_LINUX)
        uint32_t inotifyMaskFor(NotifyFilters filter) {
            const int bits = static_cast<int>(filter);
            uint32_t mask = 0;
            // IN_MOVED_FROM and IN_MOVED_TO travel with IN_CREATE/IN_DELETE rather than forming a
            // class of their own: watchLoop pairs them by cookie to report a single Renamed, so
            // admitting one half of a pair would turn a rename into a spurious Created or Deleted.
            // Both name-class values subscribe to the same events; decision 5(b) separates them
            // afterwards, on IN_ISDIR, which is not expressible in a mask.
            if ((bits & kNameClassFilters) != 0) {
                mask |= IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;
            }
            if ((bits & kWriteServedFilters) != 0) {
                mask |= IN_MODIFY;                       // decision 1(a)
            }
            if ((bits & kContentClassFilters) != 0) {
                mask |= IN_ATTRIB;                       // decisions 2(a) and 3(a)
            }
            if ((bits & static_cast<int>(NotifyFilters::LastAccess)) != 0) {
                mask |= IN_ACCESS;                       // decision 4(c)
            }
            return mask;
        }
#endif

        /**
         * Decision 5(b). `IN_ISDIR` is set on the event, so the name-class split can only be
         * applied where the event is dispatched, never in the subscription mask.
         *
         * A directory entry is either a directory or it is not, so exactly one of the two values
         * governs any given `Created`/`Deleted`/`Renamed`. A watcher naming neither never reaches
         * here, because `inotifyMaskFor` subscribed to none of those events.
         */
        bool nameClassAdmits(NotifyFilters filter, bool isDirectory) {
            const int bits = static_cast<int>(filter);
            const auto governing = isDirectory ? NotifyFilters::DirectoryName : NotifyFilters::FileName;
            return (bits & static_cast<int>(governing)) != 0;
        }
    } // namespace

#endif

#if defined(SHARP_RUNTIME_FSW_LINUX)

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
            // The pending half of a rename carries its IN_ISDIR with it: decision 5(b) has to be
            // applied to the whole pair, and an IN_MOVED_FROM left unpaired at the end of the
            // batch is reported as a Deleted, which is governed by the same value.
            struct PendingMove {
                std::string name;
                bool        isDirectory = false;
            };
            std::unordered_map<uint32_t, PendingMove> pendingMovedFrom;

            size_t i = 0;
            while (i + sizeof(struct inotify_event) <= static_cast<size_t>(len)) {
                auto* ev = reinterpret_cast<struct inotify_event*>(buf.data() + i);
                std::string name = ev->len > 0 ? std::string(ev->name) : std::string();
                // Decision 5(b), ticket #2346 / docs/StandingApprovals.md SA-7. Reading
                // notifyFilter_ here is safe for the same reason reading directory_ is: every
                // reconfiguring member joins the watcher thread before writing (#2344), and the
                // one path that cannot -- a handler reconfiguring the watcher it is running on --
                // is rejected outright (#2347).
                // Ticket #2105 (2026-08-18). inotify delivers many events in one read(), and
                // this loop used to dispatch the WHOLE batch regardless of state -- so a handler
                // that stopped its own watcher still saw the rest of the batch arrive after
                // setEnableRaisingEventsProperty(false) had returned. Measured: 13 further
                // invocations out of a 24-file batch.
                //
                // .NET gates this PER EVENT rather than per batch: Stop() sets `_emitEvents =
                // false` under a lock (FileSystemWatcher.Linux.cs:1071-1093) and QueueEvent
                // returns early on it (`if (!_emitEvents) return;`, :1211-1217). The event is
                // DROPPED, not merely deferred, and the loop keeps walking. `continue` mirrors
                // that. It is NOT load-bearing here and the honest record is that a mutation
                // replacing it with `break` is not caught: the unpaired-rename loop below is
                // gated on the same flag, so the two spellings are observably identical. The
                // reason to prefer `continue` is fidelity to the reference, not a test.
                //
                // The EXTERNAL stop needs no gate of its own: it joins the watch thread
                // (stopWatchingIfRunning), so it cannot return while a handler is running. Only
                // the self-stop path can reach here with enabled_ already false, because joining
                // yourself is a deadlock (#2347).
                if (!enabled_.load()) {
                    i += sizeof(struct inotify_event) + ev->len;
                    continue;
                }

                const bool isDirectory = (ev->mask & IN_ISDIR) != 0;
                const bool nameAdmitted = nameClassAdmits(notifyFilter_, isDirectory);

                if (name.empty() || matchesAnyFilter(filters_, name)) {
                    if (ev->mask & IN_MOVED_FROM) {
                        pendingMovedFrom[ev->cookie] = PendingMove{name, isDirectory};
                    } else if (ev->mask & IN_MOVED_TO) {
                        auto it = pendingMovedFrom.find(ev->cookie);
                        if (it != pendingMovedFrom.end()) {
                            if (nameAdmitted) {
                                RenamedEventArgs args(WatcherChangeTypes::Renamed, directory_, name,
                                                      it->second.name);
                                for (auto& handler : Renamed)
                                    try { handler(this, args); }
                                    catch (...) { reportHandlerFault(std::current_exception()); }
                            }
                            // Erased whether or not it was reported: the pair is resolved either
                            // way, and leaving it behind would resurface as a spurious Deleted.
                            pendingMovedFrom.erase(it);
                        } else if (nameAdmitted) {
                            FileSystemEventArgs args(WatcherChangeTypes::Created, directory_, name);
                            for (auto& handler : Created)
                                try { handler(this, args); }
                                catch (...) { reportHandlerFault(std::current_exception()); }
                        }
                    } else if (ev->mask & IN_CREATE) {
                        if (nameAdmitted) {
                            FileSystemEventArgs args(WatcherChangeTypes::Created, directory_, name);
                            for (auto& handler : Created)
                                try { handler(this, args); }
                                catch (...) { reportHandlerFault(std::current_exception()); }
                        }
                    } else if (ev->mask & IN_DELETE) {
                        if (nameAdmitted) {
                            FileSystemEventArgs args(WatcherChangeTypes::Deleted, directory_, name);
                            for (auto& handler : Deleted)
                                try { handler(this, args); }
                                catch (...) { reportHandlerFault(std::current_exception()); }
                        }
                    } else if (ev->mask & (IN_MODIFY | IN_ATTRIB | IN_ACCESS)) {
                        // IN_ACCESS is in the mask only when LastAccess is named (decision 4(c)),
                        // so its mere arrival means the configured filter admits it.
                        FileSystemEventArgs args(WatcherChangeTypes::Changed, directory_, name);
                        for (auto& handler : Changed)
                            try { handler(this, args); }
                            catch (...) { reportHandlerFault(std::current_exception()); }
                    }
                }

                i += sizeof(struct inotify_event) + ev->len;
            }

            for (const auto& [cookie, pending] : pendingMovedFrom) {
                (void)cookie;
                // #2105: the same gate. A rename whose second half never arrived is reported as
                // a Deleted, and that report is an event like any other.
                if (!enabled_.load()) break;
                // Decision 5(b) again: an unpaired IN_MOVED_FROM is reported as a Deleted, so it
                // is governed by the value that governs a Deleted of the same entry kind.
                if (!nameClassAdmits(notifyFilter_, pending.isDirectory)) continue;
                FileSystemEventArgs args(WatcherChangeTypes::Deleted, directory_, pending.name);
                for (auto& handler : Deleted)
                    try { handler(this, args); }
                    catch (...) { reportHandlerFault(std::current_exception()); }
            }
        }
    }

#elif defined(SHARP_RUNTIME_FSW_DARWIN)

    namespace {
        // The stop signal: an EVFILT_USER event on the watcher's own kqueue, so watchLoop's
        // kevent() wakes without a second descriptor (Linux uses an eventfd for the same job).
        constexpr uintptr_t kDarwinStopIdent = 1;

        // Per-entry vnode events a NotifyFilters value asks for (decision 1(a)/2(a)/3(a) of the
        // Linux mapping above). NOTE_ATTRIB, like IN_ATTRIB, does not say which attribute
        // changed, so it serves every content-class filter -- LastAccess included (AM4-126): an
        // explicit access-time change (utimes) is one of the changes it reports. What kqueue has
        // no counterpart for is IN_ACCESS: a plain read changes no vnode state it reports, so a
        // LastAccess watcher does not see reads, a documented Darwin limitation.
        uint32_t darwinEntryFlagsFor(NotifyFilters filter, bool isDirectory) {
            const int bits = static_cast<int>(filter);
            uint32_t flags = 0;
            // A subdirectory's NOTE_WRITE means ITS entries changed, which inotify on the parent
            // never reports either; only its own attributes count, as IN_ATTRIB does.
            if (!isDirectory && (bits & kWriteServedFilters) != 0) flags |= NOTE_WRITE | NOTE_EXTEND;
            if ((bits & kContentClassFilters) != 0) flags |= NOTE_ATTRIB;
            return flags;
        }

        struct DarwinListedEntry {
            ino_t inode = 0;
            bool isDirectory = false;
            // A regular file or a directory: the only kinds whose vnode is opened for a content
            // watch. Opening a FIFO blocks until a writer appears, and a socket or device has no
            // content Changed could describe (AM4-101).
            bool hasContent = false;
        };

        // Every Darwin watcher's per-entry descriptors, process-wide. kqueue needs one descriptor
        // per watched file, and the default soft limit of a launchd-started process is 256, so a
        // directory of a few hundred files used to exhaust the whole process (EMFILE everywhere).
        // The watchers together now hold at most half the soft limit (AM4-101).
        std::atomic<long> gDarwinEntryDescriptors{0};

        long darwinEntryDescriptorBudget() {
            struct rlimit limit{};
            if (::getrlimit(RLIMIT_NOFILE, &limit) != 0) return 64;
            const rlim_t soft = limit.rlim_cur == RLIM_INFINITY ? (rlim_t{1} << 20) : limit.rlim_cur;
            return static_cast<long>(std::min<rlim_t>(soft / 2, rlim_t{1} << 16));
        }

        void closeDarwinEntryDescriptor(int& fd) {
            if (fd < 0) return;
            ::close(fd);
            fd = -1;
            gDarwinEntryDescriptors.fetch_sub(1);
        }

        bool sameDirectoryVersion(const struct stat& a, const struct stat& b) {
            return a.st_mtimespec.tv_sec == b.st_mtimespec.tv_sec && a.st_mtimespec.tv_nsec == b.st_mtimespec.tv_nsec &&
                   a.st_ctimespec.tv_sec == b.st_ctimespec.tv_sec && a.st_ctimespec.tv_nsec == b.st_ctimespec.tv_nsec;
        }

        // nullopt when the directory cannot be listed at all (removed, renamed away, permissions,
        // no descriptor left): that is not "every entry was deleted" and must not be reported so.
        //
        // AM4-126: readdir is not a snapshot. An entry renamed while the directory is being read
        // can be named by readdir and gone by its lstat, with the new name already passed -- the
        // old name then looks deleted and the new one turns up in the next scan as created, so a
        // rename was reported as Deleted + Created about one time in six. A listing is kept only
        // when the directory's timestamps did not move while it was taken; a directory that keeps
        // changing gets the last of a bounded number of attempts, as before.
        std::optional<std::map<std::string, DarwinListedEntry>> listDarwinDirectory(const std::string& directory) {
            constexpr int kConsistentListingAttempts = 16;
            std::map<std::string, DarwinListedEntry> entries;
            for (int attempt = 1;; ++attempt) {
                entries.clear();
                DIR* dir = ::opendir(directory.c_str());
                if (dir == nullptr) return std::nullopt;
                struct stat before{};
                const bool versioned = ::fstat(::dirfd(dir), &before) == 0;
                while (const dirent* item = ::readdir(dir)) {
                    const std::string name = item->d_name;
                    if (name == "." || name == "..") continue;
                    struct stat info{};
                    if (::lstat((directory + "/" + name).c_str(), &info) != 0) continue;
                    entries[name] = DarwinListedEntry{info.st_ino, S_ISDIR(info.st_mode),
                                                      S_ISREG(info.st_mode) || S_ISDIR(info.st_mode)};
                }
                struct stat after{};
                const bool stable = versioned && ::fstat(::dirfd(dir), &after) == 0 &&
                                    sameDirectoryVersion(before, after);
                ::closedir(dir);
                if (stable || attempt == kConsistentListingAttempts) return entries;
            }
        }

        std::exception_ptr darwinCoverageFault(const std::string& directory, std::size_t unwatched) {
            return std::make_exception_ptr(System::IO::InternalBufferOverflowException(
                "FileSystemWatcher on '" + directory + "': " + std::to_string(unwatched) +
                " entr" + (unwatched == 1 ? "y has" : "ies have") + " no content watch, because the "
                "process-wide budget of per-file descriptors (half of RLIMIT_NOFILE) is spent; Changed "
                "is not raised for them. Raise the descriptor limit or watch a smaller directory."));
        }
    } // namespace

    bool FileSystemWatcher::armDarwinEntry(const std::string& name, bool isDirectory, bool hasContent) {
        DarwinWatchedEntry& entry = darwinEntries_[name];
        entry.isDirectory = isDirectory;
        const uint32_t flags = hasContent ? darwinEntryFlagsFor(notifyFilter_, isDirectory) : 0;
        if (flags == 0 || entry.fd >= 0) return true;
        if (gDarwinEntryDescriptors.fetch_add(1) >= darwinEntryDescriptorBudget()) {
            gDarwinEntryDescriptors.fetch_sub(1);
            return false;
        }
        // O_EVTONLY: a descriptor for event delivery only, which does not keep a volume busy.
        // O_NONBLOCK and O_SYMLINK close the window between the listing's lstat and this open: an
        // entry replaced by a FIFO cannot block the watcher, and a symbolic link is never followed.
        entry.fd = ::open((directory_ + "/" + name).c_str(), O_EVTONLY | O_CLOEXEC | O_NONBLOCK | O_SYMLINK);
        if (entry.fd < 0) {
            gDarwinEntryDescriptors.fetch_sub(1);
            // Gone again already: the next directory scan settles it. Out of descriptors: no
            // content watch, which the caller reports.
            return errno != EMFILE && errno != ENFILE;
        }
        struct kevent change{};
        EV_SET(&change, static_cast<uintptr_t>(entry.fd), EVFILT_VNODE, EV_ADD | EV_CLEAR, flags, 0, nullptr);
        if (::kevent(inotifyFd_, &change, 1, nullptr, 0, nullptr) != 0) closeDarwinEntryDescriptor(entry.fd);
        return true;
    }

    void FileSystemWatcher::releaseDarwinEntries() {
        for (auto& [name, entry] : darwinEntries_) {
            (void)name;
            closeDarwinEntryDescriptor(entry.fd);
        }
        darwinEntries_.clear();
    }

    void FileSystemWatcher::startWatchingIfPossible() {
        // Same preconditions, in the same order, as the Linux backend above.
        if (directory_.empty()) return;
        if (onWatcherThread()) return;
        reapSelfStoppedThread();
        if (watchThread_.joinable()) return;
        if ((static_cast<int>(notifyFilter_) & (kNameClassFilters | kContentClassFilters)) == 0) return;

        const auto failArming = [this](const std::string& message) {
            releaseDarwinEntries();
            if (watchDescriptor_ >= 0) ::close(watchDescriptor_);
            if (inotifyFd_ >= 0) ::close(inotifyFd_);
            inotifyFd_ = watchDescriptor_ = -1;
            enabled_.store(false);
            if (!Error.empty()) {
                auto ex = std::make_exception_ptr(System::IO::IOException(message));
                System::IO::ErrorEventArgs args(ex);
                for (auto& handler : Error) handler(this, args);
            }
        };

        inotifyFd_ = ::kqueue();
        if (inotifyFd_ < 0) {
            failArming("Unable to initialize kqueue: " + std::string(std::strerror(errno)));
            return;
        }
        watchDescriptor_ = ::open(directory_.c_str(), O_EVTONLY | O_CLOEXEC);
        if (watchDescriptor_ < 0) {
            failArming("Unable to watch directory '" + directory_ + "': " + std::string(std::strerror(errno)));
            return;
        }
        struct kevent changes[2]{};
        EV_SET(&changes[0], static_cast<uintptr_t>(watchDescriptor_), EVFILT_VNODE, EV_ADD | EV_CLEAR,
               NOTE_WRITE | NOTE_EXTEND | NOTE_LINK, 0, nullptr);
        EV_SET(&changes[1], kDarwinStopIdent, EVFILT_USER, EV_ADD | EV_CLEAR, NOTE_FFNOP, 0, nullptr);
        if (::kevent(inotifyFd_, changes, 2, nullptr, 0, nullptr) != 0) {
            failArming("Unable to watch directory '" + directory_ + "': " + std::string(std::strerror(errno)));
            return;
        }

        // The snapshot is taken HERE, before EnableRaisingEvents returns, so an entry created the
        // moment after is a difference the first scan reports rather than part of the baseline.
        const auto snapshot = listDarwinDirectory(directory_);
        if (!snapshot) {
            failArming("Unable to list directory '" + directory_ + "': " + std::string(std::strerror(errno)));
            return;
        }
        std::size_t unwatched = 0;
        for (const auto& [name, listed] : *snapshot) {
            if (!armDarwinEntry(name, listed.isDirectory, listed.hasContent)) ++unwatched;
            darwinEntries_[name].inode = listed.inode;
        }
        if (unwatched != 0) reportHandlerFault(darwinCoverageFault(directory_, unwatched));

        try {
            watchThread_ = std::thread(&FileSystemWatcher::watchLoop, this);
        } catch (const std::system_error&) {
            failArming("Unable to start the file system watcher thread.");
        }
    }

    void FileSystemWatcher::reapSelfStoppedThread() {
        if (!selfStopPending_.load() || onWatcherThread()) return;
        if (watchThread_.joinable()) watchThread_.join();
        releaseDarwinEntries();
        if (watchDescriptor_ >= 0) ::close(watchDescriptor_);
        if (inotifyFd_ >= 0) ::close(inotifyFd_);
        inotifyFd_ = watchDescriptor_ = -1;
        selfStopPending_.store(false);
    }

    void FileSystemWatcher::stopWatchingIfRunning() {
        // Ticket #2347's rules, unchanged from the Linux backend: signal, never self-join.
        const bool selfStop = onWatcherThread();
        if (!selfStop) {
            reapSelfStoppedThread();
            if (!watchThread_.joinable()) return;
        }
        if (inotifyFd_ >= 0) {
            struct kevent trigger{};
            EV_SET(&trigger, kDarwinStopIdent, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
            (void)::kevent(inotifyFd_, &trigger, 1, nullptr, 0, nullptr);
        }
        if (selfStop) {
            selfStopPending_.store(true);
            return;
        }
        watchThread_.join();
        releaseDarwinEntries();
        if (watchDescriptor_ >= 0) ::close(watchDescriptor_);
        if (inotifyFd_ >= 0) ::close(inotifyFd_);
        inotifyFd_ = watchDescriptor_ = -1;
    }

    void FileSystemWatcher::watchLoop() {
        const WatchThreadMarker marker(this);
        const auto raise = [this](auto& handlers, const auto& args) {
            for (auto& handler : handlers)
                try { handler(this, args); }
                catch (...) { reportHandlerFault(std::current_exception()); }
        };

        bool listingFailed = false;
        for (;;) {
            struct kevent events[32];
            const int count = ::kevent(inotifyFd_, nullptr, 0, events, 32, nullptr);
            if (count < 0) {
                if (errno == EINTR) continue;
                return;
            }
            bool directoryChanged = false;
            std::vector<std::string> contentChanged;
            for (int i = 0; i < count; ++i) {
                if (events[i].filter == EVFILT_USER) return; // stop requested
                const int fd = static_cast<int>(events[i].ident);
                if (fd == watchDescriptor_) { directoryChanged = true; continue; }
                for (const auto& [name, entry] : darwinEntries_)
                    if (entry.fd == fd) { contentChanged.push_back(name); break; }
            }
            // #2105's per-event gate: a handler that stopped this watcher sees nothing more.
            if (!enabled_.load()) continue;

            if (directoryChanged) {
                const auto listing = listDarwinDirectory(directory_);
                if (!listing) {
                    // The directory itself went away or became unreadable. Its entries were not
                    // deleted by that, so none is reported; the snapshot stays, and Error says why
                    // -- once per failure, not on every later event.
                    if (!listingFailed) {
                        listingFailed = true;
                        reportHandlerFault(std::make_exception_ptr(System::IO::IOException(
                            "FileSystemWatcher can no longer list '" + directory_ + "': " +
                            std::string(std::strerror(errno)))));
                    }
                    continue;
                }
                listingFailed = false;
                const auto& listed = *listing;
                std::vector<std::pair<std::string, DarwinWatchedEntry>> removed;
                for (const auto& [name, entry] : darwinEntries_)
                    if (listed.find(name) == listed.end() || listed.at(name).inode != entry.inode)
                        removed.emplace_back(name, entry);
                std::vector<std::pair<std::string, DarwinListedEntry>> added;
                for (const auto& [name, entry] : listed) {
                    const auto known = darwinEntries_.find(name);
                    if (known == darwinEntries_.end() || known->second.inode != entry.inode)
                        added.emplace_back(name, entry);
                }
                for (const auto& [name, entry] : removed) {
                    (void)entry;
                    darwinEntries_.erase(name);
                }

                // A removed inode that reappears under another name in the same scan is one
                // rename, as inotify's MOVED_FROM/MOVED_TO cookie pair is; its event descriptor
                // follows the vnode, so it moves with the entry. The filter rules are the Linux
                // backend's: the old name must match to start a pair, an unpaired half is a
                // Deleted or a Created, and a pair whose new name does not match is the old
                // name's Deleted (AM4-127) -- on Linux its IN_MOVED_TO is filtered out, so the
                // IN_MOVED_FROM is left unpaired and reported as one.
                std::vector<std::string> renamedOnto;
                for (auto removedIt = removed.begin(); removedIt != removed.end();) {
                    const auto addedIt = std::find_if(added.begin(), added.end(), [&](const auto& a) {
                        return a.second.inode == removedIt->second.inode;
                    });
                    if (addedIt == added.end() || !matchesAnyFilter(filters_, removedIt->first)) {
                        ++removedIt;
                        continue;
                    }
                    DarwinWatchedEntry moved = removedIt->second;
                    renamedOnto.push_back(addedIt->first);
                    darwinEntries_[addedIt->first] = moved;
                    if (enabled_.load() && nameClassAdmits(notifyFilter_, moved.isDirectory)) {
                        if (matchesAnyFilter(filters_, addedIt->first)) {
                            RenamedEventArgs args(WatcherChangeTypes::Renamed, directory_, addedIt->first,
                                                  removedIt->first);
                            raise(Renamed, args);
                        } else {
                            FileSystemEventArgs args(WatcherChangeTypes::Deleted, directory_, removedIt->first);
                            raise(Deleted, args);
                        }
                    }
                    added.erase(addedIt);
                    removedIt = removed.erase(removedIt);
                }
                for (auto& [name, entry] : removed) {
                    closeDarwinEntryDescriptor(entry.fd);
                    // An entry replaced by a rename onto its name (rename(tmp, name), the usual
                    // atomic save) is one Renamed, as inotify reports it: MOVED_TO overwrites the
                    // old entry without an IN_DELETE of its own.
                    if (std::find(renamedOnto.begin(), renamedOnto.end(), name) != renamedOnto.end()) continue;
                    if (!enabled_.load() || !matchesAnyFilter(filters_, name) ||
                        !nameClassAdmits(notifyFilter_, entry.isDirectory)) continue;
                    FileSystemEventArgs args(WatcherChangeTypes::Deleted, directory_, name);
                    raise(Deleted, args);
                }
                std::size_t unwatched = 0;
                for (const auto& [name, entry] : added) {
                    if (!armDarwinEntry(name, entry.isDirectory, entry.hasContent)) ++unwatched;
                    darwinEntries_[name].inode = entry.inode;
                    if (!enabled_.load() || !matchesAnyFilter(filters_, name) ||
                        !nameClassAdmits(notifyFilter_, entry.isDirectory)) continue;
                    FileSystemEventArgs args(WatcherChangeTypes::Created, directory_, name);
                    raise(Created, args);
                }
                if (unwatched != 0 && enabled_.load()) reportHandlerFault(darwinCoverageFault(directory_, unwatched));
            }

            for (const auto& name : contentChanged) {
                if (!enabled_.load()) break;
                if (darwinEntries_.find(name) == darwinEntries_.end()) continue; // removed above
                if (!matchesAnyFilter(filters_, name)) continue;
                FileSystemEventArgs args(WatcherChangeTypes::Changed, directory_, name);
                raise(Changed, args);
            }
        }
    }

#else // !SHARP_RUNTIME_FSW_LINUX && !SHARP_RUNTIME_FSW_DARWIN

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
