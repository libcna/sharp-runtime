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


---

## SR-AUD-046 — REMEDIATED (CCF-010, tickets #1904-#1910, 2026-07-31)

The original evidence above is retained unchanged. **Only SR-AUD-046 is closed
by this family**; every other finding in this report stays `confirmed` and none
of them is touched.

One shared policy, `modules/core/include/System/detail/ComparisonPolicy.hpp`,
now states the port's counterpart of `Comparer<T>.Default` and
`EqualityComparer<T>.Default` once — `compareValues`, `equalValues`,
`hashValue`, `DefaultLess`/`DefaultGreater`, `moveNaNsToFront` and
`defaultSort`, each `if constexpr`-gated so every non-floating instantiation
generates exactly the code that was there before. All 66 comparison sites
across `Array.hpp`, `MemoryExtensions.hpp`, `Nullable.hpp`, `ValueTuple.hpp`,
`Tuple.hpp` and `Linq.hpp` route through it.

**The finding understates its own severity, and the correction is the point.**
`Array::Sort` and `MemoryExtensions::Sort` did not merely place NaN wrongly:
raw `<` over a NaN-bearing range is not a strict weak ordering, so
`std::sort`'s precondition was violated and the **finite** elements came out
unsorted — measured at **64 of 196** size/density/placement shapes, worst case
**3,874 inversions** in 65,536 elements, with **AddressSanitizer,
UndefinedBehaviorSanitizer and `_GLIBCXX_DEBUG` all silent**. Both now use
.NET's own repair (`ArraySortHelper.cs:285-305`): move every NaN to the front
in a pre-pass, then sort the NaN-free remainder, so no NaN ever reaches the
comparator. The 196-shape sweep reads `corrupted=0` afterwards.

All **28** measured defect rows now return .NET's value; the **8** rows that
were already correct are unchanged and now pinned — including
`Nullable<T>::operator==`, which is C#'s *lifted* `==` and is `false` for
NaN vs NaN in .NET too, and `MemoryExtensions::SequenceCompareTo` of two NaNs.
`Linq::Min` and `Linq::Max` follow the reference's deliberately **asymmetric**
NaN rules rather than being made symmetric.

Evidence: 70 permanent add-only regressions in
`modules/core/tests/System/ComparisonContractTests.cpp`; whole repository
**14,815 tests across 37 executables**, from 14,745; build clean with zero
errors and zero warnings; `nm --extern-only` identical before and after
(6,168 symbols); `sizeof`/`alignof` identical for all eleven measured
instantiations. **Ten mutations**, one of them a deliberate *negative* control
that must still pass. ASan/UBSan/LSan clean with activation proved separately;
TSan recorded **not applicable** — nothing in the family has shared mutable
state. Full record: `docs/ComparisonContractPlan.md`.

---

## SR-AUD-135 — REMEDIATED (ticket #2220, 2026-08-10)

The original evidence above is retained unchanged. **Only SR-AUD-135 is closed by this ticket.**
SR-AUD-134 and SR-AUD-046 in this report were already closed (CCF-011 / CCF-010) and are untouched;
the remaining "other missing assertions" observations stay open.

**This is a CCF-004 *occurrence*, not a new CCF-004 member.** CCF-004 stays **closed 8/8** and is
neither edited nor reopened — the same treatment the cross-cutting record gives T-F, T-A, T-C and
N-B. No `SR-AUD-*` identifier was issued; numbering stays frozen at **364**. The family record is
`docs/CoreDefinedArithmeticBoundedParseFamilyPlan.md`.

**Two corrections to the finding's premises, both measured** (`build-probe/2217_probe_before.cpp`,
`-O0`, `volatile` operands, one process per case, instrumented header-only production body).

1. **There are two sites, not one.** The finding says "both Sum overloads" but names only the value
   overload's line. Measured, `Linq.hpp:236` is the plain overload and `Linq.hpp:249` is the
   **selector** overload, and each needed its own repair; a fix applied to one leaves the other
   live. A permanent test covers the selector overload separately.
2. **The finding's domain is wider than the undefined behaviour, and the difference matters.**
   `Sum<unsigned>({UINT_MAX, 1})` returns `0` and `Sum<short>({SHRT_MAX, 1})` returns `-32768`,
   both with **no** UBSan report: unsigned arithmetic wraps by definition, and `short`/`signed char`
   are promoted to `int` before the addition so only the narrowing store is lossy (well defined
   since C++20). The finding's own remediation note asks for exactly this split — "separately
   specify unsigned/floating/custom type behavior rather than inheriting native overflow" — and the
   repair does so rather than sweeping them in.

**The sharpest measured case is one the finding's framing does not reach.**
`Sum<int>({INT_MAX, INT_MAX, INT_MIN})` executed undefined behaviour at the *first* addition and
then returned **`2147483646`, the mathematically correct total**. The conceptual result is
representable; only the intermediate is not. So the defect is not "the answer is wrong" — it is
that the program has no defined meaning, and a guard placed *after* the addition would have found
nothing to report here. Current .NET's `Enumerable.Sum` is checked **per element**, so it raises at
the same intermediate; the repair is faithful to the reference rather than to the final total, and
that is a **deliberate behaviour change** on a previously undefined path, pinned by a test.

**What changed.** One shared step, `System::Linq::detail::addForSum`, now drives both overloads.
For **signed integral `T`, excluding `char` and `bool`**, the sum is formed in the unsigned
counterpart (defined wrap) and the overflow recognised from the operand signs — so nothing undefined
is executed on the way to the diagnosis — and `System::OverflowException` is raised with .NET's
verbatim message `Arithmetic operation resulted in an overflow.`. `char` is excluded **by name**
because its signedness is implementation-defined, so throwing for it would make this port's
observable behaviour differ between x86-64 and AArch64, and .NET's `char` is not a summable numeric
type. Unsigned, floating and every other `T` are **unchanged and pinned**.

**Compatibility.** No `Linq::Sum` call site exists anywhere in this repository outside its own
tests, so nothing in-repo changes. For a downstream caller the argument is the one this repository
accepted for #1817, #1818, #1825, #1836 and #1837: the results that now throw were produced by
undefined behaviour and are not guaranteed between two builds of this repository — **they never
worked**. The one non-undefined change, `short`/`signed char`, replaced a silently truncated total
with a diagnostic. No new approval is required.

**Closure evidence.** ASan + UBSan + `float-cast-overflow` + LSan at `-O0` under
`-fno-sanitize-recover=all`, one process per case: all eight measured Sum shapes exit **0**, with
every before-diagnostic absent, and 1,000 consecutive throw/catch cycles leak nothing. Activation is
proved in the **same binary** by two deliberate controls that must still fire — a signed overflow
(exit 1, diagnostic at the probe's own line) and a 256-byte leak (exit 1, LeakSanitizer summary).
+13 permanent tests in `LinqTests.cpp` (`LinqCheckedSumTests`); `Linq*` 77/77.
**Five mutations, all killed**: detecting the overflow *after* a raw signed addition (killed by the
probe only — the gate build is uninstrumented and GCC's wrap yields the same value, so this one is
labelled as the weaker signal it is), widening the checked path to unsigned, over-rejecting any
same-sign pair, dropping the narrow signed types, and leaving the selector overload on the raw
accumulation (the last four each fail gate tests).

**Source, ABI and layout consequences: none.** Both `Sum` overloads are free function templates, so
no mangled symbol exists to change; no signature, `noexcept` specification or default argument
changed. `addForSum` is one new `inline` function template in the pre-existing `System::Linq::detail`
namespace and holds no state. `System/OverflowException.hpp` is now included.
