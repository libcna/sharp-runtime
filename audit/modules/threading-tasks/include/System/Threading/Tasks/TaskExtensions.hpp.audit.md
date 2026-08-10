# Audit: `modules/threading-tasks/include/System/Threading/Tasks/TaskExtensions.hpp`

## Metadata

- AUDITED: generic/non-generic nested-task validation, direct fast paths, and
  detached proxy completion paths.
- Validation: `TaskExtensionsTests.*` passed 9/9 on 2026-07-27.

## Assessment

The implementation checks moved-from outer and inner task representations,
preserves a terminal inner fast path, and holds proxy completion sources by
shared ownership in detached workers.  The explicit pthread limitation is
diagnosed.  No independently reproducible defect was found in the implemented
subset.

## Other missing assertions and diagnostics

- Exercise simultaneous outer/inner completion, destruction of all caller task
  handles before the detached proxy completes, many concurrent Unwrap calls,
  and callback/exception ordering under ASan/TSan.
- Test null-equivalent nested values in pending and fast paths, exception
  identity/aggregate behavior, and Emscripten diagnostics.

## Final assessment

No new finding was confirmed.  No source or test was changed during this audit.
