# Audit: `modules/collections/include/System/Collections/Generic/LinkedList.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-357 — high — copied LinkedListNode handles retain dangling list iterators after node or owner destruction

`LinkedListNode<T>` is a copyable pair of a raw `std::list<T>*` and native iterator.  Removing the node or destroying its owner leaves any copied handle truthy and dereferenceable.  A direct ASan/UBSan probe retains `getFirstProperty()`, destroys the `LinkedList<int>`, then reads the node; ASan reports heap-use-after-free in the public accessor.  The raw-list representation also cannot preserve .NET's independently allocated/detached node lifetime model.

## Missing assertions and diagnostics

- Tests do not retain nodes across Remove/Clear/owner destruction or assert a deterministic detached-node result.
- Ownership/liveness state must be observable before any node value, next, previous, comparison, or implicit conversion dereference.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
