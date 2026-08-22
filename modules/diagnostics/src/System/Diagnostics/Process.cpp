// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Diagnostics/Process.hpp"
#include <atomic>
#include <mutex>
#include <poll.h>
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/PlatformNotSupportedException.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#  include <sys/wait.h>
#  include <sys/types.h>
#  include <unistd.h>
#  include <signal.h>
#  include <fcntl.h>
#  include <cerrno>
#  include <cstring>
#  include <dirent.h>
#  if defined(__APPLE__)
#    include <crt_externs.h>
#  endif
#  define SHARP_RUNTIME_PROCESS_POSIX 1
#endif

namespace System::Diagnostics {

/**
 * @brief The .NET `Timeout.Infinite` constant: a millisecond timeout of -1 waits indefinitely.
 *
 * Spelled here rather than included from `System::Threading::Timeout` because this component
 * depends only on `Core.Base`; pulling the Threading header in would add a module edge for a
 * single integer, and a test-only edge would do the same to the module graph. The value is
 * pinned behaviourally instead: tests assert that -1 waits indefinitely and that -2 and
 * INTCS_MIN are rejected, so a drift from `System::Threading::Timeout::Infinite` would fail.
 */
constexpr intcs TimeoutInfinite = -1;

struct Process::Impl {
    ProcessStartInfo startInfo;
    /**
     * Set by the destructor before it joins, and observed by the pipe readers between poll
     * slices (ticket #2029).
     *
     * Without it, destroying a redirected Process BLOCKED FOR THE CHILD'S WHOLE LIFETIME:
     * `read()` on the pipe cannot return until the child closes its end, so joining the reader
     * meant waiting for the child. Measured, 2005 ms for a 2 s child and unbounded in general.
     *
     * .NET does not do that. `Process.Close()` (`Process.cs:761-805`) stops watching for exit,
     * releases the handle and **cancels** the async read before disposing the stream -- it never
     * waits for the child. This flag is that cancellation, expressed with the tools a C++ pipe
     * reader has.
     */
    std::atomic<bool> readersStopping{false};
#if defined(SHARP_RUNTIME_PROCESS_POSIX)
    pid_t pid = -1;
#endif
    bool started = false;
    bool hasExited = false;
    bool isCurrentProcess = false;
    intcs exitCode = 0;
    /**
     * Guards `stdoutText`/`stderrText` against the pipe readers (ticket #2030).
     *
     * Both getters used to return a `const std::string&` INTO storage an internal thread was
     * still appending to -- measured, the SAME reference read 4 bytes mid-run and 8 bytes after
     * exit. There is no reference a caller can hold safely while another thread appends, so the
     * getters return by value now and take this lock to make the copy.
     *
     * `mutable` because both getters are `const`, which this class means as "does not change the
     * observable process state", not as "is thread-safe" -- see `stateMutex`.
     */
    mutable std::mutex outputMutex;
    std::string stdoutText;
    std::string stderrText;

    /**
     * Guards `hasExited`/`exitCode` (ticket #2030's second half).
     *
     * `getHasExitedProperty()` is `const` and yet MUTATES both, through `reapIfNeeded`. That is
     * .NET's shape too -- `HasExited` reaps lazily -- so the repair is not to stop mutating but
     * to make the mutation safe against a concurrent reader.
     */
    mutable std::mutex stateMutex;
    std::thread stdoutReader;
    std::thread stderrReader;

    /**
     * Ticket #2029. The join stays -- a DETACHED reader would keep writing into `stdoutText`
     * after this object is gone, which is a use-after-free and would trade one defect for a
     * worse one -- but it is now bounded, because `readersStopping` makes each reader return
     * within one poll slice instead of waiting for the child to exit.
     *
     * What this deliberately does NOT do is reap a still-running child. .NET reaps process-wide
     * from a SIGCHLD-driven wait state (`ProcessWaitState.Unix.cs`), which this port cannot
     * replicate without colliding with `PosixSignalRegistration` (#1975/#1979) -- so a child
     * that outlives its `Process` object is left to the OS, and that divergence is documented
     * rather than papered over. A child that has ALREADY exited is reaped, by `reapIfNeeded`.
     */
    ~Impl() {
        readersStopping.store(true, std::memory_order_relaxed);
        if (stdoutReader.joinable()) stdoutReader.join();
        if (stderrReader.joinable()) stderrReader.join();
    }
};

Process::Process() : impl_(std::make_unique<Impl>()) {}
Process::~Process() = default;
Process::Process(Process&&) noexcept = default;
Process& Process::operator=(Process&&) noexcept = default;

const ProcessStartInfo& Process::getStartInfoProperty() const { return impl_->startInfo; }
void Process::setStartInfoProperty(const ProcessStartInfo& value) { impl_->startInfo = value; }

intcs Process::getIdProperty() const {
#if defined(SHARP_RUNTIME_PROCESS_POSIX)
    if (!impl_->started) throw System::InvalidOperationException("No process is associated with this object.");
    return static_cast<intcs>(impl_->pid);
#else
    throw System::PlatformNotSupportedException("System::Diagnostics::Process is only supported on POSIX platforms.");
#endif
}

#if defined(SHARP_RUNTIME_PROCESS_POSIX)
namespace {
    // Drains a pipe fd into *out on a background thread until EOF/error, matching this codebase's
    // established pattern (Timer, Socket's async methods) of using std::thread for blocking I/O
    // rather than requiring the caller to poll -- avoids the classic pipe deadlock where the child
    // blocks writing to a full pipe buffer while nobody is reading it.
    // Ticket #2029. This used to call a bare blocking read(), which cannot return until the
    // child closes its end of the pipe -- so joining the reader meant waiting for the child, and
    // destroying a redirected Process blocked for the child's whole lifetime.
    //
    // It now waits on poll() in bounded slices and re-checks the stop flag between them, which
    // is the same shape #2134 had to give Socket::AcceptAsync for the same reason: a boundary
    // that cannot be crossed turns one defect into a hang. Nothing about what is READ changes --
    // the loop still drains to EOF and still retries on EINTR -- only how long it is willing to
    // wait for the next byte when it has been asked to stop.
    void drainPipe(int fd, std::string* out, std::mutex* outMutex, const std::atomic<bool>* stopping) {
        char buf[4096];
        for (;;) {
            if (stopping != nullptr && stopping->load(std::memory_order_relaxed)) break;

            struct pollfd waiter{};
            waiter.fd = fd;
            waiter.events = POLLIN;
            const int ready = ::poll(&waiter, 1, 50);
            if (ready < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (ready == 0) continue;   // nothing yet; re-check the stop flag

            ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n > 0) {
                // #2030: every append is under the same lock the getters copy under, so a
                // caller can never observe a half-written buffer.
                std::lock_guard<std::mutex> lock(*outMutex);
                out->append(buf, static_cast<size_t>(n));
                continue;
            }
            if (n == 0) break; // EOF
            if (errno == EINTR) continue;
            break;
        }
        ::close(fd);
    }

    // The child cannot throw back into the parent after fork().  A small status pipe, closed on
    // successful exec(), lets Start() distinguish an actual launch from the historical
    // "returned Process which immediately exits 127" failure mode.
    [[noreturn]] void reportChildStartupFailure(int statusFd, int error) noexcept {
        while (::write(statusFd, &error, sizeof(error)) < 0 && errno == EINTR) {}
        ::_exit(127);
    }

    void closePipe(int fds[2]) noexcept {
        if (fds[0] >= 0) ::close(fds[0]);
        if (fds[1] >= 0) ::close(fds[1]);
        fds[0] = -1;
        fds[1] = -1;
    }

    char*** processEnvironmentSlot() noexcept {
#if defined(__APPLE__)
        return _NSGetEnviron();
#else
        return &::environ;
#endif
    }

}

void Process::reapIfNeeded(Impl& impl) {
    if (impl.hasExited || !impl.started || impl.isCurrentProcess) return;
    int status = 0;
    // Retry on EINTR (#2024).  Even a non-blocking waitpid can be interrupted by a signal
    // delivered between entry and return; treating that as "not exited yet" would make a
    // poll silently skip an exit it had already observed.  This mirrors the three EINTR
    // loops this file already had (drainPipe, reportChildStartupFailure, Start's status read).
    pid_t r;
    do {
        r = ::waitpid(impl.pid, &status, WNOHANG);
    } while (r < 0 && errno == EINTR);
    if (r == impl.pid) {
        impl.hasExited = true;
        impl.exitCode = WIFEXITED(status) ? WEXITSTATUS(status)
                       : WIFSIGNALED(status) ? (128 + WTERMSIG(status))
                       : -1;
        // #2032: the reader join is GONE from here, and that is the whole repair.
        //
        // A reader cannot return until the pipe reaches EOF, and EOF needs EVERY holder of the
        // write end to close it -- including a grandchild that inherited it. `dash -c 'sleep 30'`
        // FORKS rather than exec's, so Kill() terminated only dash and this join then waited for
        // the grandchild: measured, `WaitForExit(5000)` returned true after 29,951 ms, roughly
        // 6x its own declared bound and unbounded in general. #2033 measured that the same join
        // is reached from five public doors, six counting getExitCodeProperty().
        //
        // .NET waits for the streams in exactly ONE place and says why in a comment:
        //
        //     if (exited && milliseconds == Timeout.Infinite) // if we have a hard timeout, we
        //     {                                               // cannot wait for the streams
        //         _output?.EOF.GetAwaiter().GetResult();
        //         _error?.EOF.GetAwaiter().GetResult();
        //     }
        //                                    Process.Unix.cs, WaitForExitCore
        //
        // -- so reaping the CHILD and waiting for its OUTPUT are two different things, and only
        // the unbounded overload does both. `HasExited`, `ExitCode` and `Kill` wait for neither.
        // The readers are still joined by ~Impl, bounded by `readersStopping` (#2029), so
        // nothing is leaked or detached.
    }
}
#endif

bool Process::getHasExitedProperty() const {
#if defined(SHARP_RUNTIME_PROCESS_POSIX)
    if (!impl_->started) throw System::InvalidOperationException("No process is associated with this object.");
    if (impl_->isCurrentProcess) return false;
    // #2030: this member is `const` and yet mutates hasExited/exitCode through reapIfNeeded --
    // that is .NET's shape too, since HasExited reaps lazily, so the repair is not to stop
    // mutating but to make the mutation safe against a concurrent reader.
    std::lock_guard<std::mutex> lock(impl_->stateMutex);
    reapIfNeeded(*impl_);
    return impl_->hasExited;
#else
    throw System::PlatformNotSupportedException("System::Diagnostics::Process is only supported on POSIX platforms.");
#endif
}

intcs Process::getExitCodeProperty() const {
#if defined(SHARP_RUNTIME_PROCESS_POSIX)
    if (!getHasExitedProperty())
        throw System::InvalidOperationException("Process must exit before requested information can be determined.");
    return impl_->exitCode;
#else
    throw System::PlatformNotSupportedException("System::Diagnostics::Process is only supported on POSIX platforms.");
#endif
}

// Ticket #2030 / SR-AUD-271. BY VALUE, under the lock the readers append under. A reference
// into this buffer cannot be made safe: the reader thread is still appending to it, and the
// audit measured the SAME reference reading 4 bytes mid-run and 8 bytes after exit. The copy is
// the only thing a caller can hold.
std::string Process::getStandardOutputTextProperty() const {
    if (!impl_->startInfo.getRedirectStandardOutputProperty())
        throw System::InvalidOperationException(
            "StandardOut has not been redirected. Set RedirectStandardOutput on ProcessStartInfo before starting the process.");
    std::lock_guard<std::mutex> lock(impl_->outputMutex);
    return impl_->stdoutText;
}

std::string Process::getStandardErrorTextProperty() const {
    if (!impl_->startInfo.getRedirectStandardErrorProperty())
        throw System::InvalidOperationException(
            "StandardError has not been redirected. Set RedirectStandardError on ProcessStartInfo before starting the process.");
    std::lock_guard<std::mutex> lock(impl_->outputMutex);
    return impl_->stderrText;
}

bool Process::Start() {
#if defined(SHARP_RUNTIME_PROCESS_POSIX)
    const ProcessStartInfo& si = impl_->startInfo;
    if (si.getFileNameProperty().empty())
        throw System::InvalidOperationException("Cannot start process because a file name has not been provided.");
    for (const auto& [name, value] : si.getEnvironmentVariablesProperty()) {
        (void)value;
        if (name.empty() || name.find('=') != std::string::npos) {
            throw System::ArgumentException(
                "Environment variable names must be nonempty and cannot contain '='.", "name");
        }
    }

    // Build argv: argv[0] is conventionally the program name, followed by ArgumentList (the
    // unambiguous path -- no shell/OS quoting), then a naive whitespace split of Arguments for
    // convenience (see ProcessStartInfo's doc-comment for why this is intentionally simplified).
    std::vector<std::string> argvStorage;
    argvStorage.push_back(si.getFileNameProperty());
    for (const auto& a : si.getArgumentListProperty()) argvStorage.push_back(a);
    if (!si.getArgumentsProperty().empty()) {
        std::istringstream iss(si.getArgumentsProperty());
        std::string tok;
        while (iss >> tok) argvStorage.push_back(tok);
    }
    std::vector<char*> argv;
    argv.reserve(argvStorage.size() + 1);
    for (auto& s : argvStorage) argv.push_back(s.data());
    argv.push_back(nullptr);

    // Restart preamble (#2025).  The default constructor is private, so the ONLY way a caller
    // reaches this function twice is the restart this class's own doc-comment advertises --
    // every public instance Start() is by definition a restart.  Assigning into
    // impl_->stdoutReader/stderrReader below calls std::thread::operator=, which calls
    // std::terminate when the target is joinable, so the previous child's readers must be
    // resolved *before* that assignment or the whole process aborts.
    //
    // Measured (build-probe/2025_probe1_before.log): the trigger is a JOINABLE READER, not a
    // running child.  Restarting after the previous child had already exited aborted too
    // (case C), because nothing had joined the reader yet -- only WaitForExit() and
    // getHasExitedProperty() do that.  Both cases are handled here.
    //
    // A still-running previous child is REFUSED rather than absorbed.  The two alternatives
    // are both worse: joining its readers cannot finish until the child closes stdout, so it
    // would block for the child's whole lifetime (the SR-AUD-269 blocking failure mode), and
    // detaching them is exactly the gated destructor policy of #2029, which is not approved.
    // Refusing is uniform across both redirection combinations and loses nothing a caller
    // could previously rely on: unredirected, the old behaviour silently abandoned the first
    // child unreaped (case E); redirected, it aborted.
    if (impl_->started && !impl_->isCurrentProcess) {
        reapIfNeeded(*impl_);
        if (!impl_->hasExited)
            throw System::InvalidOperationException(
                "The associated process is still running. Wait for it to exit, or call Kill() "
                "and then wait, before starting another process with this Process instance.");
    }
    // The previous child has exited (or none was started), so its pipes are at EOF and these
    // joins complete promptly.  Joining here rather than at the assignment keeps the object
    // unchanged if anything below throws.
    if (impl_->stdoutReader.joinable()) impl_->stdoutReader.join();
    if (impl_->stderrReader.joinable()) impl_->stderrReader.join();

    // Build the child's environment in the PARENT (#2026).  Between fork() and exec() the
    // child may call only async-signal-safe functions, because it inherits the address space
    // of the whole parent but only the forking thread: ::setenv allocates, so if any other
    // thread held the malloc lock at the moment of the fork, the child would deadlock inside
    // it and never reach exec -- and the parent would then block forever in the status-pipe
    // read below.  That parent is multithreaded exactly when a previous REDIRECTED Start left
    // this class's own pipe-reader threads running, so the hazard needs no caller-created
    // thread to be reachable.
    //
    // The array built here reproduces exactly what the per-variable ::setenv loop produced:
    // the parent's own environment, with each configured variable replacing a same-named
    // entry and otherwise being appended.  Names were already validated above, so no entry
    // can be empty or contain '='.
    std::vector<std::string> envStorage;
    const auto& envOverrides = si.getEnvironmentVariablesProperty();
    char*** environmentSlot = processEnvironmentSlot();
    for (char** entry = *environmentSlot; entry != nullptr && *entry != nullptr; ++entry) {
        const char* separator = std::strchr(*entry, '=');
        const std::string name =
            separator != nullptr
                ? std::string(*entry, static_cast<std::size_t>(separator - *entry))
                : std::string(*entry);
        if (envOverrides.find(name) == envOverrides.end()) envStorage.emplace_back(*entry);
    }
    for (const auto& [name, value] : envOverrides) envStorage.push_back(name + "=" + value);
    std::vector<char*> envp;
    envp.reserve(envStorage.size() + 1);
    for (auto& entry : envStorage) envp.push_back(entry.data());
    envp.push_back(nullptr);

    int stdoutPipe[2] = {-1, -1};
    int stderrPipe[2] = {-1, -1};
    int startupStatusPipe[2] = {-1, -1};
    bool redirOut = si.getRedirectStandardOutputProperty();
    bool redirErr = si.getRedirectStandardErrorProperty();
    if (redirOut && ::pipe(stdoutPipe) != 0)
        throw System::InvalidOperationException(std::string("Failed to create stdout pipe: ") + std::strerror(errno));
    if (redirErr && ::pipe(stderrPipe) != 0) {
        // stdoutPipe was already successfully created above -- close it before throwing, or it
        // leaks both fds (ironic: this failure path fires exactly when fds are scarce, which is
        // one likely reason ::pipe() just failed).
        if (redirOut) { ::close(stdoutPipe[0]); ::close(stdoutPipe[1]); }
        throw System::InvalidOperationException(std::string("Failed to create stderr pipe: ") + std::strerror(errno));
    }
    if (::pipe(startupStatusPipe) != 0) {
        closePipe(stdoutPipe);
        closePipe(stderrPipe);
        throw System::InvalidOperationException(
            std::string("Failed to create process startup status pipe: ") + std::strerror(errno));
    }
    int startupStatusFlags = ::fcntl(startupStatusPipe[1], F_GETFD);
    if (startupStatusFlags < 0 ||
        ::fcntl(startupStatusPipe[1], F_SETFD, startupStatusFlags | FD_CLOEXEC) != 0) {
        const int error = errno;
        closePipe(stdoutPipe);
        closePipe(stderrPipe);
        closePipe(startupStatusPipe);
        throw System::InvalidOperationException(
            std::string("Failed to configure process startup status pipe: ") + std::strerror(error));
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        closePipe(stdoutPipe);
        closePipe(stderrPipe);
        closePipe(startupStatusPipe);
        throw System::InvalidOperationException(std::string("Failed to fork: ") + std::strerror(errno));
    }

    if (pid == 0) {
        ::close(startupStatusPipe[0]);
        // Child: put it in its own process group so Kill(entireProcessTree=true) can target the
        // whole tree via killpg without also signaling the parent's group.
        ::setpgid(0, 0);
        if (redirOut) {
            if (::dup2(stdoutPipe[1], STDOUT_FILENO) < 0)
                reportChildStartupFailure(startupStatusPipe[1], errno);
            ::close(stdoutPipe[0]);
            ::close(stdoutPipe[1]);
        }
        if (redirErr) {
            if (::dup2(stderrPipe[1], STDERR_FILENO) < 0)
                reportChildStartupFailure(startupStatusPipe[1], errno);
            ::close(stderrPipe[0]);
            ::close(stderrPipe[1]);
        }
        if (!si.getWorkingDirectoryProperty().empty()) {
            if (::chdir(si.getWorkingDirectoryProperty().c_str()) != 0)
                reportChildStartupFailure(startupStatusPipe[1], errno);
        }
        // Nothing from here to exec may allocate (#2026): the environment was marshalled in
        // the parent above, and std::vector::data() is a plain member read.
#if defined(__GLIBC__)
        ::execvpe(argv[0], argv.data(), envp.data());
#else
        // Where execvpe is absent (notably macOS), replacing the global environ pointer is a
        // single store -- async-signal-safe -- and execvp then passes it to the new image.
        *environmentSlot = envp.data();
        ::execvp(argv[0], argv.data());
#endif
        reportChildStartupFailure(startupStatusPipe[1], errno); // exec only returns on failure
    }

    // Parent
    ::close(startupStatusPipe[1]);
    startupStatusPipe[1] = -1;
    int childStartupError = 0;
    ssize_t statusRead = 0;
    do {
        statusRead = ::read(startupStatusPipe[0], &childStartupError, sizeof(childStartupError));
    } while (statusRead < 0 && errno == EINTR);
    const int statusReadError = errno;
    ::close(startupStatusPipe[0]);
    startupStatusPipe[0] = -1;
    if (statusRead != 0) {
        if (statusRead != static_cast<ssize_t>(sizeof(childStartupError))) {
            ::kill(pid, SIGKILL);
        }
        int ignoredStatus = 0;
        while (::waitpid(pid, &ignoredStatus, 0) < 0 && errno == EINTR) {}
        closePipe(stdoutPipe);
        closePipe(stderrPipe);
        if (statusRead == static_cast<ssize_t>(sizeof(childStartupError))) {
            throw System::InvalidOperationException(
                "Failed to start process '" + si.getFileNameProperty() + "': " +
                std::strerror(childStartupError));
        }
        throw System::InvalidOperationException(
            std::string("Failed to receive process startup status: ") + std::strerror(statusReadError));
    }
    // Committed to the new child from here on: nothing below throws, so the object never
    // observes a half-applied restart.  The captured text belongs to the process that
    // produced it, so a restart starts from empty rather than appending the new child's
    // output to the previous one's (measured before this change: "first" then "firstsecond").
    impl_->stdoutText.clear();
    impl_->stderrText.clear();
    if (redirOut) {
        ::close(stdoutPipe[1]);
        impl_->stdoutReader = std::thread(drainPipe, stdoutPipe[0], &impl_->stdoutText,
                                          &impl_->outputMutex, &impl_->readersStopping);
    }
    if (redirErr) {
        ::close(stderrPipe[1]);
        impl_->stderrReader = std::thread(drainPipe, stderrPipe[0], &impl_->stderrText,
                                          &impl_->outputMutex, &impl_->readersStopping);
    }
    impl_->pid = pid;
    impl_->started = true;
    impl_->hasExited = false;
    // A Process obtained from GetCurrentProcess() now describes the child it just started,
    // not the current process; leaving this set would make WaitForExit/Kill silently no-op
    // on a real child.
    impl_->isCurrentProcess = false;
    return true;
#else
    throw System::PlatformNotSupportedException("System::Diagnostics::Process is only supported on POSIX platforms.");
#endif
}


#if defined(SHARP_RUNTIME_PROCESS_POSIX)
namespace {

    /**
     * @brief The immediate children of @p pid, read from `/proc/<n>/stat`'s fourth field.
     *
     * Ticket #2031. Linux-specific by construction: `/proc` is where the parent link lives, and
     * .NET's own `GetChildProcesses` is equally platform-bound (it goes through
     * `Process.GetProcesses()`, whose Unix implementation reads `/proc`).
     *
     * `stat`'s second field is the executable name **in parentheses and unescaped**, so it may
     * contain spaces and even `)`. Splitting on whitespace is therefore wrong; the parse scans to
     * the LAST `)` and takes the fields after it, which is what every correct /proc reader does.
     */
    std::vector<pid_t> ChildrenOf(pid_t pid) {
        std::vector<pid_t> children;
        DIR* proc = ::opendir("/proc");
        if (proc == nullptr) return children;
        while (dirent* entry = ::readdir(proc)) {
            char* end = nullptr;
            const long candidate = std::strtol(entry->d_name, &end, 10);
            if (end == entry->d_name || *end != '\0' || candidate <= 0) continue;

            std::string path = std::string("/proc/") + entry->d_name + "/stat";
            std::ifstream stat(path);
            if (!stat) continue;
            std::string line;
            if (!std::getline(stat, line)) continue;
            const size_t close = line.rfind(')');
            if (close == std::string::npos) continue;
            // After ")" come: state, ppid, ...
            std::istringstream rest(line.substr(close + 1));
            std::string state;
            long ppid = 0;
            if (!(rest >> state >> ppid)) continue;
            if (static_cast<pid_t>(ppid) == pid) children.push_back(static_cast<pid_t>(candidate));
        }
        ::closedir(proc);
        return children;
    }

    /** @brief True when @p pid is @p ancestor or any descendant of it. */
    bool IsSelfOrDescendantOf(pid_t ancestor, pid_t pid) {
        if (ancestor == pid) return true;
        std::vector<pid_t> frontier{ancestor};
        for (size_t i = 0; i < frontier.size(); ++i) {
            for (pid_t child : ChildrenOf(frontier[i])) {
                if (child == pid) return true;
                frontier.push_back(child);
            }
        }
        return false;
    }

    /**
     * @brief .NET's `Process.KillTree` (`Process.Unix.cs:97-137`), transcribed.
     *
     * The ORDER is the substance, and it is what #2031's own proposed option A was missing.
     * .NET stops the process **before** enumerating its children -- the comment says why,
     * verbatim: *"Stop the process, so it won't start additional children. This is best effort:
     * kill can return before the process is stopped."* Option A enumerated and then killed, so a
     * process that forked between those two steps left a survivor: precisely the defect the
     * ticket exists to remove, reintroduced one level down.
     *
     * `ESRCH` is ignored throughout, because a process may legitimately exit between any two
     * steps of the walk; every other errno is collected and reported together.
     *
     * HONEST NOTE ON THE EVIDENCE: a mutation removing the SIGSTOP is NOT caught, and it cannot
     * be caught deterministically. What it removes is a RACE WINDOW -- the microseconds between
     * `ChildrenOf` and the `SIGKILL`, during which the target could fork a child that is neither
     * enumerated nor killed. A test could only observe it by forking in a tight loop and hoping
     * to land inside that window, which is a FLAKY test; this repository has repaired two of
     * those this session (#2352, #2105) on the ground that an intermittently green gate is not
     * evidence. The SIGSTOP is here because .NET has it and states its purpose in a comment, not
     * because a test distinguishes it.
     */
    void KillTree(pid_t pid, std::vector<std::string>& failures) {
        if (::kill(pid, SIGSTOP) != 0) {
            if (errno != ESRCH)
                failures.push_back("SIGSTOP to pid " + std::to_string(pid) + ": " + std::strerror(errno));
            return;
        }
        const std::vector<pid_t> children = ChildrenOf(pid);
        if (::kill(pid, SIGKILL) != 0 && errno != ESRCH)
            failures.push_back("SIGKILL to pid " + std::to_string(pid) + ": " + std::strerror(errno));
        for (pid_t child : children) KillTree(child, failures);
    }

}  // namespace
#endif

void Process::Kill() { Kill(false); }

void Process::Kill(bool entireProcessTree) {
    (void)entireProcessTree; // unused on non-POSIX platforms, where this throws below
#if defined(SHARP_RUNTIME_PROCESS_POSIX)
    if (!impl_->started) return;
    // #2031: the self-guard runs FIRST, before the current-process no-op and before the exit
    // check, because that is where .NET has it -- `Kill(bool)` refuses immediately, ahead of
    // `KillTree` (`Process.NonUap.cs:23-28`). Placing it after the `isCurrentProcess` early
    // return would make `GetCurrentProcess().Kill(true)` a silent no-op where .NET throws, and
    // would leave the guard unreachable through any ordinary Process object.
    //
    // `Kill(false)` is deliberately untouched: .NET delegates it straight to `Kill()`
    // (`Process.NonUap.cs:17-20`), and this port's own no-op-on-the-current-process behaviour
    // for that overload is a separate, pre-existing divergence that is not bundled here.
    if (entireProcessTree
        && (impl_->isCurrentProcess || IsSelfOrDescendantOf(impl_->pid, ::getpid())))
        throw System::InvalidOperationException(
            "Cannot be used to terminate a process tree containing the calling process.");
    if (impl_->isCurrentProcess) return;
    reapIfNeeded(*impl_);
    if (impl_->hasExited) return;
    if (!entireProcessTree) {
        ::kill(impl_->pid, SIGKILL);
        return;
    }
    // #2031 (SR-AUD-273, cause D-E). This was `::killpg(pid, SIGKILL)`, which reaches the
    // child's process GROUP -- and a descendant that called setsid() has left it. Measured: the
    // setsid grandchild was ALIVE afterwards and the probe had to kill it itself.
    //
    // .NET walks the tree instead (`Process.Unix.cs:97-137`), and the SELF-GUARD below is
    // .NET's too (`Process.NonUap.cs:25-26`): killing a tree that contains the calling process
    // would kill the caller, so it is refused rather than attempted.
    std::vector<std::string> failures;
    KillTree(impl_->pid, failures);
    if (!failures.empty()) {
        std::string detail;
        for (const auto& f : failures) { if (!detail.empty()) detail += "; "; detail += f; }
        throw System::InvalidOperationException(
            "Not all processes in process tree could be terminated. (" + detail + ")");
    }
#else
    throw System::PlatformNotSupportedException("System::Diagnostics::Process is only supported on POSIX platforms.");
#endif
}

void Process::WaitForExit() {
#if defined(SHARP_RUNTIME_PROCESS_POSIX)
    if (!impl_->started) throw System::InvalidOperationException("No process is associated with this object.");
    if (impl_->isCurrentProcess) return;
    if (!impl_->hasExited) {
        int status = 0;
        // Retry on EINTR (#2024, SR-AUD-272).  This overload's contract is "blocks the calling
        // thread until the associated process terminates"; before this loop, any signal whose
        // handler was installed without SA_RESTART made waitpid fail with EINTR and this function
        // return with the child still running -- measured at 1000 ms into a 3 s child.
        pid_t r;
        do {
            r = ::waitpid(impl_->pid, &status, 0);
        } while (r < 0 && errno == EINTR);
        if (r == impl_->pid) {
            impl_->hasExited = true;
            impl_->exitCode = WIFEXITED(status) ? WEXITSTATUS(status)
                             : WIFSIGNALED(status) ? (128 + WTERMSIG(status))
                             : -1;
        }
    }
    // #2032: THE one place .NET waits for the output streams, and this overload is
    // `WaitForExit(Timeout.Infinite)` in .NET too (`Process.cs`). Having no deadline, it is the
    // only one that CAN wait for a pipe whose write end an inherited grandchild may still hold.
    // These joins remain required even when a prior finite wait or HasExited poll already reaped
    // the direct child: reaping the process and completing its redirected streams are separate
    // operations, and the unbounded overload promises both.
    if (impl_->stdoutReader.joinable()) impl_->stdoutReader.join();
    if (impl_->stderrReader.joinable()) impl_->stderrReader.join();
#else
    throw System::PlatformNotSupportedException("System::Diagnostics::Process is only supported on POSIX platforms.");
#endif
}

bool Process::WaitForExit(intcs milliseconds) {
    // Validated before the platform guard and before any state check, so the argument domain
    // is the same on every platform -- the System::Threading::Thread::Join(intcs) precedent
    // in this repository, whose message this transcribes verbatim (#2024, SR-AUD-268).
    if (milliseconds < TimeoutInfinite)
        throw System::ArgumentOutOfRangeException("milliseconds",
            "Number must be either non-negative and less than or equal to Int32.MaxValue or -1.");
#if defined(SHARP_RUNTIME_PROCESS_POSIX)
    if (!impl_->started) throw System::InvalidOperationException("No process is associated with this object.");
    if (impl_->isCurrentProcess) return false;
    // -1 is Timeout.Infinite: wait indefinitely rather than computing a deadline in the past.
    // Before this, the deadline was already elapsed on entry, so the loop body never ran and
    // the call returned false immediately -- measured at 0 ms against a 2 s child.
    if (milliseconds == TimeoutInfinite) {
        WaitForExit();
        return impl_->hasExited;
    }
    reapIfNeeded(*impl_);
    if (impl_->hasExited) return true;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (std::chrono::steady_clock::now() < deadline) {
        reapIfNeeded(*impl_);
        if (impl_->hasExited) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    reapIfNeeded(*impl_);
    return impl_->hasExited;
#else
    throw System::PlatformNotSupportedException("System::Diagnostics::Process is only supported on POSIX platforms.");
#endif
}

Process Process::Start(const ProcessStartInfo& startInfo) {
    Process p;
    p.setStartInfoProperty(startInfo);
    p.Start();
    return p;
}

Process Process::Start(const std::string& fileName) {
    return Start(ProcessStartInfo(fileName));
}

Process Process::Start(const std::string& fileName, const std::string& arguments) {
    return Start(ProcessStartInfo(fileName, arguments));
}

Process Process::GetCurrentProcess() {
#if defined(SHARP_RUNTIME_PROCESS_POSIX)
    Process p;
    p.impl_->pid = ::getpid();
    p.impl_->started = true;
    p.impl_->isCurrentProcess = true;
    return p;
#else
    throw System::PlatformNotSupportedException("System::Diagnostics::Process is only supported on POSIX platforms.");
#endif
}

} // namespace System::Diagnostics
