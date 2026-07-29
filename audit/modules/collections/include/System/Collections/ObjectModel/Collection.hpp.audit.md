# Audit: `modules/collections/include/System/Collections/ObjectModel/Collection.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## Assessment

At audit time, the local `IEnumerator<T>::Current()` implementation was traced as part of SR-AUD-356.  It indexed its backing storage without validating the before-start or exhausted cursor, matching the sanitizer-confirmed List pattern. The public declaration, its immediate implementation path, and focused call sites were reviewed.  No separate evidence-backed finding is assigned to this file beyond the related findings recorded in their owning reports.

## Missing assertions and diagnostics

- Keep invalid-input, lifecycle, ownership, and native-boundary diagnostics covered by focused tests as this surface evolves.

## Remediation

Ticket #1767 now applies the shared lifecycle guard before Collection storage
access. The permanent family regression and direct ASan/UBSan replacement
probe pass; SR-AUD-356 is marked `remediated` with its original evidence
retained.

## Final assessment

AUDITED. No separate evidence-backed finding is assigned to this file.

### Follow-up: ticket #1791 added a mutation counter and a fail-fast enumerator (2026-07-29)

Under the four-part approval of `docs/ListIndexerVersioningDesign.md` §28 — whose
third part is this type's object-layout change — `Collection<T>` gained a
`detail::MutationCounter`. **Measured: `sizeof(Collection<int>)` and
`sizeof(Collection<std::string>)` 32 → 40 on LP64, `alignof` 8 → 8.** Every
consumer must be rebuilt.

Two consequences beyond the layout:

1. **`GetEnumerator()` is now fail-fast.** Before #1791 it version-checked nothing
   at all — its enumerator held a bare `const std::vector<T>&` — so mutating
   during enumeration silently read reallocated storage, and **not even `Add()`
   invalidated an outstanding enumerator**. That was reproduced before the change.
   It now throws `System::InvalidOperationException` as `Generic::List<T>` does,
   which is also what .NET does: `Collection<T>.GetEnumerator()` delegates to the
   wrapped `IList<T>`'s enumerator, normally `List<T>`'s fail-fast one.
2. **The `SetItem` hook gap is narrowed, not closed.** This file's existing note
   records that `collection[index] = value` assigns directly into storage and
   never calls the virtual `SetItem` hook, unlike .NET, and that it was documented
   rather than solved with an assignment-intercepting proxy. #1791 introduced such
   a proxy — but it holds a slot and a counter, not a collection, so it still
   cannot make a virtual call. The assignment is now **tracked**, where before it
   was both untracked and hook-skipping, and the new public
   `setItem(index, value)` **does** dispatch through `SetItem`. Callers of a
   `SetItem`-overriding derived class must use `setItem`. Pinned by
   `ListIndexerImplementers.CollectionSetItemRunsTheOverridableHook`.

`InsertItem`, `RemoveItem`, `ClearItems` and `SetItem` all advance the counter.
`begin()`/`end()` still yield an untracked mutable `T&`, as on `List<T>`.
