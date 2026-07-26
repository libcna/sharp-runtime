# Audit: `modules/core/tests/System/UInt128Tests.cpp`

## Metadata

- Audit status: AUDITED (227 lines, 40 tests, full read).
- Validation: `./build/SharpRuntimeTests_Core_Base --gtest_filter='Int128Tests2.*:UInt128Test.*:UInt128NewTests.*:BitConverterTests.*Int128*:BitConverterTests.*UInt128*' --gtest_color=no`
  passed 59/59 tests on 2026-07-25.

## Assessment

The suite provides useful coverage for basic operations, division-by-zero,
upper/lower-word construction, normal parse errors, one full-range decimal
round trip, and common X/D formatting.  It specifically improved former
parse/format gaps.  All shift checks remain in-range, and no invalid format
diagnostic is asserted.

## Finding references

- **SR-AUD-020:** the suite tests only `v << 4`; it omits shift counts 128+
  and negative counts that invoke undefined native shifts in `UInt128`.
- **SR-AUD-021:** it asserts valid formats only, leaving unknown `"Q"`,
  malformed `"Xz"`, and oversized precision to silently return decimal or
  leak `std::stoi` exceptions.

## Required post-audit verification

Add modulo-128 left/right tests at 128, 129, 255, and -1, including UBSan
coverage.  Assert `System::FormatException` for invalid/oversized format
tokens.  Add cross-word carry/borrow/multiplication and bit-127 decimal/hex
vectors before treating this suite as full-width arithmetic coverage.

## Final assessment

Good happy-path and parser coverage, but its two untested error/boundary paths
are exactly where the source audit confirmed defects.  No test was changed in
this audit-only phase.
