# Audit: `modules/threading-tasks/include/System/Threading/Tasks/TaskCanceledException.hpp`

## Metadata

- AUDITED: public cancellation-exception constructors and `Task` property
  ownership contract.
- Validation: `TaskCanceledExceptionTests.*` passed 5/5 on 2026-07-27.  A
  direct C++20 AddressSanitizer probe constructed the exception from a local
  completed `Task`, ended that task's scope, then read the returned task's
  status.  A current .NET 10 counterpart retained `TaskCanceledException.Task`
  through forced collection.
- Reference basis: current .NET 10 `TaskCanceledException(Task)` and its
  GC-tracked `Task` property.

## SR-AUD-230 — high — the public Task property retains a dangling raw pointer after its source Task is destroyed

The `Task` constructor stores `const Task*` directly in `canceledTask_`, and
`getTaskProperty()` returns it without an ownership or validity check.  The
header acknowledges the lifetime hazard, but this does not make the ordinary
public exception/property interaction safe.  The ASan probe reports
`stack-use-after-scope` when `getTaskProperty()->getStatusProperty()` reads a
`Task` that was local to the construction scope.  The current-.NET counterpart
keeps `exception.Task` non-null and its `WeakReference` alive after forced GC.

The native pointer can escape in a caught exception after the task object has
been moved, returned, or destroyed; dereferencing it is undefined behavior.

## Assessment

The default/message/token constructors have the expected cancellation base
type and message.  The Task-taking constructor exposes a memory-safety defect,
not merely the documented lack of garbage collection.

## Other missing assertions and diagnostics

- Existing tests check pointer identity only while the input task is alive;
  add an ownership/lifetime regression that destroys or moves the input before
  inspecting the exception property under ASan.
- Exercise copied and asynchronously propagated exceptions, null-equivalent
  input, token preservation, HResult, and nested exception diagnostic paths.
- The API needs a diagnostic/contract that cannot return a dereferenceable raw
  pointer unless it owns or otherwise guarantees the task lifetime.

## Final assessment

SR-AUD-230 is ASan-confirmed by a direct native/current-.NET comparison.  No
production or test source was changed during this audit.
