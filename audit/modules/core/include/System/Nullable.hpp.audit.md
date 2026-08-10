# Audit: `modules/core/include/System/Nullable.hpp`

## Metadata

- Audit status: AUDITED (230-line public template header, fully read).
- Supporting validation: dedicated `NullableTest.*:NullableHelperTest.*`
  passed 24/24; the complementary nullable smoke cases in pending
  `SystemTypesRemainingTests.cpp` passed 23/23 on 2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-nullable-audit-probe.cpp` prints `0,0,0`
  for NaN/finite and NaN/NaN comparisons and `0,0` for instance/static NaN
  equality.

## Assessment

The optional-backed storage, empty access diagnostic, conversions, text
fallback, and null ordering shape are coherent for ordinary value-like C++
types.  The header intentionally has a broader template domain than C#'s
`where T : struct`; it relies on `std::optional`, default construction,
`std::hash`, comparison, and either `ToString` or stream insertion from the
chosen T.  These constraints should be documented or expressed explicitly in a
future API pass.

## Finding references

- **SR-AUD-046 (extended):** `NullableHelper::Compare` uses raw `<` in both
  directions and `Nullable<T>::Equals` / `NullableHelper::Equals` use raw
  `==` through `std::optional`.  With `Nullable<float>(NaN)` and a finite
  value, all comparisons return `0`; NaN also compares unequal to itself.
  Current local .NET `Nullable.cs` instead delegates Compare to
  `Comparer<T>.Default` and static Equals to `EqualityComparer<T>.Default`,
  whose floating behavior orders NaN before finite values and treats NaN equal
  to itself.  The standalone probe confirms the local divergence.

## Other missing assertions and diagnostics

- Direct tests use only `int` and a streamable widget.  They omit float/double
  NaN, signed zero, custom `IComparable`/equality types, non-default-
  constructible values, and a type that lacks `std::hash`.
- `GetHashCode()` narrows `std::hash<T>` to `intcs`; no portability assertion
  documents the result for a wide platform hash.
- `GetValueOrDefault()` needs `T{}` but the class has no concept requiring it;
  diagnostics occur at template instantiation rather than at the public type
  boundary.

## Final assessment

Ordinary nullable behavior is well represented, but its generic comparison and
equality path extends the confirmed raw-C++ floating-comparison defect.  No
source or test was modified during this audit.


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
