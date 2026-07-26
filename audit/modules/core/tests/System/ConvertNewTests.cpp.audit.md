# Audit: `modules/core/tests/System/ConvertNewTests.cpp`

## Metadata

- Audit status: AUDITED (282 lines, 81 tests, full read).
- Validation: `ConvertTests.*` passed 204/204, including the main companion
  file, on 2026-07-25.

## Assessment

This companion expands overload and midpoint coverage and correctly checks
several finite overflow paths. It omits negative signed inputs for
`ToUInt32`/`ToUInt64`, out-of-range long-to-byte, non-finite values, and
Base64 structure entirely.

## Finding references

- **SR-AUD-026:** unsigned conversions are tested only with non-negative
  values, and `ToByte(long)` has no invalid-range assertion.
- **SR-AUD-027:** finite negative and boundary doubles are covered, but NaN
  and infinity are absent.
- **SR-AUD-028:** this file adds hex regression coverage but no Base64 grammar
  coverage.

## Required post-audit verification

Add complementary signed-negative/range, non-finite, and Base64 grammar
vectors without weakening the existing midpoint-to-even expectations.

## Final assessment

Useful overload expansion, but it does not challenge the error paths confirmed
by the implementation audit.
