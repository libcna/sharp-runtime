# Audit: `modules/collections-object-model/include/System/Collections/ObjectModel/ReadOnlyObservableCollection.hpp`

## Metadata

- AUDITED: shared source view, notification forwarding, ownership, Empty, and
  read-only queries.
- Validation: direct C++20 AddressSanitizer probe constructed a wrapper in an
  inner scope, destroyed it, then mutated its still-live shared source.

## SR-AUD-237 — high — destroying a read-only wrapper leaves a source callback that dereferences the dead wrapper

The constructor appends a source `CollectionChanged` lambda capturing raw
`this`, but the class has no destructor that unregisters it.  Its deleted
copy/move operations prevent one documented dangling path but not ordinary
destruction.  The ASan probe reports `stack-use-after-scope` at the forwarding
lambda when `source->Add(42)` runs after the wrapper leaves scope.  A managed
ReadOnlyObservableCollection reference and its event subscriptions are
GC-tracked, so a live source cannot invoke a destroyed wrapper object.

## Assessment

The source-sharing design is the intended live-wrapper adaptation.  It needs a
subscription lifetime mechanism; otherwise normal RAII scope exit creates a
reachable memory-safety failure.

## Other missing assertions and diagnostics

- Add ASan destruction-before-source-mutation coverage, multiple wrapper
  subscriptions, handler mutation/exception/reentrancy, Empty singleton, null
  source, and concurrent source/wrapper lifetime tests.

## Final assessment

SR-AUD-237 is ASan-confirmed.  No source or test was changed during this audit.
