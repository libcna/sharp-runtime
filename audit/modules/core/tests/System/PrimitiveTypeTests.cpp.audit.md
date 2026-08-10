# Audit: `modules/core/tests/System/PrimitiveTypeTests.cpp`

## Metadata

- Audit status: AUDITED (276 lines, 84 tests, full read).
- Relevant validation: `Int32Tests.*` passed 63/63 and `UInt32Tests.*` passed
  9/9 in the 167-test focused Core.Base run on 2026-07-25.

## Assessment

This mixed primitive suite gives Int32 unusually broad public behavior
coverage: min/max parsing, `DivRem` exceptional cases, signed-minimum `Abs` and
`CopySign`, bit primitives, and basic B formatting.  It also supplies common
Int64 and UInt32 smoke coverage.  The tests are valuable regression evidence
for earlier hardening work rather than mere happy paths.

## Finding references

- **SR-AUD-021:** malformed precision is asserted, but unknown format types
  (`"Q"`) are not, so decimal fallback remains undetected.
- **SR-AUD-022:** every Clamp assertion uses an ordered interval; the public
  invalid-bound error path is absent.
- **SR-AUD-023:** unsigned UInt32 has no B/b assertion, unlike the signed
  Int32 B cases directly above it.

## Other missing assertions and diagnostics

- `Int32::ToString("B")` should be checked for `MinValue` and raw 32-bit width.
- UInt32 needs a full format matrix, including B/b, unknown types, and max
  width, rather than only its one-argument decimal conversion here.
- This file also owns basic Int64 coverage; its distinct 64-bit findings are
  reported in the Int64 mirrored reports to avoid duplicate issue IDs.

## Final assessment

Strong signed 32-bit regression coverage; it exposes a clear asymmetry where
the adjacent unsigned type lacks the binary-format tests that would reveal
SR-AUD-023.
