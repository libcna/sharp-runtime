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
