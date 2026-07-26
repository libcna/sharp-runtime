# Audit: `modules/threading-tasks/include/System/Threading/Tasks/ValueTask.hpp`

## Metadata

- AUDITED: non-generic/generic synchronous, faulted, and live Task-backed
  ValueTask representations.
- Validation: `ValueTaskTests.*` and `ValueTaskTTests.*` passed 14/14 on
  2026-07-27.

## Assessment

Task-backed values retain the task and query it live, avoiding the prior
completion snapshot/lost-exception behavior documented in the source.  Default
generic values correctly represent a completed default result.  No additional
reproducible defect was found in the representable native subset.

## Other missing assertions and diagnostics

- Exercise cancellation, repeated consumption, task destruction/move handling,
  move-only result types, and concurrent completion/inspection.
- Add null-equivalent wrapped-task diagnostics; current tests use only valid
  Tasks and therefore cannot distinguish a clean managed-style error from a
  moved-from native dereference.

## Final assessment

No new finding was confirmed.  No source or test was changed during this audit.
