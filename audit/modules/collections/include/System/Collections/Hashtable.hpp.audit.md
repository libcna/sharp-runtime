# Audit: `modules/collections/include/System/Collections/Hashtable.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-363 — medium — Hashtable accepts null keys and returns null instead of the public Keys/Values collections

The raw-key path stringifies a null pointer as `"0"`, so `Add(nullptr, …)` succeeds rather than reporting the required null-key argument failure.  The same type advertises `IDictionary.Keys` and `Values` but returns `nullptr` for both.  The direct probe prints `null-key=accepted count=1` and `keys-null=1 values-null=1`; callers cannot safely consume the promised views.

## Missing assertions and diagnostics

- Hashtable tests omit null-key rejection and never dereference/use Keys or Values through the IDictionary contract.
- Add boundary diagnostics for null keys and a lifetime-safe view implementation or an explicit unavailable-feature result.

## Post-audit remediation (ticket #1775, 2026-07-27): REMEDIATED

The audit evidence above is retained unchanged. Ticket #1775
(`REMED-COLL-HASHTABLE-VIEWS`, P1, size M) closed both halves of the finding on
branch `feature/remediation-coll-hashtable-views`.

Two facts beyond the original probe were established before the repair:

- The null view is not merely an absent feature. A consumer that follows the
  `IDictionary` documentation and uses the promised view without a null check
  is an ASan-confirmed SEGV plus a UBSan `member access within null pointer of
  type 'struct ICollection'`, while the sibling `ListDictionaryInternal`
  answers the *identical* caller code correctly. That makes this an interface
  defect with divergent implementations, not a Hashtable-local omission.
- The stringified null key `"0"` aliases the ordinary string key `"0"` accepted
  by the `Add(const std::string&, const std::any&)` overload: after
  `Add(nullptr, v)`, `ContainsKey("0")` is true and `Add("0", …)` is rejected as
  a duplicate of an entry the caller never added.
- A third null-key entry point was found: `Remove(const char*)` forwarded a
  null argument into `std::string`'s null construction and terminated with a
  `std::logic_error` that code catching `System::Exception&` cannot see.

Repair: `getKeysProperty()`/`getValuesProperty()` return a live, caller-owned
`MemberCollection` whose `Count`, `SyncRoot`, `IsSynchronized`,
`GetEnumerator`, and `copyToCore` delegate to the owning table, following the
`ListDictionaryInternal::MemberCollection` precedent already in this component
and matching .NET's `KeyCollection`/`ValueCollection`. The views reuse the
ticket #1771/#1774 copy boundary unchanged. `toKey()` became the single
validating conversion site through which every raw-key path passes, so
`getItem`, `setItem`, `Contains`, `Add`, and `Remove` reject a null key with
`System::ArgumentNullException("key")`, as .NET's `Insert`/`ContainsKey`/
`Remove`/indexer do; `Remove(const char*)` gets the same check. No non-null
address stringifies to `"0"`, so the key-space alias is structurally
unreachable. No public signature changed and no virtual member was added or
removed, so this is neither a source nor an ABI break.

Closure evidence: 70 permanent regressions in
`DictionaryKeyAndViewContractTests.cpp`, whose view cases are parameterised
over *both* non-generic `IDictionary` implementations; the same 70 under
ASan + UBSan + LeakSanitizer with no diagnostic and no leak; a 33-assertion
replacement probe (`build-probe-hashtable/probe2_fixed_boundary.cpp`,
`failures=0`) covering the previously fatal scenarios plus liveness,
non-trivial values, a 20,000-entry table, and destruction order;
`SharpRuntimeTests_Collections_Core` 1,732/1,732; a `-Werror` standalone
`Collections.Core` consumer fixture
(`test/consumer/collections_dictionary_views.cpp`) that compiles and runs; and
a network-permitted `scripts/local_ci_check.sh build` of 12,991/12,991 tests
across 37 executables with zero warnings/errors.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.

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
during remediation. Design ticket #1795 measured `Hashtable::Enumerator`'s two
`const void*` accessors and found a **write path into live dictionary storage**
that ticket #1794's own description asserted did not exist:

- `getValueProperty()` returns `&it_->second`, a pointer to the live
  `std::unordered_map`'s `mapped_type`, which is a **non-`const` `std::any`**.
  `const_cast` + assignment through it is not undefined behaviour — it is
  well-formed, defined C++ that rewrites the stored value, leaves the mutation
  counter unmoved, and is invisible to a second, independent enumerator. The
  rewritten value is then returned by `Hashtable::at()`.
- `getKeyProperty()` returns `&it_->first`, a `const std::string` inside the map.
  The write there *is* undefined behaviour, and at 64 entries it produces an
  entry that `getCountProperty()` still counts and that **no lookup can return
  by either its old or its new key**.
- Neither accessor performs the fail-fast version check `MoveNext()` and
  `Reset()` perform, so calling either again after a `Remove` or `Clear`
  dereferences an invalidated iterator — AddressSanitizer `heap-use-after-free`.
  `getEntryProperty()` and `getCurrentProperty()` are unaffected because they
  read the enumerator's own `current_` cache; that asymmetry is itself the
  design's evidence for making the cache mandatory.

Two **pre-existing, separate** write escapes on this class are recorded but were
**not** in scope: `operator[](const std::string&)` returns a non-`const`
`std::any&` and `getItem()` returns `const_cast<std::any*>(&it->second)`; both
bypass the mutation counter and both are already documented in the header as
narrow gaps. Implementation is ticket #1794, `blocked`. Full record:
`docs/IDictionaryEnumeratorKeyValueSafetyDesign.md`.
