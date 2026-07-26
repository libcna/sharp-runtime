# Audit: `modules/numerics/tests/System/Numerics/NumericsRemainingTests.cpp`

## Metadata

- Audit status: AUDITED (276 lines, fully read).
- Validation: the `BitOperationsTests.*` filter passed 13/13 in
  `SharpRuntimeTests_Numerics` on 2026-07-25.
- File ownership note: this suite combines `MathF` and `BitOperations` tests;
  `MathF` implementation and coverage were assessed separately under
  `modules/core`.

## Assessment

The BitOperations tests establish basic 32-bit happy-path behavior for every
implemented operation, including zero count conventions and one rotate/reverse
case.  They do not test the 64-bit overloads that form most of the public
surface and do not specify the partial .NET adaptation boundary.  Consequently
they would not detect an incorrect 64-bit implementation, an offset-normalizing
regression, a power-of-two overflow regression, or the absent exact signed-64
trailing-zero entry point.

## Finding references

No confirmed test-only finding.  The associated implementation report records
the unresolved compatibility-baseline decision for `Crc32C` and exact signed
64-bit `TrailingZeroCount`:
[`BitOperations.hpp.audit.md`](../../../include/System/Numerics/BitOperations.hpp.audit.md).

## Required post-audit verification

Once the supported API baseline is selected, split or clearly label the mixed
suite and add a table-driven matrix for both 32-bit and 64-bit overloads:
zero, one, high bit, all ones, overflow boundaries, and signed extremes.  Test
rotations at `-1`, `width - 1`, `width`, and `width + 1`, and compare reverse
bits against a nontrivial known pattern.  Add a compile-time exact-overload
assertion where ABI/API parity is required, plus known-vector coverage if
`Crc32C` is adopted.

## Other missing assertions and diagnostics

- `RoundUpToPowerOf2(uint64_t)` is asserted only for zero; no nonzero or
  overflow case calls it.
- `LeadingZeroCount`, `Log2`, `PopCount`, `TrailingZeroCount`, and rotations
  have no 64-bit assertion.
- The lone reverse-bit assertions use zero and single set bits, which would not
  catch many swaps/mask errors in a staged implementation.
- Tests do not make the project-specific `ReverseBits` extension or omitted
  .NET `Crc32C` capability visible to future maintainers.

## Final assessment

The present tests validate a small normal 32-bit slice and all pass, but they
are insufficient to protect the larger public overload set or to document the
intended .NET compatibility scope.  No test was modified during this audit.
