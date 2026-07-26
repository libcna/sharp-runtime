# Audit: `modules/core/tests/System/ByteTests.cpp`

## Metadata

- Audit status: AUDITED (288 lines, 79 tests, full read).
- Validation: `ByteTests.*` passed 86/86, including split-file extensions, on
  2026-07-25.

## Assessment

This is thorough for ordinary parsing, bit operations, rotation, logarithms,
and valid Clamp paths. It asserts malformed precision but not unknown format
types, and it contains no invalid-interval vector. The separate
`PrimitiveTypeTests2.cpp` extension supplies the useful `B` vectors.

## Finding references

- **SR-AUD-021:** missing `ToString("Q")` exception assertion permits the
  observed decimal fallback.
- **SR-AUD-022:** all Clamp vectors use ordered bounds; no test can reveal the
  invalid `std::clamp` precondition.

## Required post-audit verification

Add exact `System::FormatException` and `System::ArgumentException` assertions
for those two public invalid-input paths.

## Final assessment

Good small-value and bit coverage with two missing public-error assertions.
