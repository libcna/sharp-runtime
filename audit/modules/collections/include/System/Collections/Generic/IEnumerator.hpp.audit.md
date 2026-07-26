# Audit: `modules/collections/include/System/Collections/Generic/IEnumerator.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-356 — high — collection enumerators dereference invalid Current states instead of throwing

The generic enumerator facade forwards `Current()` without a state precondition.  Multiple implementations index a vector/deque/map/list with their initial or exhausted cursor: `List`, `Queue`, `Stack`, `SortedList`, `LinkedList`, ObjectModel `Collection`/ `ReadOnlyCollection`, and the ConcurrentBag/Queue/Stack snapshot enumerators.  The direct ASan/UBSan probe calls `List<int>::GetEnumerator()->Current()` before `MoveNext()` and gets a heap-buffer-overflow four bytes before the backing allocation rather than `InvalidOperationException`.  The same unguarded shapes are reachable after enumeration completes.

## Missing assertions and diagnostics

- Tests cover successful iteration but do not call `Current()` before the first or after the final `MoveNext()` across the affected collection families.
- Each enumerator needs a common lifecycle diagnostic identifying before-start, ended, or invalidated state before exposing native storage.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
