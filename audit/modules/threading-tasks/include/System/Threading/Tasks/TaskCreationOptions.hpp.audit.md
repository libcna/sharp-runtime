# Audit: `modules/threading-tasks/include/System/Threading/Tasks/TaskCreationOptions.hpp`

## Metadata

- AUDITED: managed-value-compatible creation-option enum and bitwise helpers.
- Validation: `TaskCreationOptionsTests.*` passed 2/2 on 2026-07-27.

## Assessment

The declared values match the covered managed API names.  The public contract
explicitly says eager `std::async` execution leaves scheduler-related flags
without effect, so that subset limitation is not a separate undisclosed bug.

## Other missing assertions and diagnostics

- Assert every bit value and the no-op behavior of long-running, fairness,
  attachment, scheduler, and asynchronous-continuation flags through
  TaskFactory-created tasks.

## Final assessment

No new finding was confirmed.  No source or test was changed during this audit.
