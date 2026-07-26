# Audit: `modules/numerics/tests/System/Numerics/TotalOrderIeee754ComparerTests.cpp`

## Metadata

- Audit status: AUDITED (63 lines, 7 tests, fully read).
- Validation: `TotalOrderIeee754ComparerTests.*:DivisionRoundingTests.*`
  passed 7/7 in `SharpRuntimeTests_Numerics` on 2026-07-25.

## Assessment

The suite covers a few representative finite, infinity, NaN, and signed-zero
ordering cases and verifies the five enum values.  It exercises neither the
full IEEE total-order edge domain nor the equality-comparer portion required by
the .NET counterpart.  As a result it passes even though the local comparer
cannot bind to the already-available `IEqualityComparer<T>` interface.

## Finding references

- **SR-AUD-042:** no test requires a `TotalOrderIeee754Comparer<T>` to satisfy
  `IEqualityComparer<T>` or tests its absent `Equals`/`GetHashCode` behavior.
  See [`TotalOrderIeee754Comparer.hpp.audit.md`](../../../include/System/Numerics/TotalOrderIeee754Comparer.hpp.audit.md).

## Required post-audit verification

Add a matrix for every specialization covering `-qNaN`, `-sNaN`, negative
infinity, finite negatives, `-0`, `+0`, finite positives, positive infinity,
`+sNaN`, and `+qNaN`, with deliberately distinct payloads.  Assert comparison
antisymmetry/transitivity and equality/hash agreement.  Bind the comparer as
both `IComparer<T>` and `IEqualityComparer<T>` to make the intended contract
compile-time visible.

## Other missing assertions and diagnostics

- `Half` checks only `-1 < +1`; it does not exercise its raw-bit NaNs, signed
  zero, infinity, subnormals, or payload order.
- `double` has no NaN or infinity test.
- `DivisionRounding` has no consumer because the corresponding division APIs
  are explicitly unimplemented; the test only validates declaration values.

## Final assessment

The happy-path ordering and enum declarations pass, but the test suite cannot
catch the confirmed comparer-interface omission or most total-order boundary
regressions.  No test was modified during this audit.
