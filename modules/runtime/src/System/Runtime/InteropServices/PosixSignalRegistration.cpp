// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Runtime/InteropServices/PosixSignalRegistration.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/PlatformNotSupportedException.hpp"
#include "System/IO/IOException.hpp"

#if defined(_WIN32) || defined(__EMSCRIPTEN__)
// No POSIX signal handling on Windows (real .NET uses a separate console-control-handler
// mechanism there, out of scope for this pass -- see the platform-not-supported fallback below)
// or under Emscripten (no OS-level signal delivery in a browser sandbox).
#else
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <mutex>
#include <thread>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <vector>

namespace System::Runtime::InteropServices {
namespace {

    // One past the highest native signal number this dispatcher can record a pending flag for.
    // 65 covers Linux/glibc's whole range including SIGRTMAX (64); a raw number at or above it is
    // refused by toNativeSignalNumber() rather than accepted and then never delivered.
    constexpr int kMaxSignalNumber = 65;

    // Maps a PosixSignal to this platform's actual signal number, or 0 if it cannot be handled.
    //
    // PosixSignal has TWO valid spellings and both must map to the same number:
    //
    //   * a NEGATIVE named member -- Sighup=-1 .. Sigtstp=-10, contiguous, matching the table's
    //     index 0..9 -- looked up below. Sigkill(-11) is intentionally excluded: it can never be
    //     caught (sigaction(SIGKILL, ...) always fails with EINVAL), so Create() rejects it
    //     before reaching this function. The table is a lookup rather than a switch so this
    //     function reads naturally with the real SIGxxx macros this translation unit needs
    //     anyway (see PosixSignal.hpp for why the members are spelled Sighup/Sigint/etc.);
    //
    //   * a POSITIVE raw native signal number, which current .NET's Unix contract expressly
    //     permits a caller to cast to PosixSignal, and which this function used to reject --
    //     including static_cast<PosixSignal>(SIGWINCH), the positive spelling of a signal the
    //     port already supports under the name PosixSignal::Sigwinch. That asymmetry is the
    //     defect ticket #1977 / SR-AUD-170 repairs: both spellings now return the same number,
    //     so they share a dispatch bucket and an installed handler.
    //
    // Everything else -- zero, an out-of-range positive, an unnamed negative -- returns 0 and
    // Create() reports PlatformNotSupportedException, exactly as before.
    int toNativeSignalNumber(PosixSignal signal) {
        static const int kNativeSignalNumbers[10] = {
            SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGCHLD, SIGCONT, SIGWINCH, SIGTTIN, SIGTTOU, SIGTSTP
        };
        int value = static_cast<int>(signal);
        if (value > 0) {
            // The upper bound is the pending_ array's, not the OS's: a number this dispatcher
            // cannot record a pending flag for must be refused outright rather than accepted and
            // then silently never delivered. Whether the OS will actually let us catch it is
            // decided by sigaction() in installIfNeeded().
            return (value < kMaxSignalNumber) ? value : 0;
        }
        int index = -value - 1;
        if (index < 0 || index >= 10) return 0;
        return kNativeSignalNumbers[index];
    }

    // SIGKILL and SIGSTOP cannot be caught or ignored on any POSIX system. The named
    // PosixSignal::Sigkill was already rejected by Create(); accepting raw positive values makes
    // both reachable by number for the first time, so both are named here.
    bool isUncatchableRawSignal(int signo) { return signo == SIGKILL || signo == SIGSTOP; }

    struct Entry {
        SharpRuntime::intcs token;
        PosixSignal signal;
        PosixSignalRegistration::Handler handler;
    };

    // What was installed for one signal before this port took it over. Recording only the
    // signal number -- which is all installedSignals_ used to hold -- makes it impossible to
    // put the process back the way it was found, so uninstalling degenerated into imposing
    // SIG_DFL. That is the OS default, not "undo": for most catchable signals the default
    // disposition TERMINATES the process, so a program that had deliberately set SIG_IGN on
    // SIGHUP was killed by the next SIGHUP once an unrelated component disposed its last
    // registration (ticket #1975 / SR-AUD-169). The whole struct sigaction is kept, not just
    // sa_handler, because sa_flags and sa_mask are equally part of the disposition.
    struct InstalledSignal {
        int signo;
        struct sigaction previous;
    };

    // Guards both registrations_ (the C++-visible table) and installedSignals_ (which raw signal
    // numbers currently have our sigaction handler installed, and what they had before).
    std::mutex registryMutex_;
    std::vector<Entry> registrations_;
    std::vector<InstalledSignal> installedSignals_;
    SharpRuntime::intcs nextToken_ = 0;

    // Tokens whose handler some dispatch has already copied into its snapshot and may still
    // invoke. Guarded by registryMutex_. Dispose() waits on inFlightCv_ until its own token is
    // gone from here, so it cannot return while a pending invocation of that handler remains
    // (ticket #1986). Erasing the registration alone is not enough: the snapshot holds a COPY
    // of the std::function, so the erase stops future dispatches but not the pending one.
    std::vector<SharpRuntime::intcs> inFlightTokens_;
    std::condition_variable inFlightCv_;

    // True only on the watcher thread, and only between taking a dispatch snapshot and
    // finishing the last handler in it. A handler is expressly allowed to dispose itself or a
    // sibling registration, and every token in the running snapshot is in flight by
    // definition, so a Dispose() reached from inside a callback must NOT wait -- that would be
    // the watcher thread waiting for itself. This is why the repair cannot simply hold
    // registryMutex_ across invocation, which is what the snapshot exists to avoid.
    thread_local bool dispatchingOnThisThread_ = false;

    // Async-signal-safe pending-signal flags, indexed directly by native signal number. sig_atomic_t
    // writes/reads are guaranteed atomic with respect to signal delivery on the same thread by the
    // C standard; combined with the self-pipe write below, this is the standard safe pattern for
    // moving work out of a signal handler (POSIX.1 "Signal Concepts" / the classic self-pipe trick).
    volatile std::sig_atomic_t pending_[kMaxSignalNumber] = {};

    int selfPipe_[2] = {-1, -1};
    std::thread watcherThread_;
    std::atomic<bool> watcherRunning_{false};

    /**
     * True for the three signals whose ORIGINAL disposition .NET treats as "would terminate the
     * app, and that termination is what PosixSignal lets you cancel".
     * `IsCancelableTerminationSignal`, `pal_signal.c:53-58`.
     */
    static bool isCancelableTerminationSignal(int signo) {
        return signo == SIGINT || signo == SIGQUIT || signo == SIGTERM;
    }

    /**
     * The saved original disposition for @p signo, or nullptr if this signal is not installed.
     * Must only be read where the registry lock is held, or from the signal handler, where
     * §1979's comment explains why it is safe.
     */
    const struct sigaction* originalDispositionFor(int signo) {
        for (const auto& installed : installedSignals_) {
            if (installed.signo == signo) return &installed.previous;
        }
        return nullptr;
    }

    static bool isSigDfl(const struct sigaction& action) {
        return (action.sa_flags & SA_SIGINFO) == 0 && action.sa_handler == SIG_DFL;
    }
    static bool isSigIgn(const struct sigaction& action) {
        return (action.sa_flags & SA_SIGINFO) == 0 && action.sa_handler == SIG_IGN;
    }

    void onNativeSignal(int signo) {
        // Ticket #1979 / SR-AUD-169, second half. .NET invokes the original disposition
        // IMMEDIATELY, inside the raw handler, for every signal that is not one of the three
        // cancelable termination signals -- `SignalHandler`, `pal_signal.c:229-246`, whose own
        // comment is "For other signals, we immediately invoke the original handler." Doing it
        // here rather than after the callbacks is what makes the callbacks observers of a signal
        // the process was already going to handle, instead of a gate in front of it.
        //
        // Reading installedSignals_ without the lock is deliberate and is the same trade .NET
        // makes: taking a mutex inside a signal handler is not async-signal-safe, and the vector
        // is only mutated while installing or uninstalling, which a program does not do
        // concurrently with the delivery it is installing for. The alternative -- a lock -- would
        // be a deadlock hazard in exactly the place it must not be.
        if (!isCancelableTerminationSignal(signo)) {
            if (const struct sigaction* original = originalDispositionFor(signo)) {
                if (!isSigDfl(*original) && !isSigIgn(*original)) {
                    if ((original->sa_flags & SA_SIGINFO) != 0) {
                        if (original->sa_sigaction != nullptr) original->sa_sigaction(signo, nullptr, nullptr);
                    } else if (original->sa_handler != nullptr) {
                        original->sa_handler(signo);
                    }
                }
            }
        }

        if (signo >= 0 && signo < kMaxSignalNumber) pending_[signo] = 1;
        char byte = 0;
        // write() to a pipe is async-signal-safe; ignore errors. EAGAIN on a full pipe just means
        // the watcher thread hasn't drained a previous wakeup yet, which is fine -- pending_ above
        // already recorded this delivery, so the dropped byte carries no information the watcher
        // does not already have. This is only true because ensureWatcherStarted() makes the write
        // end O_NONBLOCK; on a blocking descriptor write() would not return EAGAIN at all, it would
        // BLOCK here, inside a raw signal handler, on whichever thread the OS chose for delivery
        // (ticket #1974 / SR-AUD-172).
        ssize_t ignored = ::write(selfPipe_[1], &byte, 1);
        (void)ignored;
    }

    // Defined below; declared here because dispatchSignal calls the first and it, in turn,
    // calls the second.
    void handleNonCanceledPosixSignal(int signo);
    void reinstallHandlerLocked(int signo);

    void dispatchSignal(int signo) {
        std::vector<Entry> snapshot;
        {
            std::lock_guard<std::mutex> lock(registryMutex_);
            for (const auto& e : registrations_)
                if (toNativeSignalNumber(e.signal) == signo) snapshot.push_back(e);
            if (snapshot.empty()) return;
            // Published under the SAME lock that produced the snapshot, so there is no window
            // in which a copied handler exists but a concurrent Dispose() cannot see it.
            for (const auto& e : snapshot) inFlightTokens_.push_back(e.token);
        }

        // Retires this dispatch's in-flight records on every exit path. RAII rather than a
        // trailing statement because an exception escaping a handler would otherwise strand a
        // token in flight forever and wedge every later Dispose() of it.
        struct InFlightRetire {
            const std::vector<Entry>& entries;
            ~InFlightRetire() {
                {
                    std::lock_guard<std::mutex> lock(registryMutex_);
                    for (const auto& e : entries) {
                        auto it = std::find(inFlightTokens_.begin(), inFlightTokens_.end(), e.token);
                        if (it != inFlightTokens_.end()) inFlightTokens_.erase(it);
                    }
                }
                dispatchingOnThisThread_ = false;
                inFlightCv_.notify_all();
            }
        } retire{snapshot};
        dispatchingOnThisThread_ = true;

        // Reverse registration order, matching real .NET's PosixSignalRegistration.Unix.cs.
        PosixSignalContext context(snapshot.back().signal);
        bool canceled = false;
        for (auto it = snapshot.rbegin(); it != snapshot.rend(); ++it) {
            context.setSignalProperty(it->signal);
            it->handler(context);
            if (context.getCancelProperty()) canceled = true;
        }

        if (!canceled) {
            handleNonCanceledPosixSignal(signo);
        }
    }

    /**
     * @brief What happens after the callbacks return without cancelling.
     *
     * Ticket #1979 / SR-AUD-169 and SR-AUD-171. Transcribed from
     * `SystemNative_HandleNonCanceledPosixSignal` (`pal_signal.c:260-315`), which the design in
     * `docs/SystemRuntimeNamespaceReviewPlan.md` §10.1 could only propose from a reading taken
     * when the reference tree was absent. It is present now, and it corrects the proposal in two
     * places -- see §10.1.1.
     *
     * Before this, EVERY non-cancelled delivery ran `std::signal(signo, SIG_DFL); ::raise(signo);`
     * unconditionally. For `SIGTSTP`/`SIGTTIN`/`SIGTTOU` the default disposition is Stop, so
     * merely OBSERVING a job-control signal suspended the observing process -- measured,
     * `child_stopped_by=20 (WIFSTOPPED)` with `callbacks=1`.
     */
    void handleNonCanceledPosixSignal(int signo) {
        switch (signo) {
            case SIGCONT:
                // Default disposition is Continue. .NET additionally reinitializes the terminal
                // here; this port owns no terminal state, so there is nothing to reinitialize.
                break;
            case SIGTSTP:
            case SIGTTIN:
            case SIGTTOU:
                // Default disposition is Stop. THE HEADLINE OF THIS TICKET: no-op, so observing
                // a job-control signal no longer suspends the observer.
                break;
            case SIGCHLD:
            case SIGURG:
            case SIGWINCH:
                // Default disposition is Ignore. Raising these was harmless in effect but still
                // wrong in mechanism, because the raise ran with the handler uninstalled.
                break;
            default: {
                // Default disposition is Terminate.
                const struct sigaction* original = nullptr;
                {
                    std::lock_guard<std::mutex> lock(registryMutex_);
                    if (const struct sigaction* found = originalDispositionFor(signo)) {
                        static thread_local struct sigaction copy;
                        copy = *found;
                        original = &copy;
                    }
                }
                if (original != nullptr) {
                    if (!isCancelableTerminationSignal(signo) && !isSigDfl(*original)) {
                        // Already invoked in onNativeSignal, exactly as .NET records at
                        // pal_signal.c:293-297 ("We've already called the original handler in
                        // SignalHandler").
                        break;
                    }
                    if (isSigIgn(*original)) break;   // the original does nothing
                    // Restore the ORIGINAL disposition -- not SIG_DFL, which is what this code
                    // used to impose -- and re-raise into it.
                    std::lock_guard<std::mutex> lock(registryMutex_);
                    sigaction(signo, original, nullptr);
                } else {
                    std::signal(signo, SIG_DFL);
                }
                ::raise(signo);
                // Only reached when the restored disposition did not terminate. Reinstall our
                // handler so later deliveries are still observed.
                {
                    std::lock_guard<std::mutex> lock(registryMutex_);
                    reinstallHandlerLocked(signo);
                }
                break;
            }
        }
    }

    void watcherLoop() {
        while (watcherRunning_.load()) {
            char buf[64];
            ssize_t n = ::read(selfPipe_[0], buf, sizeof(buf));
            if (n <= 0) {
                if (!watcherRunning_.load()) break;
                continue;
            }
            for (int signo = 0; signo < kMaxSignalNumber; ++signo) {
                if (pending_[signo]) {
                    pending_[signo] = 0;
                    dispatchSignal(signo);
                }
            }
        }
    }

    void ensureWatcherStarted() {
        // Caller already holds registryMutex_.
        if (watcherRunning_.load()) return;
        if (::pipe(selfPipe_) != 0) {
            throw System::IO::IOException("Failed to initialize POSIX signal handling (pipe() failed).");
        }
        // The WRITE end must be non-blocking: onNativeSignal() runs inside a raw signal handler,
        // and a blocking write() there hangs the delivering thread as soon as the finite pipe
        // buffer fills -- deadlocking the process if that thread holds a lock the watcher or a
        // handler needs. Losing the byte is harmless because pending_[signo] is already set.
        // The READ end stays BLOCKING on purpose: watcherLoop()'s read() is how the watcher
        // parks, and making it non-blocking would turn it into a spin loop.
        int writeFlags = ::fcntl(selfPipe_[1], F_GETFL, 0);
        if (writeFlags == -1 || ::fcntl(selfPipe_[1], F_SETFL, writeFlags | O_NONBLOCK) != 0) {
            int savedErrno = errno;
            ::close(selfPipe_[0]);
            ::close(selfPipe_[1]);
            selfPipe_[0] = selfPipe_[1] = -1;
            errno = savedErrno;
            throw System::IO::IOException(
                "Failed to initialize POSIX signal handling (could not make the self-pipe non-blocking).");
        }
        // Both ends must also be close-on-exec. Without it they survive an exec() in a child,
        // which leaves that child holding a WRITE end to a pipe whose watcher thread does not
        // exist in it -- fork() gives the child no watcher -- and a READ end that keeps the
        // pipe alive for as long as the child does. Neither descriptor has any meaning after
        // exec(), so both are pure leak (ticket #1985).
        //
        // Done with fcntl rather than pipe2(O_CLOEXEC) to stay portable to POSIX platforms
        // without pipe2 (notably macOS, which this port's downstream Apple Clang builds use),
        // and to match the O_NONBLOCK handling immediately above. The cost is that setting the
        // flag is not atomic with pipe(): a fork+exec on another thread landing in that window
        // still inherits the pair. The window is two adjacent statements, entered once per
        // process at the first Create() and under registryMutex_, and a fork landing in it is
        // no worse than the unconditional inheritance this replaces.
        for (int end = 0; end < 2; ++end) {
            int fdFlags = ::fcntl(selfPipe_[end], F_GETFD, 0);
            if (fdFlags == -1 || ::fcntl(selfPipe_[end], F_SETFD, fdFlags | FD_CLOEXEC) != 0) {
                int savedErrno = errno;
                ::close(selfPipe_[0]);
                ::close(selfPipe_[1]);
                selfPipe_[0] = selfPipe_[1] = -1;
                errno = savedErrno;
                throw System::IO::IOException(
                    "Failed to initialize POSIX signal handling (could not make the self-pipe close-on-exec).");
            }
        }
        watcherRunning_.store(true);
        watcherThread_ = std::thread(watcherLoop);
        watcherThread_.detach();
    }

    /**
     * Re-arms this port's handler for @p signo after handleNonCanceledPosixSignal restored the
     * original disposition and re-raised into it, WITHOUT disturbing the saved original: the
     * entry in installedSignals_ still describes what the process had before this port touched
     * it, and that is what Dispose() must put back.
     *
     * Caller already holds registryMutex_.
     */
    void reinstallHandlerLocked(int signo) {
        for (const auto& installed : installedSignals_) {
            if (installed.signo != signo) continue;
            struct sigaction sa{};
            sa.sa_handler = onNativeSignal;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = SA_RESTART;
            sigaction(signo, &sa, nullptr);
            return;
        }
    }

    void installIfNeeded(int signo) {
        // Caller already holds registryMutex_.
        for (const auto& s : installedSignals_) if (s.signo == signo) return;
        struct sigaction sa{};
        sa.sa_handler = onNativeSignal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        // The third argument is sigaction()'s oldact out-parameter. Passing nullptr here -- which
        // is what this code used to do -- discards the process's existing disposition with no way
        // to get it back.
        struct sigaction previous{};
        if (sigaction(signo, &sa, &previous) != 0) {
            throw System::IO::IOException("Failed to install handler for the requested POSIX signal.");
        }
        installedSignals_.push_back(InstalledSignal{signo, previous});
    }

    void uninstallIfUnused(int signo) {
        // Caller already holds registryMutex_.
        for (const auto& e : registrations_)
            if (toNativeSignalNumber(e.signal) == signo) return; // still in use

        auto it = std::find_if(installedSignals_.begin(), installedSignals_.end(),
                                [signo](const InstalledSignal& s) { return s.signo == signo; });
        if (it == installedSignals_.end()) return;

        // Restore exactly what was there before the first registration for this signal, including
        // sa_flags and sa_mask -- not SIG_DFL. Restoring SIG_IGN as SIG_IGN is the case that
        // matters most: SIGHUP's default disposition terminates, so imposing SIG_DFL on a process
        // that had deliberately ignored SIGHUP arms an unrelated kill.
        struct sigaction previous = it->previous;
        sigaction(signo, &previous, nullptr);
        installedSignals_.erase(it);
    }

} // namespace

PosixSignalRegistration::PosixSignalRegistration(PosixSignalRegistration&& other) noexcept
    : signal_(other.signal_), token_(other.token_) {
    other.token_ = -1;
}

PosixSignalRegistration& PosixSignalRegistration::operator=(PosixSignalRegistration&& other) noexcept {
    if (this != &other) {
        Dispose();
        signal_ = other.signal_;
        token_ = other.token_;
        other.token_ = -1;
    }
    return *this;
}

PosixSignalRegistration::~PosixSignalRegistration() { Dispose(); }

PosixSignalRegistration PosixSignalRegistration::Create(PosixSignal signal, Handler handler) {
    if (!handler) throw System::ArgumentNullException("handler");
    if (signal == PosixSignal::Sigkill)
        throw System::PlatformNotSupportedException("SIGKILL cannot be caught or ignored.");

    int signo = toNativeSignalNumber(signal);
    if (signo == 0) throw System::PlatformNotSupportedException();
    // Reachable only through the raw positive spelling: PosixSignal has no named SIGSTOP member,
    // and named Sigkill is rejected above. Reported the same way as named Sigkill rather than
    // being left to sigaction()'s EINVAL, so the two spellings of SIGKILL agree.
    if (isUncatchableRawSignal(signo)) {
        throw System::PlatformNotSupportedException(
            signo == SIGKILL ? "SIGKILL cannot be caught or ignored."
                             : "SIGSTOP cannot be caught or ignored.");
    }

    std::lock_guard<std::mutex> lock(registryMutex_);
    ensureWatcherStarted();
    installIfNeeded(signo);

    SharpRuntime::intcs token = nextToken_++;
    registrations_.push_back(Entry{token, signal, std::move(handler)});
    return PosixSignalRegistration(signal, token);
}

void PosixSignalRegistration::Dispose() {
    if (token_ < 0) return;
    SharpRuntime::intcs token = token_;
    token_ = -1;

    std::unique_lock<std::mutex> lock(registryMutex_);
    int signo = toNativeSignalNumber(signal_);
    registrations_.erase(
        std::remove_if(registrations_.begin(), registrations_.end(),
                        [token](const Entry& e) { return e.token == token; }),
        registrations_.end());

    // The erase above stops FUTURE dispatches from seeing this registration, but a dispatch
    // that already took its snapshot holds a copy of the handler and will still invoke it.
    // Returning here would let the caller destroy whatever that handler captured -- typically
    // its own stack frame, which is how every handler in this port's tests is written -- while
    // the call is still pending; ASan reports the resulting stack-use-after-scope
    // (build-probe/1986_probe2_before.log). So wait until no pending invocation of this token
    // remains. Ticket #1986.
    //
    // Not waited on when the caller IS the dispatching thread: see dispatchingOnThisThread_.
    // Because dispatch runs on the single watcher thread, no two dispatches can wait on each
    // other, so this wait cannot form a cycle among dispatches.
    if (!dispatchingOnThisThread_) {
        inFlightCv_.wait(lock, [token] {
            return std::find(inFlightTokens_.begin(), inFlightTokens_.end(), token)
                   == inFlightTokens_.end();
        });
    }

    uninstallIfUnused(signo);
}

} // namespace System::Runtime::InteropServices

#endif // !_WIN32 && !__EMSCRIPTEN__

#if defined(_WIN32) || defined(__EMSCRIPTEN__)
namespace System::Runtime::InteropServices {

PosixSignalRegistration::PosixSignalRegistration(PosixSignalRegistration&& other) noexcept
    : signal_(other.signal_), token_(other.token_) {
    other.token_ = -1;
}

PosixSignalRegistration& PosixSignalRegistration::operator=(PosixSignalRegistration&& other) noexcept {
    signal_ = other.signal_;
    token_ = other.token_;
    other.token_ = -1;
    return *this;
}

PosixSignalRegistration::~PosixSignalRegistration() = default;

PosixSignalRegistration PosixSignalRegistration::Create(PosixSignal, Handler handler) {
    if (!handler) throw System::ArgumentNullException("handler");
    throw System::PlatformNotSupportedException(
        "POSIX signal handling is not implemented on this platform in this port.");
}

void PosixSignalRegistration::Dispose() { token_ = -1; }

} // namespace System::Runtime::InteropServices
#endif
