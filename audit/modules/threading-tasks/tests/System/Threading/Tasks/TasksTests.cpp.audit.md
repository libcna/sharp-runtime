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


---

## Note -- tickets #1965 and #1966, 2026-08-03

`SharpRuntimeTests_Threading_Tasks` went **171 -> 218**, in a new translation unit
`modules/threading-tasks/tests/System/Threading/Tasks/TasksBoundaryTests.cpp`.

- **#1965 (SR-AUD-231)** added 37 cases covering all 22 public entries that accept a callable:
  the empty callable rejected at entry with .NET's own parameter name, the first valid callable
  unaffected, the no-partial-state guarantee (no task started, no continuation registered), the
  exception catchable as `System::Exception` -- which `std::bad_function_call` was not -- and a
  sweep asserting that **no** public entry can still reach `std::bad_function_call` or defer the
  argument error into an `AggregateException`. Two of the cases exist specifically to pin
  corrections: `Parallel::Invoke` must report a null **element** with a plain
  `ArgumentException`, not `ArgumentNullException`, and an empty range or source must **still**
  throw rather than silently completing.
- **#1966 (SR-AUD-232)** added 10 cases covering degrees -2, -3, 0, `INTCS_MIN`, -1, 1 and a
  value above the core count, each asserting that an invalid option runs **no** body at all, that
  a degree of 1 admits no overlapping iteration (measured peak concurrency, not timing), that the
  degree error precedes the body error, and that the four overloads without options are
  untouched.

This addresses the report's "add exact immediate-error tests for empty action/function/
continuation values across `Task`, `TaskT` and `TaskFactory`; assert no task or worker is
created before the error" and "add zero, less-than--1, -1, one, and oversized degree tests".
**Still open:** `WhenAll` cancellation state, `WhenAny` races with all inputs already terminal,
continuation registration/completion races, `Delay(-1)`, and `Parallel::Invoke`'s unbatched
launch.
