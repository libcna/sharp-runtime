# Audit: `modules/core/tests/System/BitConverterTests.cpp`

## Metadata

- Audit status: AUDITED (449 lines, 67 tests, full read).
- Validation: `BitConverterTests.*` passed 67/67 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

The suite has broad normal round-trip coverage across scalar, float, Half,
BFloat16, and 128-bit values, plus unusually good formatter boundary tests.
All typed vector conversions are exercised only with an exact-size valid buffer
at offset zero, so the shared bounds defect remains entirely invisible.

## Finding references

- **SR-AUD-041:** no vector `To*` test asserts the exception for a negative
  index, index at/beyond size, or fewer remaining bytes than the target width.
  The tested ordinary calls all pass while ASan independently demonstrates
  underflow and overflow reads for `ToInt32`.

## Required post-audit verification

Add exception tests for at least 1-, 2-, 4-, 8-, and 16-byte decoders at
negative index, end index, and short-buffer boundaries.  Run those tests under
ASan; ordinary exception assertions alone cannot prove that validation occurs
before pointer arithmetic.

## Final assessment

The test suite is strong for successful byte layout and the ToString repair,
but lacks the negative decoder coverage needed to prevent the confirmed
memory-safety regression.  No implementation was modified during this audit.
