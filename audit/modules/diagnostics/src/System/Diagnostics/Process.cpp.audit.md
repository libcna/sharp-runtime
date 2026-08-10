# Audit: `modules/diagnostics/src/System/Diagnostics/Process.cpp`

## Metadata

- AUDITED: POSIX fork/exec, pipe-draining threads, wait/reap, kill, and destruction implementation.
- Evidence: source review, `SharpRuntimeTests_Diagnostics` (159/159), official .NET Process WaitForExit contract, and native direct probes in `/tmp/sharp-runtime-diagnostics-audit/`.

## Assessment

### SR-AUD-268 — medium — `Process::WaitForExit(int)` breaks the negative-timeout contract

`process_timeout_contract` prints `minus-one=0 elapsed-ms=0`: .NET reserves
`-1` for an infinite wait and rejects values below it, while this code builds an
already-expired deadline and returns immediately.

### SR-AUD-269 — high — Process destruction leaks a zombie or blocks on redirected output

`process_destructor_zombie` prints identical started pid and `waitpid` result
after Process destruction, proving that no-redirection destruction leaves a
zombie. With redirection, the destructor joins a reader that cannot finish
until the child closes its pipe, unexpectedly blocking ownership release.

### SR-AUD-270 — high — restarting a redirected Process calls `std::terminate`

`process_restart` aborts with `terminate called without an active exception`:
the second `Start` assigns a new `std::thread` over a joinable existing reader.

### SR-AUD-271 — high — Process state and captured output have internal data races

Reader threads append to `std::string` while public output access returns an
unguarded reference; PID, exit, and reader-thread lifecycle fields are also
read/written without synchronization.

### SR-AUD-272 — medium — `WaitForExit` silently returns when `waitpid` is interrupted

`process_wait_eintr` prints `exit=throws` after SIGUSR1 interrupts `waitpid`:
the blocking wait returns without setting `hasExited` or `exitCode`.

### SR-AUD-273 — medium — `Kill(true)` is only a process-group kill, not a full process-tree kill

`process_kill_tree` shows `kill0=0` for a `setsid` descendant after `Kill(true)`.
The implementation documents a full tree but can only signal one process group.

### SR-AUD-274 — high — fork child calls non-async-signal-safe code in multithreaded parents

The child calls `setenv` and `execvp` after `fork`; neither is async-signal-safe
under POSIX's multithreaded-fork rule, creating a deadlock risk before exec.

## Other missing assertions and diagnostics

- Add subprocess tests for every direct probe, including redirected destructor
  blocking and start-twice behavior.
- Retry EINTR, report failed native waits/signals, serialize public state, and
  use a fork-safe launch primitive such as `posix_spawn` or an async-signal-safe
  `execve` child path.

## Final assessment

SR-AUD-268 through SR-AUD-274 apply. No source or test changed.
