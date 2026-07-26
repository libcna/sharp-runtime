# Audit: `modules/core/include/System/Linq.hpp`

## Metadata

- AUDITED: 287-line header-only vector LINQ subset, fully read.
- Validation: `LinqTests.*` passed 45/45 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-linq-audit-probe` prints
  `where_empty_size=0`, `first_or_default_empty=0`,
  `nonempty_empty_predicate_throws=1`, `contains_nan=0`,
  `distinct_nan_size=2`, and `min_late_nan_is_nan=0`; the UBSan Sum probe
  reports signed overflow at `Linq.hpp:164` for `INT_MAX + 1` and exits 1.
- Reference basis: local .NET Enumerable `Sum.cs:13-44`, `Min.cs:70-146`,
  `Contains.cs:10-80`, `Distinct.cs:11-155`, and predicate overload sources.

## SR-AUD-134 — medium — LINQ callback overloads accept empty std::function values and make failure data-dependent

`Where`, `Select`, predicate `First`/`FirstOrDefault`/`LastOrDefault`, `Any`,
`All`, `Count`, selector `Sum`, and key-selector ordering overloads store their
callables as `std::function` but never validate them. An empty predicate on an
empty vector returns an ordinary empty result/default (`where_empty_size=0`,
`first_or_default_empty=0`), whereas a nonempty `Any` reaches native
`std::bad_function_call`. Current .NET rejects a null delegate deterministically
at the public argument boundary, before examining the sequence.

Validate each callable before enumeration and map the error through the
project's argument-exception policy. This extends CCF-011; simply allowing the
empty-sequence fast path preserves inconsistent public behavior.

## SR-AUD-135 — high — integral LINQ Sum performs unchecked signed C++ addition and reaches undefined behavior

Both Sum overloads use `result = result + item` (or selector result) for every
T. `Sum<int>({INT_MAX, 1})` reaches UBSan-confirmed signed overflow. Current
.NET `Enumerable.Sum(int)` uses checked accumulation and raises
`OverflowException`; the C++ generic form neither detects the overflow nor
limits its generic domain to types with defined additive semantics.

Use checked intermediate arithmetic and the project's overflow diagnostic for
supported signed integral types; separately specify unsigned/floating/custom
type behavior rather than inheriting native overflow or operator rules.

## SR-AUD-046 (extended) — medium — raw equality and ordering diverge for floating LINQ operations

`Contains` and `Distinct` use raw `==`, so `Contains({NaN}, NaN)` is false and
`Distinct({NaN, NaN})` retains two values. `Min` uses `std::min_element`; a
later NaN leaves a prior finite value, while current .NET explicitly returns
NaN to establish a stable floating minimum. `Max` and OrderBy also rely on raw
`<`/`>` rather than the local/.NET comparer contract. The probe confirms all
three implemented behaviors above. This extends the existing floating
comparison/equality finding rather than creating a second incompatible policy.

## Other missing assertions and diagnostics

- The documented partial surface lacks a formal capability/version boundary;
  callers see C++ vector-only eager materialization instead of .NET enumerable
  laziness, custom comparers, and broad overload families.
- Missing empty-callable tests on both empty/nonempty sources, callback throws,
  selector invocation counts, move-only/non-default generic values, and
  exception taxonomy vectors.
- Missing signed overflow, float NaN/signed-zero, custom equality/comparer,
  huge Count, and selector-overflow cases.
- OrderBy calls its key selector from the sort comparator rather than caching
  keys once per element; stateful selectors and exceptions have no documented
  contract or regression coverage.

## Final assessment

The ordinary vector algorithms are coherent for small integral pure callbacks,
but callback validation, signed Sum safety, and default floating semantics have
confirmed SR-AUD-134/135 and SR-AUD-046 gaps. No source or test was modified
during this audit.
