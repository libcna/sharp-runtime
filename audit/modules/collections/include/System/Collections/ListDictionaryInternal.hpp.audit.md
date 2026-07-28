# Audit: `modules/collections/include/System/Collections/ListDictionaryInternal.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## Assessment

The raw `ICollection::CopyTo(void*, int)` implementation was traced as part of SR-AUD-358.  It cannot validate destination capacity and writes through the supplied pointer without a null/index boundary check. The public declaration, its immediate implementation path, and focused call sites were reviewed.  No separate evidence-backed finding is assigned to this file beyond the related findings recorded in their owning reports.

## Missing assertions and diagnostics

- Keep invalid-input, lifecycle, ownership, and native-boundary diagnostics covered by focused tests as this surface evolves.

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

## Post-audit note — design ticket #1795 (2026-07-28)

**No `SR-AUD-*` identifier**: the numbering is frozen at 364 and this was found
during remediation. Design ticket #1795 measured
`ListDictionaryInternal::NodeEnumerator` and found the opposite profile from
`Hashtable::Enumerator` under the same interface:

- `getKeyProperty()` and `getValueProperty()` return **the caller's own
  pointers**, never dictionary storage. Writing through them after a
  `const_cast` hits the caller's object, cannot corrupt the dictionary, and does
  not affect lookup, because this dictionary compares keys by **address**. The
  const-correctness and version-bypass classes therefore genuinely do **not**
  apply here — which is why "`const void*` is unsafe" is the wrong summary of
  this interface.
- The lifetime class applies **more** broadly here than on `Hashtable`. The
  enumerator caches nothing, so **all four** accessors — including
  `getEntryProperty()` and the `std::any`-returning `getCurrentProperty()` that
  ticket #1793 already migrated — dereference the `std::list` iterator on every
  call, with no version check. After `Clear()` or the dictionary's destruction
  each is an AddressSanitizer `heap-use-after-free`. .NET is safe here only
  because its `current` is a GC-rooted strong reference to the node.
- Two parity defects: `getCurrentProperty()` boxes the **key**, where .NET is
  `public object Current => Entry;`; and the `const` on a value is spelled three
  different ways — `DictionaryEntry::Value` is `void*`, the value view's
  `Current` is `const void*`, and `MemberCollection::copyToCore` writes `void*`.

The selected fix adds a `DictionaryEntry current_` snapshot filled in
`MoveNext()`, which grows this private nested class from 40 to 72 bytes and, via
the `inline` `GetEnumerator()`, is a measured stale-object hazard for any
consumer that is not fully rebuilt. Implementation is ticket #1794, `blocked`.
Full record: `docs/IDictionaryEnumeratorKeyValueSafetyDesign.md`.
