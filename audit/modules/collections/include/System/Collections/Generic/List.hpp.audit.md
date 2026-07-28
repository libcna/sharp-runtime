# Audit: `modules/collections/include/System/Collections/Generic/List.hpp`

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

Ticket #1767 now applies the shared lifecycle guard before List storage access.
The permanent family regression and direct ASan/UBSan replacement probe pass;
SR-AUD-356 is marked `remediated` with its original evidence retained.

## Final assessment

AUDITED. No separate evidence-backed finding is assigned to this file.

## Post-remediation follow-up: ticket #1787 (2026-07-28)

Ticket #1787 (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, size M) repaired
the mutation counter this file's fail-fast enumerator snapshots. It carries **no
`SR-AUD-*` identifier** — the numbering is frozen at 364 and the pattern was
found during remediation, by ticket #1786's own inventory — and it reopens no
finding here. The original evidence above is retained unchanged.

The counter is now `System::Collections::detail::MutationCounter` (64-bit unsigned) and
the enumerator snapshots `detail::MutationVersion`. `sizeof`, `alignof`, and the
counter's own byte offset are all unchanged — measured, not assumed — because the
widening consumed padding the type already had at that position.

Repository-wide, three defect classes were reproduced against the committed
pre-fix headers before anything changed
(gitignored `build-probe-collversion/probe2_defects.cpp`): fourteen UBSan
signed-integer-overflow reports at `++version_`, fifteen 2^32
snapshot-reuse (ABA) reproductions, and — recorded in neither #1786's nor
#1787's description — an assignment defect that transplanted the *source's*
counter into the destination and needed no overflow at all, with six
AddressSanitizer `heap-use-after-free`/`heap-buffer-overflow` reproductions. The
full record, including the .NET comparison and the per-type layout measurements,
is `docs/CollectionVersionCounterSweep.md`.


## Post-remediation follow-up: design ticket #1790 (2026-07-28)

Ticket #1790 (`REMED-COLL-LIST-INDEXER-VERSION`, P3, size L, category `parity`)
completed a **design-only** investigation of this file's mutable-access surface.
It carries **no `SR-AUD-*` identifier** — the numbering is frozen at 364 and this
divergence was recorded during remediation, by ticket #1787's Category D
classification — and it reopens no finding here. The original evidence above is
retained unchanged. **No production behaviour, signature, layout, or exception
changed**; the single edit to `List.hpp` is a doc-comment correction.

The investigated defect: `List<T>::operator[]` (non-const) returns a plain `T&`,
so `list[i] = value` cannot advance the mutation counter, and an outstanding
enumerator is not invalidated — unlike real .NET, whose index setter bumps
`_version` unconditionally (`List.cs:162`), including for an equal-value write.
Reproduced with the counter read directly: the native iterator, the fail-fast
enumerator, the equal-value write, and the write through `IList<T>&` all leave
the counter at rest, while the `Add()` control correctly invalidates.

**Three findings extend or correct what was previously recorded about this
file:**

1. **The non-const `ToVector()` is a wider hole than the indexer.** It returns
   the whole backing `std::vector<T>&`, so `push_back`, `erase`, `resize`, and
   `clear` are all reachable — **structural** mutations the fail-fast guard never
   sees, where the indexer can only replace an existing element. Reproduced with
   an enumerator outstanding: `Count` went to 0 and the guard stayed silent. This
   was documented nowhere before #1790; the class comment even called the indexer
   "one narrow, documented gap". That comment is corrected by this ticket.
2. **The `T&` is a memory-safety hazard, not only a parity divergence.** Four
   AddressSanitizer `heap-use-after-free` reports: a retained `T&` read after a
   reallocating `Add()`, written after one, read after `Clear()`, and read after
   move assignment of the whole list.
3. **Measured, not estimated, call-site data.** Compiling all 625 translation
   units against a `[[deprecated]]`-tagged shim of the committed header: the
   non-const indexer has **61 call sites, all in two test files**; `begin()` 3,
   `end()` 3, `ToVector()` 1; `IList<T>::operator[]` **0**. No library source in
   the repository includes `List.hpp` at all. CNA and mobile-eggbert were not
   inspected and their usage is explicitly not covered by these figures.

The selected architecture is a tracked proxy return
(`System::Collections::detail::ElementReference<T>`), handed to implementation
ticket **#1791**, opened `blocked` pending the four-part approval recorded in
`docs/ListIndexerVersioningDesign.md` section 28. Declaring the divergence
permanent was considered and rejected, because of finding 2 above. Fourteen
permanent regressions now pin both the contract that must survive and the
divergence that must be flipped
(`modules/collections/tests/System/Collections/Generic/ListIndexerVersionTests.cpp`).

A separate, newly discovered defect in `Generic/IEnumerator.hpp` — a
`const_cast` publishing a mutable `void*` to the live element, affecting every
collection — is filed as ticket **#1792** and is deliberately not absorbed here.
