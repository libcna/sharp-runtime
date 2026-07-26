# Audit: `modules/core/tests/System/LinqTests.cpp`

## Metadata

- AUDITED: 303-line dedicated fixture, fully read.
- Validation: `LinqTests.*` passed 45/45 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.

## Findings

The fixture broadly exercises ordinary integer/vector success paths, empty
sequence defaults, stable ordering, and the previously repaired First/Min/Max
exception category. Every callable is nonempty, arithmetic stays well within
range, and equality/order tests use only integers. Consequently all tests pass
while leaving SR-AUD-134, SR-AUD-135, and the LINQ extension of SR-AUD-046
unobserved.

## Missing assertions and diagnostics

- Missing empty `std::function` callbacks on empty and nonempty vectors for
  every predicate/selector/key-selector overload.
- Missing checked signed Sum/selector overflow and expected System exception
  assertions.
- Missing float/double NaN, signed-zero, custom equality/comparer, and
  stateful/throwing selector vectors.
- Missing negative Skip/Take, empty Where/Select, copy independence, huge
  Count, and full partial-API/overload diagnostics.
- The test does not show that OrderBy key selectors may run multiple times,
  unlike the single-materialization .NET enumerable contract.

## Final assessment

Strong basic algorithm smoke coverage, but normal happy paths lock in none of
the confirmed boundary defects. No source or test was modified during this
audit.
