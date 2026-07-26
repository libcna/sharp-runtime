# Audit: `modules/threading-tasks/include/System/Threading/Tasks/TaskCompletionSource.hpp`

## Metadata

- AUDITED: generic/void promise bridges, atomic terminal transitions,
  destructor completion, cancellation tagging, and stable Task properties.
- Validation: `TaskCompletionSourceTests.*` and `TaskCompletionSourceVoidTests.*`
  passed 30/30 within the complete 171/171 module run on 2026-07-27.
- Reference basis: current .NET 10 TaskCompletionSource single-completion and
  Task-identity behavior; explicit RAII lifetime adaptation is documented.

## Assessment

The atomic claim-before-set protocol, copy-failure bridge completion, and
separate cancellation marker address the previously documented races and
type-confusion.  Destroying an unresolved native source cancels its bridge task
to break the `std::async`/promise teardown cycle; this differs from managed
GC lifetime but is prominently documented as an RAII requirement rather than
hidden behavior.

## Other missing assertions and diagnostics

- Add destructor-abandonment, race-with-destruction (under TSan), and exact
  task-status tests; the lifetime warning should be accompanied by a safe
  shared_ptr producer example regression.
- Test empty/null-equivalent exception pointers, task identity under concurrent
  retrieval, duplicate SetException/SetCanceled, and continuation reentrancy
  when a source completes.

## Final assessment

No additional evidence-backed finding was confirmed.  No source or test was
changed during this audit.
