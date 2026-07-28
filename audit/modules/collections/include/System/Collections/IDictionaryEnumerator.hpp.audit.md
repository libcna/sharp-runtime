# Audit: `modules/collections/include/System/Collections/IDictionaryEnumerator.hpp`

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

## Post-audit note — design ticket #1795 (2026-07-28)

**No `SR-AUD-*` identifier is assigned, and none may be**: the audit numbering is
frozen at SR-AUD-001..364 and the material below was found during remediation,
not during the audit. This note exists so that a reader of this file is not left
with "no separate evidence-backed finding" as the last word on the type.

Design ticket **#1795** (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY-DESIGN`)
measured this interface in full. Summary of what is true of it today:

- `getKeyProperty()` and `getValueProperty()` return `const void*`. On
  `Hashtable::Enumerator` those point **into live `std::unordered_map`
  storage**; on `ListDictionaryInternal::NodeEnumerator` they are **the caller's
  own pointers**. One signature, two incompatible meanings, and no way for a
  caller holding an `IDictionaryEnumerator*` to tell which it has — reproduced
  as a `stack-buffer-overflow` from one correct function called against the
  other implementation.
- `Hashtable`'s value pointee is a **non-`const` `std::any`**, so `const_cast` +
  assignment through the `const` accessor is well-formed, defined C++ that
  rewrites live dictionary storage with the mutation counter unmoved and a
  second enumerator silent. The key pointee is a `const std::string`, where the
  write is undefined behaviour and, at 64 entries, leaves an entry that `Count`
  still reports and no lookup can return by either its old or its new key.
- **No accessor performs a fail-fast version check**, so all of them dereference
  a container iterator a mutation may have invalidated: eight AddressSanitizer
  `heap-use-after-free` reports, three of them on `ListDictionaryInternal`'s
  `getEntryProperty()`/`getCurrentProperty()`, whose return types are already
  owning values.
- Two `ListDictionaryInternal` parity defects: `getCurrentProperty()` boxes the
  key where .NET is `public object Current => Entry;`, and the `const` on a value
  is spelled three different ways across `DictionaryEntry`, the value view, and
  `copyToCore`.

Selected design: `Entry` stays canonical, `Key`/`Value` return an owning
`std::any` equal to its members, and every implementation snapshots the entry at
`MoveNext`. Implementation is ticket **#1794**, which stays `blocked` on a
four-item approval. Full record, including all probes and the ABI measurements:
`docs/IDictionaryEnumeratorKeyValueSafetyDesign.md`.

SR-AUD-356 stays `remediated` and CCF-018 is not reopened.


### Remediated by ticket #1794 (2026-07-28)

Landed under the design's four-item approval, granted in full. Both accessors now
return an owning `std::any` by value, and **both implementations answer every
accessor from a `DictionaryEntry` snapshot taken during a successful
`MoveNext()`** — the snapshot rule is written into this header as an invariant of
the *interface*, so a future implementation that reads its container inside an
accessor is wrong even with correct signatures.

One correction to the note above, made by re-measuring before any source changed:
the figure is **nine** AddressSanitizer `heap-use-after-free` reports, not eight.
The design record's §8.2 table listed nine and only its prose said eight.

The accessors deliberately did **not** gain a version check — .NET's do not
either, and the snapshot makes the resulting stale read safe rather than turning
a read .NET permits into an exception. `MoveNext()`/`Reset()` after the
collection is destroyed remain undefined and are not claimed closed.

Permanent suite: `DictionaryEnumeratorKeyValueSafetyTests.cpp` (+64 tests,
parameterised over both implementations). SR-AUD-356 and CCF-018 are recorded as
remediated by this ticket. Full record:
`docs/IDictionaryEnumeratorKeyValueSafetyDesign.md` §37.
