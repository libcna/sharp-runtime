# Audit: `modules/threading-tasks/tests/System/Threading/Tasks/TasksTests.cpp`

## Metadata

- AUDITED: 1,523-line module fixture covering Task/TaskT, continuations,
  completion sources, ValueTask, Parallel, exception adapters, scheduler, and
  factory behavior.
- Validation: the executable passed all 171 tests from 26 suites on
  2026-07-27.

## Assessment

The suite covers ordinary completion, fault, cancellation, continuation,
multi-task, bridge, and bounded-parallel paths well, including several recent
regressions.  Green results do not cover the public native null-equivalent
delegate and lifetime boundaries found during this audit.

## Other missing assertions and diagnostics

- Add a sanitizer-backed TaskCanceledException lifetime regression for
  SR-AUD-230; the current pointer-identity test never lets the input Task die.
- Add synchronous `ArgumentNullException`-equivalent assertions for empty
  Task/TaskT/TaskFactory delegates and continuations (SR-AUD-231), asserting
  no deferred `bad_function_call` task is returned.
- Add zero and less-than--1 MaxDegreeOfParallelism tests that require no work
  to run and an argument error (SR-AUD-232).
- Cover empty Parallel bodies/actions, Invoke stress/resource behavior,
  cancellation state after WhenAll, Delay(-1)'s documented adaptation,
  scheduler identity/ownership, ValueTask wrapping of moved tasks, and
  TaskCompletionSource destruction/concurrent lifetime under sanitizers.

## Final assessment

The tests are valuable regression coverage but omit all three confirmed
Threading.Tasks findings in this audit pass.  No source or test was changed.
