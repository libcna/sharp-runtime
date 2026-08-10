# Audit: `modules/runtime/include/System/Runtime/InteropServices/PosixSignalRegistration.hpp`

## Metadata

- AUDITED: 64-line public move-only registration declaration, fully read.
- Validation: `PosixSignalTests.*` and `PosixSignalContextTests.*` passed 9/9
  on 2026-07-27.
- Reference basis: local current-.NET `PosixSignalRegistration.cs` and Unix
  partial implementation.

## Assessment

The move-only ownership model, null-handler validation, reverse-callback
order, and explicit `Dispose` surface are reasonable C++ representations of a
single-owner managed registration.  The header's description of a self-pipe
watcher accurately describes the intended architecture, but its claims about
default handling and safe signal delivery depend on the implementation.
SR-AUD-169, SR-AUD-171, and SR-AUD-172 show that the implementation loses a
pre-existing disposition, applies an incompatible terminal-signal default,
and can block the raw handler on its blocking pipe.

## Other missing assertions and diagnostics

- The direct fixture omits move construction, move assignment over an existing
  registration, moved-from/idempotent Dispose, and a handler disposing itself
  or another registration during callback dispatch.
- It never installs a pre-existing `sigaction` before Create, so it cannot
  catch the missing chaining/restoration in SR-AUD-169.
- It has no raw positive signal, non-cancelled `Sigtstp`, signal-flood/pipe
  saturation, handler-throwing, or concurrent Create/Dispose/delivery probe.
- No test makes the detached watcher/thread/file-descriptor lifetime visible
  at process teardown or after a failed `pipe`, `thread`, or `sigaction`
  setup.

## Final assessment

The declaration describes useful ownership and callback intent, but it cannot
guarantee the Unix signal semantics advertised by its implementation.  No
source or test was modified.
