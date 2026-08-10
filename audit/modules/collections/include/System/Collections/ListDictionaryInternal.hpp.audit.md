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


### Remediated by ticket #1794 (2026-07-28)

`NodeEnumerator` gained the `DictionaryEntry current_` snapshot, filled by
`MoveNext()` only after the version check passes and only when positioned, and
cleared by `Reset()`. **All four accessors now read only the snapshot**, so none
of them dereferences the `std::list` iterator — closing the three
use-after-frees that reached `getEntryProperty()` and the already-owning
`getCurrentProperty()` and that no return-type change could have closed.

Both parity defects were corrected, under explicit approval:
`getCurrentProperty()` boxes the `DictionaryEntry`, matching .NET's
`public object Current => Entry;`; and the value view boxes `void*`, agreeing
with `DictionaryEntry::Value` and `copyToCore` where it previously agreed with
neither. `ListDictionaryInternalTest.Values_ReflectsContents` and
`EnumeratorCurrentSafety.ListDictionaryInternalPreservesTheConstOnItsBoxedKey`
were **updated, not deleted** — the latter still asserts that the caller's
`const` survives, now on the entry's Key, on `getKeyProperty()`, and on the key
view.

Confirmed costs: this private nested class grows **40 → 72 bytes**, re-measured,
and the stale-object hazard through the `inline` `GetEnumerator()` was reproduced
as ASan `heap-use-after-free`. `MoveNext()` also became more expensive — **2.8 →
23.9 ns per position** — because it now builds the snapshot; that number was not
in the design record and is added by §37.1. `sizeof(ListDictionaryInternal)` is
unchanged at 40.

Still asymmetric, predating this ticket and outside its approval: the key view
boxes `const void*` while `copyToCore` normalises the key to `void*`. Full
record: `docs/IDictionaryEnumeratorKeyValueSafetyDesign.md` §37.

## Post-audit note — design ticket #1799 (2026-07-29)

**No `SR-AUD-*` identifier**: the numbering is frozen at 364 and every defect
below was found during remediation. Design ticket #1799
(`REMED-COLL-LISTDICT-SETITEM-DESIGN`, P3, size M, `done`) made **no production
change**. Durable record: `docs/ListDictionaryInternalSetterDesign.md`.
Implementation is ticket #1798, which stays `blocked`.

#1798 was opened by #1797 naming **two** defects. Measured against the committed
headers, there are **six**, and two of #1798's stated facts are wrong:

- **`setItem`'s replace branch returns before `++version_`** — confirmed
  (version `3 → 3` while the stored value changed). Four outstanding enumerator
  kinds then walked to the end without throwing — the dictionary enumerator, the
  key view, the value view and the same through an `IDictionary&` — and the
  **value view enumerated the post-mutation value**, with **0 AddressSanitizer
  and 0 UndefinedBehaviorSanitizer reports**.
- **All five raw-key entry points accept `nullptr`** — confirmed, and a null key
  is stored, found, enumerated, copied out and removed. But the rationale
  #1798 borrows from SR-AUD-363 does **not** transfer: keys here are compared by
  raw address, no valid object has the null address, and a null key was measured
  **not** to alias any real key. The defect is that the **two `IDictionary`
  implementations disagree**, not that anything collides.
- **Not in #1798, and the only one with a hard failure:**
  `MemberCollection::copyToCore` boxes `const_cast<void*>(n.key)` for the key
  view, where the enumerator's `Key`, `DictionaryEntry::Key`, the key view's
  `Current` and the typed `CopyTo` all box `const void*`. One view, two element
  types: `std::any_cast<const void*>` on a `CopyTo` slot throws
  `std::bad_any_cast`, and writing through the `void*` it hands out for a key
  the caller declared `const` was reproduced as an **AddressSanitizer `SEGV` on
  a write to read-only storage**. Reaching the same object through `Current`
  cannot do this. This is the residual asymmetry
  `docs/IDictionaryEnumeratorKeyValueSafetyDesign.md` §37 recorded as outside
  #1794's approval.
- **Not in #1798:** `Add` on a duplicate key and `Remove` of an absent key both
  diverge from .NET `ListDictionaryInternal` in the **opposite** direction from
  the setter — .NET bumps `version++` first and unconditionally, so both
  invalidate every enumerator there and neither does here. **.NET's own
  `Hashtable` does neither**, so "match .NET" is not by itself a specification;
  the design selects *advance on effective mutation* and records the two
  deviations explicitly.

The selected fix adds a private `ValidatedKey` boundary and a single
`findNode()` locator, making null validation **structurally unskippable** rather
than conventional, and places the version bump **after** the mutation for a
strong exception guarantee. **No signature, return type, parameter type or data
member changes**: 53 of 53 mangled names byte-identical, the 19-entry vtable
identical, `sizeof(ListDictionaryInternal)` unchanged at **40** and every nested
type unchanged. The stale-object hazard is therefore **silent** rather than
crashing — and link-order dependent: at `-O0` with a stale object first on the
link line, a correctly *rebuilt* translation unit reverted to the defective
bodies, and `-flto -Wodr` diagnosed nothing.

Implementation needs **three explicit approvals** (design §36) and is not
begun. A separate inactive ticket #1802 carries the `Hashtable::Remove`
absent-key over-bump this design measured on the sibling implementation.

### Remediated by ticket #1798 (2026-07-29)

All six defects design ticket #1799 measured are closed, under its three
explicit per-action approvals. **No `SR-AUD-*` identifier**: the numbering is
frozen at 364 and every one was found during remediation. The pre-fix evidence
above is retained unchanged.

**Validation is now structural.** A private `ValidatedKey` throws
`System::ArgumentNullException("key")` on `nullptr`, and the single
`findNode(ValidatedKey)` locator — in `const` and non-`const` overloads — is the
**only** place in the class that compares a key against `list_`. A public method
therefore cannot reach storage without validating, which is what distinguishes
the selected shape from the rejected `Hashtable`-style `toKey()` helper: that one
is a convention a sixth entry point can forget. A negative consumer fixture
compiles the claim (6 of 6 sites rejected: `ValidatedKey` cannot be constructed,
constructed from `nullptr`, or named; neither `findNode` overload is callable;
`Node` cannot be named).

**`setItem` is one upsert** — validate, locate, then replace-and-bump or
`push_back`-and-bump — with the bump **after** the mutation, giving a strong
exception guarantee .NET's bump-first indexer setter cannot offer. A replacement
now invalidates outstanding enumerators, **including an equal-value
replacement**: the value is never compared, because equality of a `void*` is
address equality and .NET compares neither. `Remove` erases the single located
node instead of scanning with `remove_if`. `Clear` is unchanged and still bumps
unconditionally, matching .NET. `MemberCollection::copyToCore`'s
`const_cast<void*>` is deleted, so every key surface boxes `const void*` and the
recorded AddressSanitizer SEGV on read-only storage is **unreachable** — the cast
that used to yield the writable pointer now throws `std::bad_any_cast` first.

**Two deliberate deviations from .NET `ListDictionaryInternal`**, approved and
asserted as contract: a throwing duplicate `Add` and a `Remove` of an absent key
do **not** advance the counter. .NET does both, which would have introduced two
new false-positive `InvalidOperationException`s; .NET's own `Hashtable` does
neither. The rule is *advance on effective mutation*.

**Four of #1799's own figures are corrected** (design §37.1): one existing
assertion changed for the null-key row, not zero — `HashtableValueAccessSafetyTests.cpp`'s
`ListDictionaryInternalStillAcceptsANullKeyAndThatIsTicket1798`, which ticket
#1796 planted deliberately as "a test to flip", and which was **flipped, not
deleted**; the stale-object hazard is link-order dependent at **both** `-O0` and
`-O2`, not only `-O0`; the new-symbol count is 10 lines rather than 7, none of
them a `ListDictionaryInternal` symbol; and the "+0.2 ns per replace" is below
run-to-run noise, while "0 allocations added" holds exactly.

**No signature, return type, parameter type or data member changed**: 53 of 53
mangled names byte-identical, the 19-entry vtable identical with `getItem` at 72
and `setItem` at 80, `this` still in `%rdi` with no hidden `sret`, and
`sizeof(ListDictionaryInternal)` unchanged at **40**. `ValidatedKey` is 8 bytes,
appears in no public signature, and emits no symbol at `-O2`. Because of that
identity, a consumer that is not rebuilt does **not** crash — it silently keeps
the defect, and with a stale object first on the link line a correctly *rebuilt*
translation unit reverts too. **A full consumer rebuild is mandatory**, recorded
in `README.md`.

`Collections.Core` 2,371 → **2,437**; repository 13,657 → **13,723 across 37
executables**, from a fully fresh configuration and clean-first rebuild with 631
objects and 0 stale. ASan + UBSan + LSan clean on the whole suite and both
consumer fixtures, LeakSanitizer proved active by a 350-byte self-test.

Still not claimed closed: address-based key comparison; `MoveNext`/`Reset` after
the collection is destroyed; a view or enumerator outliving its dictionary; the
silent stale-object hazard; and the cosmetic duplicate-`Add` message divergence.
`Hashtable` was **not** modified — its absent-key `Remove` over-bump is inactive
ticket **#1802**. Full record: `docs/ListDictionaryInternalSetterDesign.md` §37.
