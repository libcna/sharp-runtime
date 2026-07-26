# Audit: `modules/runtime/src/System/Runtime/InteropServices/PosixSignalRegistration.cpp`

## Metadata

- AUDITED: 240-line POSIX implementation and 20-line Windows/Emscripten
  fallback, fully read.
- Validation: `PosixSignalTests.*` plus `PosixSignalContextTests.*` passed 9/9
  on 2026-07-27.
- Reference basis: local current-.NET `PosixSignalRegistration.cs`,
  `PosixSignalRegistration.Unix.cs`, and native `pal_signal.c`.
- Reproduction: C++20 probe linked with Runtime/Core prints
  `during_registration_original=0 callbacks=1`,
  `after_dispose_original=0`, and `raw_usr1=unsupported`; a forked helper
  prints `child_stopped_by=20` after a non-cancelled `Sigtstp` callback.

## SR-AUD-169 — high — registering a POSIX handler discards, neither chains nor restores the process's prior signal disposition

`installIfNeeded` calls `sigaction(signo, &sa, nullptr)`, deliberately
discarding the old action, and `uninstallIfUnused` later calls
`std::signal(signo, SIG_DFL)`.  `dispatchSignal` likewise swaps to `SIG_DFL`
before re-raising a non-cancelled signal.  Consequently a process that already
has a custom handler or `SIG_IGN` loses it when the first registration is
created and receives the default disposition after the last registration is
disposed.

The standalone helper first installs a `SIGWINCH` handler, creates a cancelling
`Sigwinch` registration, delivers the signal, disposes the registration, and
delivers it again.  It prints zero original calls both while registered and
after disposal, proving both loss paths without a destructive signal.

Current .NET saves the original `sigaction` including flags/mask, invokes a
non-default/non-ignore action for non-termination signals, and restores the
saved action when the final registration is disabled.  Replacing arbitrary
application signal policy can suppress required cleanup/notification or turn
later delivery into termination, so this is high severity.

## SR-AUD-170 — medium — Create rejects valid positive raw Unix signal values that current .NET expressly supports

`toNativeSignalNumber` accepts only the ten named negative values.  Any other
cast, including a positive native signal number, becomes zero and `Create`
throws `PlatformNotSupportedException`.  The probe's
`Create(static_cast<PosixSignal>(SIGUSR1), ...)` prints
`raw_usr1=unsupported` before any delivery occurs.

Current .NET's Unix `GetPlatformSignalNumber` returns a positive raw value in
the OS range and its public Create remarks expressly allow callers to cast
raw values to `PosixSignal`.  This C++ port therefore removes a documented
extension point, including common catchable signals such as `SIGUSR1`.

## SR-AUD-171 — high — non-cancelled job-control signals perform an OS stop where current .NET's Unix path deliberately does not

For every non-cancelled signal, C++ sets `SIG_DFL` and calls `raise(signo)`.
That applies to `SIGTSTP`, `SIGTTIN`, and `SIGTTOU`, whose default action stops
the process.  Current .NET's `HandleNonCanceledPosixSignal` deliberately makes
the job-control cases no-ops after its managed notification, instead of
stopping the process.

The forked helper creates a non-cancelling `Sigtstp` registration in a child,
sends `SIGTSTP`, observes `WIFSTOPPED`, prints `child_stopped_by=20`, then
safely resumes and reaps the child.  A caller merely registering an observer
can therefore suspend its process under C++ where the current managed path
continues execution.

## SR-AUD-172 — high — the self-pipe is blocking despite the raw handler claiming full-pipe writes are harmless

`ensureWatcherStarted` uses `pipe(selfPipe_)` and never applies `O_NONBLOCK`.
POSIX pipe descriptors are blocking by default.  The raw signal handler then
calls `write(selfPipe_[1], ...)`; its comment says a full pipe returns
`EAGAIN` and is safe to ignore, but `EAGAIN` occurs only for a non-blocking
descriptor.  Once the finite pipe buffer is full, a delivered signal can block
inside the raw handler until the watcher reads, potentially deadlocking a
thread interrupted while holding a lock the watcher/callback needs.

The direct fixture sends only isolated signals and has no saturation or
liveness bound, so it cannot expose this reachability.  This is a
source-proven high availability defect even though a flood reproducer is not
run in the test process.

## Assessment

The snapshot-before-callback locking, reverse iteration, empty-handler
validation, SIGKILL rejection, and basic watcher handoff are sound for the
green nominal cases.  They do not compensate for replacement of process-wide
signal state, missing raw-number support, incompatible job-control delivery,
or a blocking raw-handler write.

## Other missing assertions and diagnostics

- Tests cover only one safe default-ignore signal and a cancelling SIGTERM;
  they omit original action preservation, `SIG_IGN`, raw values, all
  job-control signals, move ownership, concurrent register/dispose, and
  failure setup paths.
- No test measures delivered-versus-coalesced signal cardinality, pipe
  saturation, watcher progress, or file descriptor/thread teardown.
- Callback exceptions are not contained; the detached watcher can invoke
  `std::terminate`, but no fixture specifies the intended process policy.

## Final assessment

The POSIX path has four confirmed behavior/liveness defects (SR-AUD-169
through SR-AUD-172).  No source or test was modified.
