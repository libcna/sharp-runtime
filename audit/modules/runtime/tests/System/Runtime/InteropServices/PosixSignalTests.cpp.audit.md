# Audit: `modules/runtime/tests/System/Runtime/InteropServices/PosixSignalTests.cpp`

## Metadata

- AUDITED: 129-line dedicated POSIX fixture, fully read.
- Validation: `PosixSignalTests.*` plus `PosixSignalContextTests.*` passed 9/9
  on 2026-07-27 in approximately 466 ms.
- Reference basis: local current-.NET POSIX registration sources and the
  first-party implementation reviewed in the mirrored source report.

## Assessment

The fixture usefully verifies null-handler and SIGKILL rejection, safe
`SIGWINCH` delivery, reverse registration order, Dispose/destructor removal,
SIGTERM cancellation, and basic context construction.  Its note about moving
the atomic completion marker after mutex cleanup is a precise ThreadSanitizer
repair and avoids a false race in the order test.

Those green cases exercise only the port's internally installed handler.  They
do not establish compatibility with existing process signal actions, raw Unix
signal numbers, non-cancelled job-control behavior, or a saturated self-pipe.
Accordingly they leave SR-AUD-169 through SR-AUD-172 undetected.

## Missing assertions and diagnostics

- Install a custom `sigaction` or `SIG_IGN` before Create; assert its expected
  chaining while registered and exact restoration after the final Dispose.
- In a reaped helper process, create a non-cancelling `Sigtstp` registration
  and assert the intended post-callback state.  The audit helper observes the
  current child stop, whereas local current .NET's native branch deliberately
  does not stop for that case.
- Assert that a supported positive raw Unix value such as `SIGUSR1` is
  accepted, and distinguish unsupported OS values from SIGKILL/SIGSTOP setup
  failures.
- Cover move construction/assignment, idempotent/moved-from Dispose, callback
  removal during dispatch, signal flood/pipe liveness, and concurrent
  registration against delivery.
- The sleep-only Dispose checks have no proof that a late callback cannot run;
  add a barrier/counter diagnostic instead of relying on 100 ms timing.

## Final assessment

The 9/9 result is meaningful nominal evidence but insufficient for the
process-global and liveness contracts of this component.  No source or test
was modified.
