# Audit: `modules/buffers/include/System/Buffers/SearchValues.hpp`

## Metadata

- Audit status: AUDITED (98-line public header-only implementation, fully
  read).
- Validation: `SearchValuesTests.*:SearchValuesFactoryTests.*` passed 8/8
  within the complete 37/37 Batch16 focused filter in
  `SharpRuntimeTests_Buffers` on 2026-07-26.
- Reproducer: compiling
  `/tmp/sharp-runtimervc-searchvalues-equality-probe.cpp` with C++20 fails at
  construction and `Contains`: `std::hash<EqualityOnly>` is deleted. The type
  provides only `bool operator==`.
- Reference: local .NET `SearchValues.cs`, `SearchValues.T.cs`, and its
  exhaustive byte/char membership tests were reviewed.

## Assessment

For built-in hashable values the unordered-set implementation provides correct
membership and duplicate elimination, while returned value order is correctly
treated as unspecified by the source tests. Its public template documentation
and constructors promise a broader equality-based type surface than the chosen
container can instantiate.

## SR-AUD-077 — medium — SearchValues<T> documents equality-only support but imposes an undocumented std::hash requirement

The header says `T` must be equality-comparable and offers public generic
vector/initializer-list construction. Its `std::unordered_set<T>` backing also
requires a usable `std::hash<T>`. The standalone equality-only value type fails
to compile in both constructor and `Contains`, reporting deleted
`std::hash<EqualityOnly>`.

The current .NET base type is constrained by `IEquatable<T>` rather than a hash
contract. Its built-in public factories specialize byte/char/string selection,
but this C++ header explicitly broadens construction to arbitrary equality
comparable `T`; that exposed extension must either use equality-only storage,
accept a hasher template parameter, or state/enforce the additional requirement
with a concise public diagnostic.

## Other missing assertions and diagnostics

- The eight direct cases instantiate only built-in `int`, `uint8_t`, and
  `char`; no equality-only, custom-hash, move-only, throwing-hash, duplicate,
  empty, or large set is tested.
- No test asserts that `GetValues` is unordered/set-like, independent of the
  input vector after creation, or preserves membership after copy/move.
- Public constructors expose concrete creation where .NET reserves derived
  SearchValues construction to CoreLib and uses its static byte/char/string
  factories. The broader C++ API needs an intentional extensibility decision.
- The local source has optimized span search integration beyond `Contains`.
  This port offers only membership, so performance and `IndexOfAny`-family API
  breadth require a documented scope decision rather than inference from the
  type name.

## Final assessment

Built-in membership paths pass, but the advertised generic equality contract is
false at compile time for ordinary equality-only C++ values. No source or test
was modified during this audit.

## Post-audit record for SR-AUD-077 (ticket #2054, 2026-08-04): REMEDIATED

The audit evidence above is retained unchanged. SR-AUD-077 is now **`remediated`**. The owning
review is
[`docs/BuffersNamespaceReviewPlan.md`](../../../../../../docs/BuffersNamespaceReviewPlan.md)
(ticket #2048) §4.4/§23.4; **no `SR-AUD-*` identifier was issued.**

The review grouped SR-AUD-077 with SR-AUD-070 as one root cause — *a public generic surface
silently requires more of `T` than it documents* — spanning **five** sites across four
headers, and gave the pair a single compatible ticket, **#2054** (`done`, 2026-08-04). The planned repair
states each requirement in the doc-comment and adds a `static_assert` **at the point where the
requirement is already enforced**, so exactly the same set of programs continues to compile
and only the diagnostic improves; a negative consumer fixture site proves the equality-only
and non-default-constructible cases are still rejected. A class-scope assert is explicitly
rejected: it would reject a mere declaration that compiles today, which would be a source
break.

The family is **deliberately not minted as a CCF**: two findings inside one module is not yet
a cross-cutting pattern. The review's §22 records the promotion rule — if a second module's
review finds the same shape, mint CCF-021 then, citing both modules' evidence.

`SearchValues<T>`'s immutability after construction and its independence from the source
vector — neither previously asserted — are now pinned by
`SearchValuesPinTests.IsImmutableAfterConstructionAndIndependentOfItsSource` (#2061).

### What landed

The `@tparam` sentence that promised equality-only support — *"Must be equality-comparable"* —
is replaced by an explicit statement of what this port actually needs: equality **and** a
usable `std::hash<T>`, which is strictly more than .NET's `IEquatable<T>` constraint. The
workaround is named in the same block: supply a `std::hash<T>` specialization for the
equality-only type. Both constructors `static_assert` the two halves **separately**, so the
message tells the caller which half is missing, and an equality-only `T` now fails with that
sentence instead of

```
error: 'std::__hash_enum<_Tp, <anonymous> >::~__hash_enum() [with _Tp = EqOnly; …]'
    is private within this context
```

The asserts are in the constructor **bodies**, not at class scope. Measured before and after:
`SearchValues<EqualityOnly>*` and `sizeof(SearchValues<EqualityOnly>)` compile, and both
constructors do not — an identical acceptance set on either side of the change.

The two predicates are `System::Buffers::detail::searchValuesHashUsable<T>` and
`searchValuesEqualityUsable<T>`, `requires`-expressions rather than trait probes of
`std::hash<T>`, so they answer without assuming that specialization is a complete type.
`BuffersGenericRequirementsTests.cpp` pins both directions of both predicates — false for the
equality-only type, true for `std::uint8_t`, `char`, `int` and `std::string` — so the negative
fixture cannot silently stop proving anything.

**Site count correction:** the review counted five sites for the pair SR-AUD-070 + SR-AUD-077;
there are **six** production sites plus `SearchValues`, plus two already-documented
`SequenceReader` sites. The sixth is `ArrayBufferWriter(intcs initialCapacity)`. See the
`ArrayBufferWriter.hpp` report and `docs/BuffersNamespaceReviewPlan.md` §23.4.

**Not remediated, and not claimed:** the underlying design question — whether an equality-only
`T` *should* be storable, i.e. whether `SearchValues<T>` should fall back to a linear container
when `std::hash<T>` is unavailable — is untouched. The finding is closed on the disclosure
reading it was written under ("documentation promises equality-only support" — it no longer
does), not on a widening of what the type accepts. No such widening was requested or approved.
Source, ABI and behaviour consequences: none.
