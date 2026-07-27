# Audit: `modules/collections/include/System/Collections/Generic/IEnumerator.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-356 — high — collection enumerators dereference invalid Current states instead of throwing

At audit time, the generic enumerator facade forwarded `Current()` without a state precondition.  Multiple implementations indexed a vector/deque/map/list with their initial or exhausted cursor: `List`, `Queue`, `Stack`, `SortedList`, `LinkedList`, ObjectModel `Collection`/ `ReadOnlyCollection`, and the ConcurrentBag/Queue/Stack snapshot enumerators.  The direct ASan/UBSan probe called `List<int>::GetEnumerator()->Current()` before `MoveNext()` and got a heap-buffer-overflow four bytes before the backing allocation rather than `InvalidOperationException`.  The same unguarded shapes were reachable after enumeration completed.

## Missing assertions and diagnostics

- Tests cover successful iteration but do not call `Current()` before the first or after the final `MoveNext()` across the affected collection families.
- Each enumerator needs a common lifecycle diagnostic identifying before-start, ended, or invalidated state before exposing native storage.

## Remediation

**REMEDIATED by ticket #1767 on 2026-07-27.** A shared `EnumeratorState`
rejects before-start and after-end `Current` access before native storage is
touched in all ten affected implementations. Permanent regressions exercise
typed and non-generic Current bridges, normal iteration, repeated exhaustion,
and Reset. Managed generic enumerators can return a cached default outside a
valid position, but this C++ API returns `const T&`; throwing follows the
non-generic invalid-state contract and avoids inventing an unsafe reference.
The regressions pass 13/13; Collections.Core passes 1,435/1,435, the direct
ASan/UBSan replacement probe reports zero failures, and the
network-permitted repository gate passes 12,694/12,694.

## Final assessment

AUDITED. SR-AUD-356 was confirmed with reproducible evidence and is now
REMEDIATED; the original evidence is retained above.
