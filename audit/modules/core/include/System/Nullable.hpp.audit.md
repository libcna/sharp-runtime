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
