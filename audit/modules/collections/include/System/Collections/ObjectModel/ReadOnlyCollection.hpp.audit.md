# Audit: `modules/collections/include/System/Collections/ObjectModel/ReadOnlyCollection.hpp`

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

Ticket #1767 now applies the shared lifecycle guard before ReadOnlyCollection
storage access. The permanent family regression and direct ASan/UBSan
replacement probe pass; SR-AUD-356 is marked `remediated` with its original
evidence retained.

## Final assessment

AUDITED. No separate evidence-backed finding is assigned to this file.

### Follow-up: ticket #1791 changed the non-const indexer's return type (2026-07-29)

Under the four-part approval of `docs/ListIndexerVersioningDesign.md` §28, the
non-const `operator[]` now returns
`System::Collections::detail::ElementReference<T>` so that this class still
overrides `Generic::IList<T>::operator[]`. **No proxy is ever constructed** — the
call throws `System::NotSupportedException("Collection is read-only.")` before it
could be — and `getItem(intcs) const` / a throwing `setItem(intcs, const T&)` were
added.

**This type gained no mutation counter**, deliberately: its design requires an
explicit unsupported mutation path, not tracking. `sizeof(ReadOnlyCollection<int>)`
and `sizeof(ReadOnlyCollection<std::string>)` are **unchanged at 24** on LP64,
measured.

The read-only guarantee is now also a *compile-time* one for aliasing:
`int& r = readOnly[0];` no longer type-checks, so a caller cannot hold a mutable
alias into a read-only wrapper's storage without ever calling the throwing
accessor. Pinned by the `readonly-mutable-alias` site of
`test/consumer/collections_list_indexer_negative.cpp` and by
`ListIndexerImplementers.ReadOnlyCollectionReadsButRefusesEveryWrite`.
