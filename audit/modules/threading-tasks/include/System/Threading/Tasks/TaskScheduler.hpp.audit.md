# Audit: `modules/threading-tasks/include/System/Threading/Tasks/TaskScheduler.hpp`

## Metadata

- AUDITED: scheduler identity, default/current accessors, concurrency property,
  and the declared practical-subset contract.
- Validation: `TaskSchedulerTests.*` passed 3/3 on 2026-07-27.  A direct
  C++20/current-.NET 10 probe confirmed `MaximumConcurrencyLevel` is
  `INT_MAX` and verified scheduler-exception HResult separately.

## Assessment

The header candidly states that Tasks do not dispatch through custom schedulers
and that Current always means Default.  The direct probe observes native
Default.Id as 0 versus the current .NET implementation's 1, but the public
contract requires uniqueness rather than a specified initial numeric value, so
this is not an evidence-backed defect.

## Other missing assertions and diagnostics

- Assert scheduler IDs are stable/unique across constructed schedulers and
  assert exact maximum-concurrency semantics.
- Add explicit tests documenting that TaskFactory scheduler input and ambient
  Current are deliberately non-routing subset behavior.

## Final assessment

No new finding was confirmed.  No source or test was changed during this audit.
