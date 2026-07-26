# Audit: `modules/core/tests/System/NullableTests.cpp`

## Metadata

- Audit status: AUDITED (139 lines, 24 tests, fully read).
- Validation: `NullableTest.*:NullableHelperTest.*` passed 24/24 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The direct suite covers construction, empty-value access, default selection,
ordinary equality, hash zero for empty, string conversion (including the member
`ToString` preference), explicit conversion, bool/nullopt behavior, and
nullable helper ordering/equality.  Its all-`int` comparison data gives good
ordinary state coverage but cannot exercise the generic comparer semantics
claimed by the helper.

## Finding references

- **SR-AUD-046 (extended):** no test uses `Nullable<float>` or
  `Nullable<double>` NaN.  The audit probe shows raw `<` returns equality for
  NaN versus a finite nullable and raw equality makes NaN unequal to itself,
  unlike .NET's default comparer/equality-comparer behavior.

## Other missing assertions and diagnostics

- `GetHashCode_WithValue` requires a nonzero hash for `42`, which happens to be
  true for the local integer hash but is not a general nullable hash contract.
- No test covers a hashable/streamable constraint failure, move-only value,
  signed zero, nullopt comparison in reversed operand order, or the full
  pending smoke suite's duplication.

## Final assessment

The ordinary nullable-state coverage is useful, but floating generic semantics
and template constraints are missing.  No test was modified during this audit.
