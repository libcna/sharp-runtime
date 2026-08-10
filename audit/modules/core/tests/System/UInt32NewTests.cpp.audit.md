# Audit: `modules/core/tests/System/UInt32NewTests.cpp`

## Metadata

- Audit status: AUDITED (93 lines, 41 tests, full read).
- Validation: `UInt32NewTests.*` passed 41/41 in the focused Core.Base run on
  2026-07-25.

## Assessment

The suite closes an earlier overload gap and covers common arithmetic, bit
operations, rotations, count functions, and parse/format basics.  It confirms
malformed `X` precision maps to `System::FormatException`, but does not
exercise unsupported format letters or the invalid Clamp precondition.

## Finding references

- **SR-AUD-021:** no `ToString(value, "Q")` exception assertion exists.
- **SR-AUD-022:** only ordered Clamp bounds are covered.
- **SR-AUD-023:** no B/b format test exists despite the public .NET integral
  binary contract; the implementation silently returns decimal.

## Required post-audit verification

Add exact exception tests for `Q` and oversized precision, an
`ArgumentException` test for `Clamp(5, 10, 0)`, and B/b vectors for 5, zero,
width, and `MaxValue`.  Do not remove the existing malformed-width regression.

## Final assessment

Useful broad unsigned happy-path coverage, but the precise missing assertions
map directly to three confirmed cross-width public API defects.
