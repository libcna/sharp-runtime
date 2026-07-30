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

---

## SR-AUD-134 — REMEDIATED (ticket #1870, 2026-07-30, CCF-011)

The original evidence above is retained unchanged. **Only SR-AUD-134 is closed by
this ticket**; the other findings in this report — including SR-AUD-046's raw
`<`/`==` floating comparison, which this ticket deliberately does not touch —
remain `confirmed`.

All eleven callback overloads — `Where`, `Select`, `FirstOrDefault(predicate)`,
`First(predicate)`, `LastOrDefault(predicate)`, `Any(predicate)`,
`All(predicate)`, `Count(predicate)`, `Sum(selector)`, `OrderBy` and
`OrderByDescending` — now call one shared `detail::requireCallable` **before the
sequence is examined**, throwing `System::ArgumentNullException` with .NET's own
parameter name (`predicate`, `selector`, `keySelector`).

**Correction to the finding's premise (measured 2026-07-30).** Two refinements;
the finding's shape is confirmed, its boundaries were narrower than reality.

1. The finding says an empty callable "return[s] normal results on an empty
   vector" and throws "only when an item is reached". Measured,
   `OrderBy`/`OrderByDescending` also returned a normal result for a
   **one-element** vector (`linq.orderby=no-throw`,
   `linq.orderby.two=bad_function_call`, `build-probe/1866_prefix.log`), because
   `std::stable_sort` never invokes the comparator below two elements. The
   silent region was one element wider than stated for the ordering operators.
2. `First(empty, {})` did **not** return a normal result: it threw
   `System::InvalidOperationException("Sequence contains no matching element.")`,
   so the sequence error *masked* the argument error. .NET validates `predicate`
   before the sequence (`First.cs:110`), so the repair is a validation-*order*
   change there, not only an added check. A permanent test pins that the argument
   error now wins and that a real predicate still gets the sequence error.

The historical text above is left as written, per this repository's practice.

**Eager validation is exact, not an approximation.** .NET's `OrderBy` is lazily
enumerated, but its `keySelector` null check runs in `OrderedIterator`'s
constructor (`OrderedEnumerable.cs:80-82`) before any element is touched. These
operators are eager throughout, so checking at entry matches the reference
rather than merely approximating it.

**One observable behaviour change, deliberate.** A previously silent normal
result now throws — including `All(empty, {})`, whose vacuous `true` used to hide
the invalid argument entirely. A real predicate still gets the vacuous `true`,
and a test pins that. Recorded as B1 in `docs/EmptyCallableBoundaryPlan.md` §9.

Closure evidence: 8 new permanent regressions in `LinqTests.cpp` (every predicate
overload and every selector overload at length 0, 1 and 2; both ordering
operators below the two-element sort threshold; all three `paramName` values;
`First`'s argument-before-sequence order together with proof that a real
predicate still reaches the sequence error; `All`'s lost vacuous-true fast path;
catchability as `System::Exception`; and a regression pass asserting all eleven
operators still produce their pre-existing results with real callbacks).
`LinqTests` 53/53, `SharpRuntimeTests_Core_Base` 5,301/5,301, whole-repository
build clean with zero errors and zero warnings. The direct probe
`build-probe/1866_empty_callable_probe.cpp`, compiled **with**
`-fsanitize=address,undefined` so this header-only template change is itself
instrumented, exits 0 with zero AddressSanitizer, UndefinedBehaviorSanitizer and
LeakSanitizer reports (`build-probe/1870_postfix_asan.log`); across all 60 cases
**no** `bad_function_call` remains anywhere in the family, and the only
`no-throw` outcomes left are the eight deliberate event-subscription no-ops.

Source, ABI and layout consequences: none. Every operator is a free function
template, so no mangled symbol exists to change; no signature, `noexcept`
specification or default argument changed. The new `System::Linq::detail`
namespace holds one `inline` function template and no state.

The plan for this family is `docs/EmptyCallableBoundaryPlan.md` (ticket #1866).
