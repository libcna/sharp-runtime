# Audit: `modules/collections-async/include/System/Collections/Generic/IAsyncEnumerator.hpp`

## Metadata

- AUDITED: polymorphic advance/current/disposal interface.
- Validation: `SharpRuntimeTests_Collections_Async` passed 6/6 on 2026-07-27.

## Assessment

MoveNextAsync returning bool and DisposeAsync returning void are explicitly
documented synchronous adaptations rather than a claim of ValueTask-backed
managed behavior.  The virtual destructor supports the advertised polymorphic
use.

## Other missing assertions and diagnostics

- Test Current before first advance and after exhaustion, repeated disposal,
  disposal exceptions, move-only values, and destruction through the base
  pointer.
- Add an example/regression for resource-owning enumerators and document that
  callers cannot await disposal or propagation through IAsyncDisposable.

## Final assessment

No evidence-backed finding was confirmed.  No source or test was changed
during this audit.
