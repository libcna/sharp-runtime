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

## Post-audit corrections — the `System::Runtime` namespace review, ticket #1972 (2026-08-03)

All four findings remain **confirmed** and every word above is retained. Four
corrections, each measured on 2026-08-03
(`build-probe/1972_probe2_posix_signal.cpp`, `build-probe/1972_probe2_before.log`).

### 1. A methodology trap that produced two false negatives before it was caught

The registry's watcher is a **process-global thread started by the first `Create()`**,
and `fork()` duplicates only the calling thread. A child forked *after* the parent has
registered anything inherits `watcherRunning_ == true` with **no watcher thread behind
it**: `ensureWatcherStarted()` returns early, nothing drains the pipe,
`dispatchSignal()` never runs, and both forked reproductions report a confident
**false negative**:

```
(forked after the parent registered)          (forked before any parent registration)
  child_still_running... callbacks=0            child_stopped_by=20 (WIFSTOPPED) callbacks=1
  watcher_never_parked                          passed_pipe_capacity ... died_signal_14 (SIGALRM)
```

This is `docs/ThreadingNamespaceReviewPlan.md` §19.4's rule in a new form — *a probe's
negative result is evidence about the probe until the probe has been shown capable of
reporting something*. Every forked case in `1972_probe2` now prints a liveness marker
(`callbacks=`, `parked`) and the forked cases run first.

### 2. SR-AUD-169's consequence is sharper than "receives the default disposition"

For most catchable signals the default disposition **terminates the process**, so the
phrasing understates the case the audit's own SIGWINCH probe cannot reach (SIGWINCH's
default *is* ignore):

```
sighup_before_create=SIG_IGN
sighup_during_registration=port-handler
sighup_after_dispose=SIG_DFL      <-- SIGHUP's default is *terminate*
```

A process that deliberately set `SIG_IGN` on `SIGHUP` — the standard daemon idiom — is
**killed by the next SIGHUP** once an unrelated component disposes its last
registration. The repair must restore `SIG_IGN` exactly, not merely "restore
something". Owned by cause **R-A**, ticket **#1975**.

### 3. SR-AUD-170 rejects the positive spelling of **named** members too

```
raw SIGUSR1(10)=rejected   raw SIGUSR2(12)=rejected   raw SIGPIPE(13)=rejected
raw SIGALRM(14)=rejected   raw SIGWINCH(28)=rejected   <-- a signal the port supports
```

`static_cast<PosixSignal>(SIGWINCH)` is rejected although `PosixSignal::Sigwinch` is
accepted. The defect is therefore not "raw numbers are unsupported" but "the enum is
accepted in exactly one of its two valid spellings", and a repair must make the two
spellings agree. Owned by cause **R-D**, ticket **#1977**.

### 4. SR-AUD-172's flood reproducer now exists, and the blocked writes carry no information

The finding correctly notes that no flood reproducer was run. One now is, and it is
deterministic: with the watcher parked inside a user callback, delivery ~65,537 blocks
**inside the raw handler** and `alarm(10)` is what ends the child
(`saturation_child=died_signal_14`). Two things follow that the finding does not
state: the blocked writes are **pure loss** — `pending_[signo]` is a flag already set
*before* the `write()`, so the byte that blocks carries nothing the watcher does not
already have — and the thread that blocks is whichever thread the OS chose for
delivery, which is exactly the "interrupted while holding a lock the watcher needs"
deadlock the finding describes. Owned by cause **R-B**, ticket **#1974**.

### Dispositions

SR-AUD-169 → **#1975** (compatible: save and restore only). SR-AUD-172 → **#1974**
(compatible). SR-AUD-170 → **#1977** (compatible: widening only). SR-AUD-171, plus
SR-AUD-169's *chaining* half, → **#1979**, **approval-gated**: the port's behaviour is
reproduced here but .NET's is not, because this report's reference basis is a reading
of `pal_signal.c` taken when `/rv/tmp/runtime/src/libraries/` was present — it is
**absent** in this environment — and carries no managed probe. That is the same line
the repository drew between #1968 and #1963.

One separate post-audit defect was found and is **not** folded into #1974: the
self-pipe descriptors have no `O_CLOEXEC` and survive `exec()`. Tracked as inactive
ticket **#1985**. **No new `SR-AUD-*` identifier was issued**; numbering stays frozen
at **364**.
