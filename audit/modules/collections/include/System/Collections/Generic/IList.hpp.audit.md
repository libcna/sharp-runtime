# Audit: `modules/collections/include/System/Collections/Generic/IList.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## Assessment

The public declaration, its immediate implementation path, and focused call sites were reviewed.  No separate evidence-backed finding is assigned to this file beyond the related findings recorded in their owning reports.

## Missing assertions and diagnostics

- Keep invalid-input, lifecycle, ownership, and native-boundary diagnostics covered by focused tests as this surface evolves.

## Final assessment

AUDITED. No separate evidence-backed finding is assigned to this file.

### Follow-up: ticket #1791 changed this interface (2026-07-29)

Under the four-part approval of `docs/ListIndexerVersioningDesign.md` §28,
`virtual T& operator[](intcs)` became
`virtual System::Collections::detail::ElementReference<T> operator[](intcs)`, and
two pure virtuals were added: `virtual const T& getItem(intcs) const` and
`virtual void setItem(intcs, const T&)`. The const indexer is unchanged.

A covariant return applies to pointers and references only, so a class return type
cannot override a reference return: **every implementer had to migrate, and an
unmigrated one fails to compile rather than silently keeping the old behaviour.**
All four migrated — `Generic::List<T>`, `ObjectModel::Collection<T>` (which gained
a mutation counter, `sizeof` 32 → 40), `ObjectModel::ReadOnlyCollection<T>` (both
mutation paths throw `NotSupportedException`; it gained no counter), and the
hand-written `IntList` in `ReadOnlyInterfacesTests.cpp`, which is the repository's
own evidence that consumers implement this interface by hand.

The interface's vtable grew from **14 to 16** non-null entries, measured. The
mangled name of `operator[]` did **not** change, because a return type is not part
of a C++ mangled name — so a stale object file links with no diagnostic and
silently loses tracking. Recorded in the design record §33 and in `README.md`.
