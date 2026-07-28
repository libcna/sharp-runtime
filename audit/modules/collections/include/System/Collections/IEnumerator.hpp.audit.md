# Audit: `modules/collections/include/System/Collections/IEnumerator.hpp`

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

## Design ticket #1792 (2026-07-28): this file's `getCurrentProperty()` is the defect's root

Ticket **#1792** (`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, P3, size M) is
**done as a design ticket**. It carries **no `SR-AUD-*` identifier** — the
numbering is frozen at 364 and this was found during remediation, by ticket
#1790's mutable-access inventory — and it **reopens no finding**. The original
evidence above is retained unchanged and **nothing in this file was changed**;
#1792 is design-only.

The declaration at line 100 of this file is where the defect originates:

```cpp
[[nodiscard]] virtual void* getCurrentProperty() const = 0;
```

with the doc-comment *"Pointer to the current element; cast to the appropriate
type"*, which is the whole of the type contract. Measured against every
implementation in the repository, that one signature carries **ten distinct
lifetime rules** — live contiguous storage, live heap nodes, live associative
values, a live shared vector behind a read-only wrapper, a live hash-map key,
enumerator-owned snapshots, enumerator-owned single values, enumerator-owned
`mutable` caches, a caller-owned object, and the element itself by value — and
nothing in the signature distinguishes any two of them. That is why the file's
own comment cannot be made true by editing it: there is no single correct
sentence to write.

Reproduced against the committed pre-fix headers, all in the gitignored
`build-probe-ienumerator/`: **four AddressSanitizer `heap-use-after-free`
reports** (pointer retained across reallocation, `Clear()`, the collection's
destruction, and the enumerator's destruction); two non-faulting stale-aliasing
shapes across `MoveNext()` and `Reset()`, which show the interface has no
validity window at all; a `ReadOnlyCollection<T>` mutated through its own
enumerator, reaching the caller's shared backing vector; and a `Hashtable` entry
made unreachable by **both** its old and its new key while `Count` still reported
it. 0 UBSan diagnostics; 0 LSan leaks.

Note the contrast with this file's siblings, which is the design's own evidence
that the const-correct spelling is already the local convention:
`IDictionaryEnumerator::getKeyProperty()`/`getValueProperty()` return
`const void*`, `getEntryProperty()` returns `DictionaryEntry` **by value**, and
`Generic::IAsyncEnumerator<T>::getCurrentProperty()` returns `const T&`.

**Selected architecture:** this declaration becomes
`[[nodiscard]] virtual std::any getCurrentProperty() const = 0;`, the direct
counterpart of .NET's `object IEnumerator.Current`. `const void*` was evaluated
and **measured not to be a fix** — a one-line `const_cast` restores the write.

Implementation is ticket **#1793**
(`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`, P2, size L), opened
**blocked** and deliberately not begun; the approval it needs is stated verbatim
in `docs/IEnumeratorCurrentSafetyDesign.md` section 33, and includes
acknowledgement of a **silent ABI break**: the mangled name is byte-identical
before and after, and the calling convention is not.

## Implementation ticket #1793 closed: implemented (2026-07-28)

The design above landed under ticket **#1793**
(`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`, P2, size L), on local branch
`feature/remediation-coll-ienumerator-current-safety`, after the user granted
design section 33's three-part approval explicitly and scoped to that ticket.
**No new `SR-AUD-*` identifier; SR-AUD-356 stays `remediated`.** The evidence in
every section above is retained unchanged — the unsafe pointer, its four
AddressSanitizer `heap-use-after-free` reports, and the broken
`std::unordered_map` invariant stay on the record exactly as measured.

**This file changed.** The declaration is now:

```cpp
[[nodiscard]] virtual std::any getCurrentProperty() const = 0;
```

The doc-comment that promised *"Pointer to the current element; cast to the
appropriate type"* — the whole of the previous type contract, silent on
lifetime, mutability, and ownership — is replaced by the ownership and lifetime
rules the design's section 7 table showed could not be written for the old
signature: the returned box is the caller's, nothing invalidates it, writing to
it cannot reach the collection, and reading it advances no mutation counter. The
`getCurrentProperty()` naming collision with
`Generic::IAsyncEnumerator<T>` is recorded on the class as known and deliberate
so it is not "fixed" by accident.

Eight production non-generic overrides migrated with it — `ArrayList`,
`Hashtable` and its member view, `BitArray`, the non-generic `Stack` and
`Queue`, and `ListDictionaryInternal` twice — deleting all **four** `const_cast`s
outside the bridge and both `mutable` members. The three in-library call sites
(`ArrayList.hpp:108`, `Stack.hpp:47`, `Queue.hpp:47`) migrated with them; the
latter two gained a documented `std::bad_any_cast` path, where the old code
silently stored a pointer into the source collection's live storage.

The six lifetime shapes reproduced above were re-run in their migrated form
under ASan+UBSan: **0 reports**, where four were `heap-use-after-free`. LSan
reports 0 leaks and was proved active by a deliberate-leak self-test. A negative
consumer fixture writing the exact pre-fix source is rejected at all six marked
sites, and a positive fixture compiles under `-Wall -Wextra -Wpedantic -Werror`
and exits 0.

`IDictionaryEnumerator::getKeyProperty()`/`getValueProperty()` were **not**
changed and still return `const void*` into live storage. They are const-correct,
so no write path exists through them, but the type-safety, lifetime and
ownership-ambiguity classes remain open on that sibling interface. A warning now
sits on `IDictionaryEnumerator` itself pointing at
`docs/IEnumeratorCurrentSafetyDesign.md` sections 15 and 30 risk 4, and
migrating them is opened as ticket **#1794**
(`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, P3, size M, `blocked`, not begun): a
second public break on a second interface, needing its own approval.
