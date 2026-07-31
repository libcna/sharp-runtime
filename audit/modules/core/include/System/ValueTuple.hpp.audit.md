# Audit: `modules/core/include/System/ValueTuple.hpp`

## Metadata

- Audit status: AUDITED (874-line public template header, fully read).
- Supporting validation: the combined dedicated and aggregate
  `ValueTuple*` Core.Base filter passed 53/53 on 2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-valuetuple-audit-probe.cpp` builds with
  `c++ -std=c++20 -Wall -Wextra -Wpedantic -I modules/core/include` and prints
  `0,0,0,0,0,0` for `ValueTuple1<float>` and `ValueTuple8` NaN comparisons /
  self equality where .NET's default comparers produce ordering and equality.

## Assessment

The zero through eight element layouts, factory flattening convention, and
ordinary lexicographic behavior are coherent for the covered integral and
string-like values.  The design deliberately uses C++ template operations:
stream insertion for text, `std::hash<T>` for hashes, `==` for equality, and
`<` for comparison.  That much broader domain than C#'s value-type/
comparer-based generic surface is not expressed through concepts; errors emerge
only when a selected member is instantiated.  `ValueTuple8` additionally
requires a `Rest` type with `Equals`, `GetHashCode`, `CompareTo`, and
`ToString`, without a public constraint or early diagnostic.

## Finding references

### SR-AUD-046 — medium — raw C++ tuple comparisons do not preserve .NET floating comparer semantics

Every generic tuple's `CompareTo` uses a pair of raw `<` operations and its
equality uses raw `==`; `ValueTuple8` applies the same policy to its first
seven elements and calls the same methods on `Rest`.  Thus a NaN item compares
as neither less nor greater than a finite item, so `CompareTo` returns zero,
and `Equals` says a tuple does not equal itself.  The standalone reproducer
shows this for `ValueTuple1<float>` and a NaN in `ValueTuple8`.

Current local .NET `ValueTuple.cs` instead calls `Comparer<T>.Default.Compare`
for each comparison and `EqualityComparer<T>.Default.Equals` for each equality
component.  Its floating comparer gives NaN a stable order relative to finite
values and its equality comparer treats NaN as equal to itself.  This extends
SR-AUD-046; it is the same confirmed policy defect already present in Array,
MemoryExtensions, and Nullable rather than an independent issue.

## Other missing assertions and diagnostics

- Direct tests use only integral and `std::string` values.  They omit
  float/double NaN, signed zero, a custom equality/comparison type, and a
  type with a valid .NET-like comparer but no raw C++ relational operators.
- No test verifies a hash/equality invariant for any tuple containing NaN or
  probes the implementation-defined narrowing of `std::hash<T>` to `intcs`.
- The direct suite has no `ValueTuple8` test; aggregate smoke tests cover its
  normal construction but not invalid `Rest`, nested rest, comparison, hash,
  or NaN behavior.
- There is no boundary diagnostic for an unsuitable `TRest` or for a selected
  `T` that lacks streaming, hashing, equality, or ordering.  A future API pass
  should make the intended C++ constraints explicit rather than allowing a
  member-specific template error.

## Final assessment

The normal arity/factory behavior is materially covered, but the generic
comparison/equality implementation extends the known floating-comparer parity
defect and its constraints are not diagnosable at the public type boundary.
No source or test was modified during this audit.


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
